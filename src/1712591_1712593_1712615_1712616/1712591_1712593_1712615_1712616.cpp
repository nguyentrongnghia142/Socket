#define _WINSOCK_DEPRECATED_NO_WARNINGS
#define _CRT_SECURE_NO_WARNINGS
#define DEFAULT_BUFLEN 1460
#define DEFAULT_PORT 8888 
#include<stdio.h>
#include<iostream>
#include<string>
#include<conio.h>
#include<winsock2.h>
#include<ws2tcpip.h>
#include<time.h>
#include<stdlib.h>
#include<Windows.h>
#include<strsafe.h>
#include<sstream>
#include<fstream>
#include<vector>
#include<io.h>
#pragma comment(lib, "Ws2_32.lib")
using namespace std;

int GetContent_Length(string a)
{
	int temp = 0, pos = a.find("Content-Length: ");
	if (pos == -1)
		return pos;
	pos += 16;
	while (a[pos] != '\r')
	{
		temp += a[pos] - '0';
		temp *= 10;
		pos++;
	}
	temp /= 10;
	return temp;
}

string find_host(string buffer)
{
	string host("Host: ");
	int found = buffer.find(host);
	int start = found + 6;
	string domain_name;
	for (int i = start; ; i++)
	{
		if (buffer[i] == '\r')
			break;
		domain_name += buffer[i];
	}
	return domain_name;
}

string find_referer(const string &buffer)
{
	int found = buffer.find("Referer: ");
	if (found < 0)
		return find_host(buffer);// n?u không có tru?ng Referer trong request thì tr? l?i chính host c?a request dó

	int start = found + 16;
	string referer;
	for (int i = start; ; i++)
	{
		if (buffer[i] == '/')
			break;
		referer += buffer[i];
	}
	return referer;
}

void send_403_forbidden_mess(const SOCKET &acceptSock)
{
	string text;
	stringstream stream;

	FILE *fp = fopen("403.html", "r");
	if (fp == NULL)
		return;
	fseek(fp, 0L, SEEK_END);
	stream << "HTTP/1.1 403 Forbidden\r\nContent-length: " << ftell(fp) << "\r\n";//ftell trả về số bytes của file html
	fseek(fp, 0L, SEEK_SET);// đưa con trỏ file về đầu file
	text = stream.str();
	send(acceptSock, text.c_str(), text.length(), 0);

	text = "Content-Type: text/html\r\n\r\n";
	send(acceptSock, text.c_str(), text.length(), 0);
	string html_buff;
	char c;
	while (!feof(fp))
	{
		fscanf(fp, "%c", &c);
		html_buff += c;
	}
	//send(acceptSock, html_buff.c_str(), html_buff.length(), 0);
	fclose(fp);
}

void send_404_notfound_mess(const SOCKET &acceptSock)
{
	string text;
	stringstream stream;

	FILE *fp = fopen("404.html", "r");
	if (fp == NULL)
		return;

	fseek(fp, 0L, SEEK_END);
	stream << "HTTP/1.1 404 Not Found\r\nContent-length: " << ftell(fp) << "\r\n";
	fseek(fp, 0L, SEEK_SET);
	text = stream.str();
	send(acceptSock, text.c_str(), text.length(), 0);

	text = "Content-Type: text/html\r\n\r\n";
	send(acceptSock, text.c_str(), text.length(), 0);
	string html_buff;
	char c;
	while (!feof(fp))
	{
		fscanf(fp, "%c", &c);
		html_buff += c;
	}
	send(acceptSock, html_buff.c_str(), html_buff.length(), 0);
	fclose(fp);
}

bool is_in_blacklist(const string &recvbuf)
{
	string domain_name = find_host(recvbuf);
	string referer = find_referer(recvbuf);

	ifstream fin("blacklist.conf");
	if (fin.fail()==true)
	{
		return false;
	}
	string black_domain;
	while (!fin.eof())
	{
		fin >> black_domain;
		if (domain_name == black_domain || referer == black_domain)
			return true;
	}
	fin.close();
	return false;
}

char* file_cache(char *buff)
{
	char *a = new char[strlen(buff) + 8];
	strcpy(a, "cache/");
	strcat(a, buff);
	int i = 5;
	while (a[++i] != '\r')
	{
		if (a[i] == '/') { a[i] = '0'; continue; }
		if (a[i] == 92) { a[i] = '1'; continue; }
		if (a[i] == '*') { a[i] = '2'; continue; }
		if (a[i] == ':') { a[i] = '3'; continue; }
		if (a[i] == '<') { a[i] = '4'; continue; }
		if (a[i] == '>') { a[i] = '5'; continue; }
		if (a[i] == '|') { a[i] = '6'; continue; }
		if (a[i] == '?') { a[i] = '7'; continue; }
		if (a[i] == '"') { a[i] = '8'; continue; }
	};
	a[i] = '\0';
	strcat(a, ".txt");
	return a;
}

