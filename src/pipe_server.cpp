/*****************************************************************************
 * file  : pipe_server.cpp
 * date  : 2019-06-04
 * author: luhx
 *
 * fun   : 使用完成例程命名管道服务器
 *
 * 
*******************************************************************************/

#include "stdafx.h"

#include <windows.h> 
#include <stdio.h>
#include <tchar.h>
#include <strsafe.h>
#include <thread>

#define PIPE_TIMEOUT 5000
#define BUFSIZE 4096

#define PIPE_NAME_READ "\\\\.\\pipe\\mynamedpipe_read"
#define PIPE_NAME_WRITE "\\\\.\\pipe\\mynamedpipe_write"

typedef struct
{
	OVERLAPPED oOverlap;
	HANDLE hPipeInst;
	TCHAR chRequest[BUFSIZE];
	DWORD cbRead;
	TCHAR chReply[BUFSIZE];
	DWORD cbToWrite;
} PIPEINST, *LPPIPEINST;

VOID DisconnectAndClose(LPPIPEINST);
BOOL ConnectToNewClient(HANDLE, LPOVERLAPPED);
VOID GetAnswerToRequest(LPPIPEINST);

BOOL CreateAndConnectPipInstance(const char* pipName, HANDLE &hPipInst, LPOVERLAPPED);
VOID WINAPI DoReadRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED);

VOID WINAPI DoWriteRoutine(DWORD, DWORD, LPOVERLAPPED);
VOID WINAPI CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED);


void PipeRecvThread()
{
	HANDLE hConnectEvent, hPipeInst;
	OVERLAPPED oConnect;
	LPPIPEINST lpPipeInst;
	DWORD dwWait, cbRet;
	BOOL fSuccess, fPendingIO;

	// Create one event object for the connect operation. 

	hConnectEvent = CreateEvent(
		NULL,    // default security attribute
		TRUE,    // manual reset event 
		TRUE,    // initial state = signaled 
		NULL);   // unnamed event object 

	if (hConnectEvent == NULL)
	{
		printf("CreateEvent failed with %d.\n", GetLastError());
		return ;
	}

	oConnect.hEvent = hConnectEvent;

	// Call a subroutine to create one instance, and wait for 
	// the client to connect. 

	fPendingIO = CreateAndConnectPipInstance(PIPE_NAME_READ, hPipeInst, &oConnect);

	while (1)
	{
		// Wait for a client to connect, or for a read or write 
		// operation to be completed, which causes a completion 
		// routine to be queued for execution. 

		dwWait = WaitForSingleObjectEx(
			hConnectEvent,  // event object to wait for 
			INFINITE,       // waits indefinitely 
			TRUE);          // alertable wait enabled 

		switch (dwWait)
		{
				// The wait conditions are satisfied by a completed connect 
				// operation. 
			case WAIT_OBJECT_0:
				// If an operation is pending, get the result of the 
				// connect operation. 
				printf("new client \n");
				// Allocate storage for this instance. 

				lpPipeInst = (LPPIPEINST)GlobalAlloc(
					GPTR, sizeof(PIPEINST));
				if (lpPipeInst == NULL)
				{
					printf("GlobalAlloc failed (%d)\n", GetLastError());
					return ;
				}
				lpPipeInst->hPipeInst = hPipeInst;

				if (fPendingIO)
				{
					fSuccess = GetOverlappedResult(
						hPipeInst,     // pipe handle 
						&oConnect, // OVERLAPPED structure 
						&cbRet,    // bytes transferred 
						FALSE);    // does not wait 
					if (!fSuccess)
					{
						printf("ConnectNamedPipe (%d)\n", GetLastError());
						return;
					}
				}

				// Start the read operation for this client. 
				// Note that this same routine is later used as a 
				// completion routine after a write operation. 
				lpPipeInst->cbToWrite = 0;
				DoReadRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

				// Create new pipe instance for the next client. 
				fPendingIO = CreateAndConnectPipInstance(PIPE_NAME_READ, hPipeInst, &oConnect);
				break;

				// The wait is satisfied by a completed read or write 
				// operation. This allows the system to execute the 
				// completion routine. 

			case WAIT_IO_COMPLETION:
				printf("WAIT_IO_COMPLETION\n");
				break;

				// An error occurred in the wait function. 

			default:
			{
				printf("WaitForSingleObjectEx (%d)\n", GetLastError());
				return ;
			}
		}
	}

}

