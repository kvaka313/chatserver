#undef UNICODE

#define WIN32_LEAN_AND_MEAN

#include <iostream>
#include <thread>
#include <windows.h>
#include <winsock2.h>
#include <ws2tcpip.h>
#include <stdlib.h>
#include <stdio.h>
#include <map>
#include <string>
#include <vector>

#pragma comment (lib, "Ws2_32.lib")

#define DEFAULT_PORT "7777"

std::vector<std::string> split(const std::string& str, const std::string& delim)
{
    std::vector<std::string> tokens;
    size_t prev = 0, pos = 0;
    do
    {
        pos = str.find(delim, prev);
        if (pos == std::string::npos) pos = str.length();
        std::string token = str.substr(prev, pos-prev);
        if (!token.empty()) tokens.push_back(token);
        prev = pos + delim.length();
    }
    while (pos < str.length() && prev < str.length());
    return tokens;
}

std::string get_command_from_string(std::string* message){
    std::string delim = ":";
    return split(*message, delim)[0];
};

std::string get_data_from_string(std::string* message) {
    std::string delim = ":";
    std::vector<std::string> parse_result = split(*message, delim);
    if(parse_result.size() != 2) {
        return "";
    }
    return parse_result[1];
}

void handle_client(SOCKET* client_socket, std::map<std::string, SOCKET*>* clients) {
    char recvbuf[512] = {0};
    int recvbuflen = 512;
    int iResult= 0;
    iResult = recv(*client_socket, recvbuf, recvbuflen, MSG_WAITALL);


    std::string message(recvbuf);
    std::string command = get_command_from_string(&message);
    std::string data = get_data_from_string(&message);
    if (command == "name") {
        clients->insert(std::map<std::string, SOCKET *>::value_type(data, client_socket));
        return;
    }
    if (command == "broadcast") {
        for (std::map<std::string, SOCKET *>::iterator it = clients->begin(); it != clients->end(); ++it) {
            int iSendResult = send(*(it->second), data.c_str(), iResult, 0);
            if (iSendResult == SOCKET_ERROR) {
                printf("send failed with error: %d\n", WSAGetLastError());
                closesocket(*(it->second));
            }
        }
        return;
    }
    if (command == "list") {
        std::string list;
        for (std::map<std::string, SOCKET *>::iterator it = clients->begin(); it != clients->end(); ++it) {
            list.append(it->first);
            list.append("\n");
        }
        int iSendResult = send(*client_socket, list.c_str(), iResult, 0);
        if (iSendResult == SOCKET_ERROR) {
            printf("send failed with error: %d\n", WSAGetLastError());
            closesocket(*client_socket);
        }
        return;
    }

    int iSendResult = send(*(clients->at(command)), data.c_str(), iResult, 0);
    if (iSendResult == SOCKET_ERROR) {
        printf("send failed with error: %d\n", WSAGetLastError());
        closesocket(*client_socket);
    }
    return;
}

int main() {
    std::map<std::string,SOCKET*>* clients = new std::map<std::string, SOCKET*>();
    WSADATA wsaData;
    int iResult = WSAStartup(MAKEWORD(2, 2), &wsaData);
    if (iResult != 0) {
        printf("WSAStartup failed with error: %d\n", iResult);
        return 1;
    }

    struct addrinfo *result = NULL;
    struct addrinfo hints;
    ZeroMemory(&hints, sizeof(hints));
    hints.ai_family = AF_INET;
    hints.ai_socktype = SOCK_STREAM;
    hints.ai_protocol = IPPROTO_TCP;
    hints.ai_flags = AI_PASSIVE;

    iResult = getaddrinfo(NULL, DEFAULT_PORT, &hints, &result);
    if (iResult != 0) {
        printf("getaddrinfo failed with error: %d\n", iResult);
        WSACleanup();
        return 1;
    }

    SOCKET ListenSocket = INVALID_SOCKET;
    ListenSocket = socket(result->ai_family, result->ai_socktype, result->ai_protocol);
    if (ListenSocket == INVALID_SOCKET) {
        printf("socket failed with error: %ld\n", WSAGetLastError());
        freeaddrinfo(result);
        WSACleanup();
        return 1;
    }

    iResult = bind(ListenSocket, result->ai_addr, (int) result->ai_addrlen);
    if (iResult == SOCKET_ERROR) {
        printf("bind failed with error: %d\n", WSAGetLastError());
        freeaddrinfo(result);
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    freeaddrinfo(result);

    iResult = listen(ListenSocket, SOMAXCONN);
    if (iResult == SOCKET_ERROR) {
        printf("listen failed with error: %d\n", WSAGetLastError());
        closesocket(ListenSocket);
        WSACleanup();
        return 1;
    }

    while (true) {
        SOCKET ClientSocket = INVALID_SOCKET;
        ClientSocket = accept(ListenSocket, NULL, NULL);
        if (ClientSocket == INVALID_SOCKET) {
            printf("accept failed with error: %d\n", WSAGetLastError());
            closesocket(ListenSocket);
            WSACleanup();
            return 1;
        }
        std::thread t(handle_client, &ClientSocket, clients);

    }
    return 0;
}