int Cache(SOCKET &AcceptSocket, FILE *fin)
{
	// receive header
	char headerrecv[5000];// = { 0 };
	string header_res;
	int index = 0, end_header = 0;
	while (end_header < 4)
	{
		fread(&headerrecv[index], sizeof(headerrecv[index]), 1, fin);
		header_res.push_back(headerrecv[index]);
		if (headerrecv[index] == '\r' || headerrecv[index] == '\n')
			end_header++;
		else end_header = 0;
		index++;
	}
	cout << "Header received completely\n";
	headerrecv[index] = '\0';//

		// send the HEADER result buffer back to the client
	int sentBytes = send(AcceptSocket, headerrecv, index, 0);
	if (sentBytes == SOCKET_ERROR)
	{
		cout << "Send failed with error: " << WSAGetLastError() << endl;
		return 0;
	}
	cout << "Header sent to client completely\n";
	// receive and send body
	int contentlength = GetContent_Length(header_res);// find the header "Content-Lenght" (if have) and get the byte
	int	byterecv;
	char body_res[DEFAULT_BUFLEN];// = { 0 };
	do
	{
		byterecv = fread(&body_res, 1, DEFAULT_BUFLEN, fin);
		contentlength -= byterecv;
		if (byterecv > 0)
		{
			sentBytes = send(AcceptSocket, body_res, byterecv, 0);
			if (contentlength > 0)
				continue;
			if (byterecv < DEFAULT_BUFLEN)// received the last block
				break;
		}
		else if (byterecv == 0)// in this case : the last block is 1460 bytes full !!!
			break;
	} while (byterecv > 0);
	return 1;
}

WORD WINAPI MyThread(LPVOID a)
{
	// Make sure there is a console to receive output results. 
	HANDLE hStdout;
	hStdout = GetStdHandle(STD_OUTPUT_HANDLE);
	if (hStdout == INVALID_HANDLE_VALUE)
		return 1;

	//Declare
	SOCKET AcceptSocket = (SOCKET)a;
	int iSendResult;
	char recvbuf[5000];
	int recvbuflen = DEFAULT_BUFLEN;

	// Receive from clinet
	int byterecv = recv(AcceptSocket, recvbuf, recvbuflen, 0);
	if (byterecv > 0) {
		cout << "Bytes received from client: " << byterecv << endl;
	}
	else if (byterecv == 0) {
		cout << "Connection to client is closing...\n";
		return 1;
	}
	else {
		cout << "Receive failed with error: " << WSAGetLastError() << endl;
		return 1;
	}
	recvbuf[byterecv] = '\0';

	char *filename = file_cache(recvbuf);
	FILE *fin = fopen(filename, "rb");
	delete[] filename;
	if (fin != NULL)
	{
		if (Cache(AcceptSocket, fin))
		{
			fclose(fin);
			cout << "Body sent completely\n";
			cout << "==============\n";
			closesocket(AcceptSocket);
			return 1;
		}
	}
	
	// Get host-name then find the IP addr
	SOCKADDR_IN sockAddr;
	string domain_name = find_host(recvbuf);

	hostent *remoteHost = NULL;
	remoteHost = gethostbyname(domain_name.c_str());
	// check if this web exists
	if (remoteHost == NULL) {
		send_404_notfound_mess(AcceptSocket);
		closesocket(AcceptSocket);
		return 1;
	}
	// check if host is in black list
	if (is_in_blacklist(recvbuf) == true) {
		send_403_forbidden_mess(AcceptSocket);
		closesocket(AcceptSocket);//
		return 1;
	}

	in_addr addr;
	addr.s_addr = *(u_long *)remoteHost->h_addr_list[0];

	sockAddr.sin_addr = addr;// The IPv4 Address from the Address Resolution Result
	sockAddr.sin_family = AF_INET;  // IPv4
	sockAddr.sin_port = htons(80);  // HTTP Port: 80

	// Create a SOCKET for connecting to web server
	SOCKET ConnectSocket = INVALID_SOCKET;
	ConnectSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ConnectSocket == INVALID_SOCKET) {
		cout << "Socket failed with error: " << WSAGetLastError() << endl;
		closesocket(AcceptSocket);//
		return 1;
	}

	// Connect to web server.
	cout << "Connecting to webserver...\n";
	if (connect(ConnectSocket, (SOCKADDR*)&sockAddr, sizeof(sockAddr)) != 0)
	{
		cout << "Could not connect\n";
		closesocket(ConnectSocket);//
		closesocket(AcceptSocket);
		return 1;
	}
	cout << "Connected to web server.\n";

	// Send request to web server
	int sentBytes = send(ConnectSocket, recvbuf, byterecv, 0);
	if (sentBytes < byterecv || sentBytes == SOCKET_ERROR)
	{
		cout << "Could not send the request to the Server" << endl;
		closesocket(ConnectSocket);
		closesocket(AcceptSocket);//
		return 1;
	}
	printf("Bytes Sent to webserver: %d\n", byterecv);

	// Receive from webserver: recv header first, recv body after

		// receive header
	char *fileout = file_cache(recvbuf);
	FILE *fout = fopen(fileout, "wb");
	

	char headerrecv[5000];
	string header_res;
	int index = 0, end_header = 0;
	while (end_header < 4)
	{
		byterecv = recv(ConnectSocket, headerrecv + index, 1, 0);
		header_res.push_back(headerrecv[index]);
		if (fout != NULL)
			fwrite(headerrecv + index, sizeof(headerrecv[index]), 1, fout);

		cout << headerrecv[index]; //
		if (headerrecv[index] == '\r' || headerrecv[index] == '\n')
			end_header++;
		else end_header = 0;
		index++;
	}
	cout << "Header received completely\n";
	headerrecv[index] = '\0';

	//send the HEADER result buffer back to the client
	sentBytes = send(AcceptSocket, headerrecv, index, 0);
	if (sentBytes == SOCKET_ERROR)
	{
		cout << "Send failed with error: " << WSAGetLastError() << endl;
		if (fout != NULL)
			fclose(fout);
		if (!_access(fileout, 0))
			remove(fileout);
		delete[] fileout;
		closesocket(AcceptSocket);
		closesocket(ConnectSocket);//
		return 1;
	}
	cout << "Header sent to client completely\n";
	// receive and send body
	int contentlength = GetContent_Length(header_res);// find the header "Content-Lenght" (if have) and get the byte
	size_t k = -1;
	bool dk = (contentlength < DEFAULT_BUFLEN);
	char body_res[DEFAULT_BUFLEN + 1];
	do
	{
		byterecv = recv(ConnectSocket, body_res, DEFAULT_BUFLEN, 0);
		body_res[byterecv] = '\0';
		contentlength -= byterecv;
		if (byterecv > 0)
		{
			sentBytes = send(AcceptSocket, body_res, byterecv, 0);
			if (fout != NULL)
			{
				k = fwrite(body_res, 1, byterecv, fout);
			}
			if (contentlength > 0)
				continue;
			if (byterecv < DEFAULT_BUFLEN)// received the last block
				break;
		}
		else if (byterecv == 0)// in this case : the last block is 1460 bytes full !!!
			break;
	} while (byterecv > 0);


	if (fout != NULL)
		fclose(fout);
	
	if ((contentlength > 0) || k == -1 || dk)
	{
		if (!_access(fileout, 0))
			remove(fileout);
	}
	cout << "Body sent completely\n";

	delete[] fileout;
	closesocket(ConnectSocket);
	closesocket(AcceptSocket);
	return 0;
}