void PipeWriteThread()
{
	HANDLE hConnectEvent, hPipInst;
	OVERLAPPED oConnect;
	LPPIPEINST lpPipeInst;
	DWORD dwWait, cbRet;
	BOOL fSuccess, fPendingIO;

	// Create one event object for the connect operation. 

	hConnectEvent = CreateEvent(
		NULL,    // default security attribute
		TRUE,    // manual reset event 
		TRUE,    // initial state = signaled 
		NULL);   // unnamed event object 

	if (hConnectEvent == NULL)
	{
		printf("CreateEvent failed with %d.\n", GetLastError());
		return;
	}

	oConnect.hEvent = hConnectEvent;

	// Call a subroutine to create one instance, and wait for 
	// the client to connect. 

	fPendingIO = CreateAndConnectPipInstance(PIPE_NAME_WRITE, hPipInst, &oConnect);

	while (1)
	{
		// Wait for a client to connect, or for a read or write 
		// operation to be completed, which causes a completion 
		// routine to be queued for execution. 

		dwWait = WaitForSingleObjectEx(
			hConnectEvent,  // event object to wait for 
			INFINITE,       // waits indefinitely 
			TRUE);          // alertable wait enabled 

		switch (dwWait)
		{
			// The wait conditions are satisfied by a completed connect 
			// operation. 
		case WAIT_OBJECT_0:
			// If an operation is pending, get the result of the 
			// connect operation. 

			if (fPendingIO)
			{
				fSuccess = GetOverlappedResult(
					hPipInst,     // pipe handle 
					&oConnect, // OVERLAPPED structure 
					&cbRet,    // bytes transferred 
					FALSE);    // does not wait 
				if (!fSuccess)
				{
					printf("ConnectNamedPipe (%d)\n", GetLastError());
					return;
				}
			}

			// Allocate storage for this instance. 

			lpPipeInst = (LPPIPEINST)GlobalAlloc(
				GPTR, sizeof(PIPEINST));
			if (lpPipeInst == NULL)
			{
				printf("GlobalAlloc failed (%d)\n", GetLastError());
				return;
			}

			lpPipeInst->hPipeInst = hPipInst;

			// Start the read operation for this client. 
			// Note that this same routine is later used as a 
			// completion routine after a write operation. 

			lpPipeInst->cbToWrite = 0;
			DoWriteRoutine(0, 0, (LPOVERLAPPED)lpPipeInst);

			// Create new pipe instance for the next client. 

			fPendingIO = CreateAndConnectPipInstance(PIPE_NAME_WRITE, hPipInst, &oConnect);
			break;

			// The wait is satisfied by a completed read or write 
			// operation. This allows the system to execute the 
			// completion routine. 

		case WAIT_IO_COMPLETION:
			printf("WAIT_IO_COMPLETION\n");
			break;

			// An error occurred in the wait function. 

		default:
		{
			printf("WaitForSingleObjectEx (%d)\n", GetLastError());
			return;
		}
		}
	}

}



// CompletedWriteRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as a completion routine after writing to 
// the pipe, or when a new client has connected to a pipe instance.
// It starts another read operation. 