int main(int argc, char *argv)
{
	
	WSADATA wsaData;
	int iResult; 

	// Initialize Winsock
	iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
	if (iResult != 0)
	{
		cout << "WSAStartup failed with error: " << iResult << endl;
		_getch();
		return 1;
	}

	// create a socket for listening to client 
	SOCKET ListenSocket = INVALID_SOCKET;
	ListenSocket = socket(AF_INET, SOCK_STREAM, IPPROTO_TCP);
	if (ListenSocket == INVALID_SOCKET) {
		cout << "Socket failed with error: " << WSAGetLastError() << endl;
		WSACleanup();
		_getch();
		return 1;
	}

	// Bind listensocket
	sockaddr_in service;
	service.sin_family = AF_INET;
	service.sin_addr.s_addr = INADDR_ANY;//inet_aton("127.0.0.1");
	service.sin_port = htons(8888);
	if (bind(ListenSocket, (SOCKADDR *)& service, sizeof(service)) == SOCKET_ERROR)
	{
		printf("bind failed with error: %ld\n", WSAGetLastError());
		closesocket(ListenSocket);
		WSACleanup();
		_getch();
		return 1;
	}

	// listen
	if (listen(ListenSocket, SOMAXCONN) == SOCKET_ERROR)
	{
		cout << "Listen failed with error: " << WSAGetLastError() << endl;
		closesocket(ListenSocket);
		WSACleanup();
		_getch();
		return 1;
	}
	DWORD threadID;
	HANDLE threadStatus;
	while (1)
	{
		// Create a SOCKET for accepting incoming requests.
		SOCKET AcceptSocket = INVALID_SOCKET;
		cout << "Waiting for client to connect...\n";

		// Accept the connection if client want to request.
		AcceptSocket = accept(ListenSocket, NULL, NULL);
		if (AcceptSocket == INVALID_SOCKET)
		{
			cout << "Accept failed with error: " << WSAGetLastError() << endl;
			continue;
		}
		else
			cout << "Client connected.\n";

		//do sth
		threadStatus = CreateThread(NULL, 0, (LPTHREAD_START_ROUTINE)MyThread, (LPVOID)AcceptSocket, 0, &threadID);
	}
	closesocket(ListenSocket);
	WSACleanup();
	_getch();
	return 0;
}