VOID WINAPI DoReadRoutine(DWORD dwErr, DWORD cbWritten,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The write operation has finished, so read the next request (if 
	// there is no error). 

	if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
		fRead = ReadFileEx(
			lpPipeInst->hPipeInst,
			lpPipeInst->chRequest,
			BUFSIZE * sizeof(TCHAR),
			(LPOVERLAPPED)lpPipeInst,
			(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedReadRoutine);

	// Disconnect if an error occurred. 

	if (!fRead)
		DisconnectAndClose(lpPipeInst);
}

VOID WINAPI DoWriteRoutine(DWORD dwErr, DWORD cbWritten,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fRead = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The write operation has finished, so read the next request (if 
	// there is no error). 

	// if ((dwErr == 0) && (cbWritten == lpPipeInst->cbToWrite))
// 这儿的发送数据在适当的时候发送，为主动发送
// 	fRead = WriteFileEx(
// 		lpPipeInst->hPipeInst,
// 		lpPipeInst->chRequest,
// 		BUFSIZE * sizeof(TCHAR),
// 		(LPOVERLAPPED)lpPipeInst,
// 		(LPOVERLAPPED_COMPLETION_ROUTINE)CompletedWriteRoutine);
// 
// 	// Disconnect if an error occurred. 
// 
// 	if (!fRead)
// 		DisconnectAndClose(lpPipeInst);
}

// CompletedReadRoutine(DWORD, DWORD, LPOVERLAPPED) 
// This routine is called as an I/O completion routine after reading 
// a request from the client. It gets data and writes it to the pipe. 

VOID WINAPI CompletedReadRoutine(DWORD dwErr, DWORD cbBytesRead,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The read operation has finished, so write a response (if no 
	// error occurred). 

	if ((dwErr == 0) && (cbBytesRead != 0))
	{
		GetAnswerToRequest(lpPipeInst);
		DoReadRoutine(0, lpPipeInst->cbToWrite, (LPOVERLAPPED)lpPipeInst);
// TODO: 临时去除
// 		fWrite = WriteFileEx(
// 			lpPipeInst->hPipeInst,
// 			lpPipeInst->chReply,
// 			lpPipeInst->cbToWrite,
// 			(LPOVERLAPPED)lpPipeInst,
// 			(LPOVERLAPPED_COMPLETION_ROUTINE)DoReadRoutine);
	}

	// Disconnect if an error occurred. 
	
// TODO: 临时去除
// 	if (!fWrite)
// 		DisconnectAndClose(lpPipeInst);
}

VOID WINAPI CompletedWriteRoutine(DWORD dwErr, DWORD cbBytesRead,
	LPOVERLAPPED lpOverLap)
{
	LPPIPEINST lpPipeInst;
	BOOL fWrite = FALSE;

	// lpOverlap points to storage for this instance. 

	lpPipeInst = (LPPIPEINST)lpOverLap;

	// The read operation has finished, so write a response (if no 
	// error occurred). 

// 	if ((dwErr == 0) && (cbBytesRead != 0))
// 	{
// 		GetAnswerToRequest(lpPipeInst);
// 		DoReadRoutine(0, lpPipeInst->cbToWrite, (LPOVERLAPPED)lpPipeInst);
	// TODO: 临时去除
// 	fWrite = WriteFileEx(
// 		lpPipeInst->hPipeInst,
// 		lpPipeInst->chReply,
// 		lpPipeInst->cbToWrite,
// 		(LPOVERLAPPED)lpPipeInst,
// 		(LPOVERLAPPED_COMPLETION_ROUTINE)DoWriteRoutine);
//	}

	// Disconnect if an error occurred. 

	// TODO: 临时去除
// 	if (!fWrite)
// 		DisconnectAndClose(lpPipeInst);
}

// DisconnectAndClose(LPPIPEINST) 
// This routine is called when an error occurs or the client closes 
// its handle to the pipe. 

VOID DisconnectAndClose(LPPIPEINST lpPipeInst)
{
	// Disconnect the pipe instance. 

	if (!DisconnectNamedPipe(lpPipeInst->hPipeInst))
	{
		printf("DisconnectNamedPipe failed with %d.\n", GetLastError());
	}

	// Close the handle to the pipe instance. 

	CloseHandle(lpPipeInst->hPipeInst);

	// Release the storage for the pipe instance. 

	if (lpPipeInst != NULL)
		GlobalFree(lpPipeInst);
}

// CreateAndConnectInstance(LPOVERLAPPED) 
// This function creates a pipe instance and connects to the client. 
// It returns TRUE if the connect operation is pending, and FALSE if 
// the connection has been completed. 

BOOL CreateAndConnectPipInstance(const char* pipName, HANDLE &hPipeInst, LPOVERLAPPED lpoOverlap)
{
	hPipeInst = CreateNamedPipe(
		pipName,             // pipe name 
		PIPE_ACCESS_DUPLEX |      // read/write access 
		FILE_FLAG_OVERLAPPED,     // overlapped mode 
		PIPE_TYPE_MESSAGE |       // message-type pipe 
		PIPE_READMODE_MESSAGE |   // message read mode 
		PIPE_WAIT,                // blocking mode 
		PIPE_UNLIMITED_INSTANCES, // unlimited instances 
		BUFSIZE * sizeof(TCHAR),    // output buffer size 
		BUFSIZE * sizeof(TCHAR),    // input buffer size 
		PIPE_TIMEOUT,             // client time-out 
		NULL);                    // default security attributes
	if (hPipeInst == INVALID_HANDLE_VALUE)
	{
		printf("CreateNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	// Call a subroutine to connect to the new client. 
	return ConnectToNewClient(hPipeInst, lpoOverlap);
}

BOOL ConnectToNewClient(HANDLE hPipeInst, LPOVERLAPPED lpo)
{
	BOOL fConnected, fPendingIO = FALSE;

	// Start an overlapped connection for this pipe instance. 
	fConnected = ConnectNamedPipe(hPipeInst, lpo);

	// Overlapped ConnectNamedPipe should return zero. 
	if (fConnected)
	{
		printf("ConnectNamedPipe failed with %d.\n", GetLastError());
		return 0;
	}

	switch (GetLastError())
	{
		// The overlapped connection in progress. 
		case ERROR_IO_PENDING:
			fPendingIO = TRUE;
			break;

			// Client is already connected, so signal an event. 

		case ERROR_PIPE_CONNECTED:
			if (SetEvent(lpo->hEvent))
				break;

			// If an error occurs during the connect operation... 
		default:
		{
			printf("ConnectNamedPipe failed with %d.\n", GetLastError());
			return 0;
		}
	}
	return fPendingIO;
}

VOID GetAnswerToRequest(LPPIPEINST pipe)
{
	_tprintf(TEXT("[0x%x]Recv: %s\n"), (unsigned int)pipe->hPipeInst, pipe->chRequest);
	StringCchCopy(pipe->chReply, BUFSIZE, TEXT("Default answer from server"));
	pipe->cbToWrite = (lstrlen(pipe->chReply) + 1) * sizeof(TCHAR);
}





int _tmain(VOID)
{
	std::thread* t1 = new std::thread(&PipeRecvThread);
	std::thread* t2 = new std::thread(&PipeWriteThread);
	while (true)
	{
		std::this_thread::sleep_for(std::chrono::milliseconds(100));
	}
	return 0;
}