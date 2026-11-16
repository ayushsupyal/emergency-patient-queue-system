/*
  Emergency Patient Queue System
  PBL Project - C++

  The patients are stored in a linked list. The list is kept in
  priority order, so the most urgent patient stays at the front.

  Priority:
  1 - Critical
  2 - Serious
  3 - Normal

  Compile on Windows:
  g++ -std=c++11 main.cpp -o emergency_queue_server.exe -lws2_32 -lshell32
*/

#include <iostream>
#include <fstream>
#include <sstream>
#include <cstring>
#include <cstdlib>
#include <cstdio>
#include <ctime>
#include <cctype>
#include <string>

#ifdef _WIN32
  #define WIN32_LEAN_AND_MEAN
  #include <winsock2.h>
  #include <ws2tcpip.h>
  #include <windows.h>
  #include <shellapi.h>
  #pragma comment(lib, "ws2_32.lib")
  typedef SOCKET socket_t;
  #define CLOSESOCKET closesocket
#else
  #include <sys/types.h>
  #include <sys/socket.h>
  #include <netinet/in.h>
  #include <arpa/inet.h>
  #include <unistd.h>
  typedef int socket_t;
  #define INVALID_SOCKET (-1)
  #define SOCKET_ERROR (-1)
  #define CLOSESOCKET close
#endif

using namespace std;

const int PORT = 8787;
const int REQUEST_CAP = 16384;
const int RESPONSE_CAP = 65536;
const char* QUEUE_FILE = "patients.txt";
const char* HISTORY_FILE = "treated_patients.txt";

struct Patient {
    int id;
    int age;
    int priority; // 1 critical, 2 serious, 3 normal
    long sequence;
    char name[80];
    char condition[160];
};

struct Node {
    Patient patient;
    Node* next;
};

class HospitalQueue {
private:
    Node* head;
    int nextId;
    long nextSequence;

    const char* priorityText(int p) const {
        if (p == 1) return "Critical";
        if (p == 2) return "Serious";
        return "Normal";
    }

    void insertSorted(const Patient& p) {
        Node* n = new Node;
        n->patient = p;
        n->next = NULL;

        if (head == NULL ||
            p.priority < head->patient.priority ||
            (p.priority == head->patient.priority &&
             p.sequence < head->patient.sequence)) {
            n->next = head;
            head = n;
            return;
        }

        Node* cur = head;
        while (cur->next != NULL &&
              (cur->next->patient.priority < p.priority ||
              (cur->next->patient.priority == p.priority &&
               cur->next->patient.sequence < p.sequence))) {
            cur = cur->next;
        }
        n->next = cur->next;
        cur->next = n;
    }

    void appendHistory(const Patient& p) {
        ofstream out(HISTORY_FILE, ios::app);
        if (!out) return;
        out << p.id << "|" << p.name << "|" << p.age << "|"
            << p.priority << "|" << p.condition << "|" << (long)time(NULL) << "\n";
    }

public:
    HospitalQueue() : head(NULL), nextId(1001), nextSequence(1) {
        load();
    }

    ~HospitalQueue() { clear(); }

    void clear() {
        while (head != NULL) {
            Node* t = head;
            head = head->next;
            delete t;
        }
    }

    void load() {
        ifstream in(QUEUE_FILE);
        if (!in) return;
        char line[600];
        while (in.getline(line, sizeof(line))) {
            Patient p;
            char* token = strtok(line, "|");
            if (!token) continue;
            p.id = atoi(token);
            token = strtok(NULL, "|"); if (!token) continue; p.name[0] = '\0'; strncpy(p.name, token, sizeof(p.name)-1); p.name[sizeof(p.name)-1] = '\0';
            token = strtok(NULL, "|"); if (!token) continue; p.age = atoi(token);
            token = strtok(NULL, "|"); if (!token) continue; p.priority = atoi(token);
            token = strtok(NULL, "|"); if (!token) continue; strncpy(p.condition, token, sizeof(p.condition)-1); p.condition[sizeof(p.condition)-1] = '\0';
            token = strtok(NULL, "|"); if (!token) continue; p.sequence = atol(token);
            if (p.priority < 1 || p.priority > 3) continue;
            insertSorted(p);
            if (p.id >= nextId) nextId = p.id + 1;
            if (p.sequence >= nextSequence) nextSequence = p.sequence + 1;
        }
    }

    void save() const {
        ofstream out(QUEUE_FILE);
        Node* cur = head;
        while (cur) {
            const Patient& p = cur->patient;
            out << p.id << "|" << p.name << "|" << p.age << "|"
                << p.priority << "|" << p.condition << "|" << p.sequence << "\n";
            cur = cur->next;
        }
    }

    int count() const {
        int n = 0;
        Node* cur = head;
        while (cur) { ++n; cur = cur->next; }
        return n;
    }

    bool add(const char* name, int age, int priority, const char* condition, Patient& created) {
        if (!name || !*name || age < 0 || age > 120 || priority < 1 || priority > 3) return false;
        created.id = nextId++;
        created.age = age;
        created.priority = priority;
        created.sequence = nextSequence++;
        strncpy(created.name, name, sizeof(created.name)-1);
        created.name[sizeof(created.name)-1] = '\0';
        strncpy(created.condition, (condition && *condition) ? condition : "Not specified", sizeof(created.condition)-1);
        created.condition[sizeof(created.condition)-1] = '\0';
        // The | symbol is used to separate fields in the file.
        if (strchr(created.name, '|') || strchr(created.condition, '|') ||
            strchr(created.name, '\n') || strchr(created.condition, '\n')) return false;
        insertSorted(created);
        save();
        return true;
    }

    bool removeNext(Patient& removed) {
        if (!head) return false;
        Node* t = head;
        removed = t->patient;
        head = head->next;
        delete t;
        appendHistory(removed);
        save();
        return true;
    }

    bool removeById(int id, Patient& removed) {
        Node* cur = head;
        Node* prev = NULL;
        while (cur && cur->patient.id != id) { prev = cur; cur = cur->next; }
        if (!cur) return false;
        if (prev) prev->next = cur->next; else head = cur->next;
        removed = cur->patient;
        delete cur;
        save();
        return true;
    }

    bool find(int id, Patient& found) const {
        Node* cur = head;
        while (cur) {
            if (cur->patient.id == id) { found = cur->patient; return true; }
            cur = cur->next;
        }
        return false;
    }

    Node* getHead() const { return head; }

    void stats(int& critical, int& serious, int& normal) const {
        critical = serious = normal = 0;
        Node* cur = head;
        while (cur) {
            if (cur->patient.priority == 1) ++critical;
            else if (cur->patient.priority == 2) ++serious;
            else ++normal;
            cur = cur->next;
        }
    }

    const char* priorityName(int p) const { return priorityText(p); }
};

static void jsonEscape(const char* src, char* dst, size_t cap) {
    size_t j = 0;
    if (!src) src = "";
    for (size_t i = 0; src[i] && j + 2 < cap; ++i) {
        unsigned char c = (unsigned char)src[i];
        if (c == '"' || c == '\\') { dst[j++] = '\\'; dst[j++] = (char)c; }
        else if (c == '\n') { dst[j++] = '\\'; dst[j++] = 'n'; }
        else if (c == '\r') { dst[j++] = '\\'; dst[j++] = 'r'; }
        else if (c < 32) { dst[j++] = ' '; }
        else dst[j++] = (char)c;
    }
    dst[j] = '\0';
}

static bool getJsonString(const char* body, const char* key, char* out, size_t cap) {
    char pattern[100];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(body, pattern);
    if (!p) return false;
    p = strchr(p, ':'); if (!p) return false;
    ++p; while (*p && isspace((unsigned char)*p)) ++p;
    if (*p != '"') return false;
    ++p;
    size_t j = 0;
    while (*p && *p != '"' && j + 1 < cap) {
        if (*p == '\\' && p[1]) ++p;
        out[j++] = *p++;
    }
    out[j] = '\0';
    return true;
}

static int getJsonInt(const char* body, const char* key, int fallback) {
    char pattern[100];
    snprintf(pattern, sizeof(pattern), "\"%s\"", key);
    const char* p = strstr(body, pattern);
    if (!p) return fallback;
    p = strchr(p, ':'); if (!p) return fallback;
    ++p; while (*p && isspace((unsigned char)*p)) ++p;
    return atoi(p);
}

static void sendAll(socket_t s, const char* data, int length) {
    int sent = 0;
    while (sent < length) {
        int n = send(s, data + sent, length - sent, 0);
        if (n <= 0) break;
        sent += n;
    }
}

static void sendResponse(socket_t client, const char* status, const char* type, const char* body) {
    char header[512];
    int bodyLen = (int)strlen(body);
    snprintf(header, sizeof(header),
        "HTTP/1.1 %s\r\nContent-Type: %s\r\nContent-Length: %d\r\n"
        "Access-Control-Allow-Origin: *\r\nAccess-Control-Allow-Methods: GET, POST, DELETE, OPTIONS\r\n"
        "Access-Control-Allow-Headers: Content-Type\r\nConnection: close\r\nCache-Control: no-store\r\n\r\n",
        status, type, bodyLen);
    sendAll(client, header, (int)strlen(header));
    sendAll(client, body, bodyLen);
}

static void patientJson(const Patient& p, char* out, size_t cap) {
    char name[200], condition[300];
    jsonEscape(p.name, name, sizeof(name));
    jsonEscape(p.condition, condition, sizeof(condition));
    snprintf(out, cap,
        "{\"id\":%d,\"name\":\"%s\",\"age\":%d,\"priority\":%d,\"priorityName\":\"%s\",\"condition\":\"%s\"}",
        p.id, name, p.age, p.priority,
        p.priority == 1 ? "Critical" : (p.priority == 2 ? "Serious" : "Normal"),
        condition);
}

static void handleApi(socket_t client, const char* method, const char* path, const char* body, HospitalQueue& q) {
    char response[RESPONSE_CAP];
    response[0] = '\0';

    if (strcmp(method, "OPTIONS") == 0) {
        sendResponse(client, "204 No Content", "text/plain", "");
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/patients") == 0) {
        strcat(response, "{\"patients\":[");
        Node* cur = q.getHead();
        bool first = true;
        while (cur) {
            char item[800];
            patientJson(cur->patient, item, sizeof(item));
            if (!first) strcat(response, ",");
            if (strlen(response) + strlen(item) + 100 >= sizeof(response)) break;
            strcat(response, item);
            first = false;
            cur = cur->next;
        }
        int c, s, n; q.stats(c, s, n);
        char tail[200];
        snprintf(tail, sizeof(tail), "],\"total\":%d,\"critical\":%d,\"serious\":%d,\"normal\":%d}", q.count(), c, s, n);
        strcat(response, tail);
        sendResponse(client, "200 OK", "application/json; charset=utf-8", response);
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/patients") == 0) {
        char name[80], condition[160];
        int age = getJsonInt(body, "age", -1);
        int priority = getJsonInt(body, "priority", -1);
        if (!getJsonString(body, "name", name, sizeof(name))) name[0] = '\0';
        if (!getJsonString(body, "condition", condition, sizeof(condition))) strcpy(condition, "Not specified");
        Patient p;
        if (q.add(name, age, priority, condition, p)) {
            char item[800];
            patientJson(p, item, sizeof(item));
            snprintf(response, sizeof(response), "{\"success\":true,\"message\":\"Patient registered\",\"patient\":%s}", item);
            sendResponse(client, "201 Created", "application/json; charset=utf-8", response);
        } else {
            sendResponse(client, "400 Bad Request", "application/json", "{\"success\":false,\"message\":\"Invalid details. Check name, age, priority and avoid | characters.\"}");
        }
        return;
    }

    if (strcmp(method, "POST") == 0 && strcmp(path, "/api/call-next") == 0) {
        Patient p;
        if (q.removeNext(p)) {
            char item[800]; patientJson(p, item, sizeof(item));
            snprintf(response, sizeof(response), "{\"success\":true,\"patient\":%s}", item);
            sendResponse(client, "200 OK", "application/json; charset=utf-8", response);
        } else {
            sendResponse(client, "404 Not Found", "application/json", "{\"success\":false,\"message\":\"The waiting queue is empty.\"}");
        }
        return;
    }

    if (strcmp(method, "GET") == 0 && strncmp(path, "/api/patients/", 14) == 0) {
        int id = atoi(path + 14);
        Patient p;
        if (q.find(id, p)) {
            char item[800]; patientJson(p, item, sizeof(item));
            snprintf(response, sizeof(response), "{\"success\":true,\"patient\":%s}", item);
            sendResponse(client, "200 OK", "application/json; charset=utf-8", response);
        } else {
            sendResponse(client, "404 Not Found", "application/json", "{\"success\":false,\"message\":\"Patient not found in the waiting queue.\"}");
        }
        return;
    }

    if (strcmp(method, "DELETE") == 0 && strncmp(path, "/api/patients/", 14) == 0) {
        int id = atoi(path + 14);
        Patient p;
        if (q.removeById(id, p)) {
            sendResponse(client, "200 OK", "application/json", "{\"success\":true,\"message\":\"Patient removed from waiting queue.\"}");
        } else {
            sendResponse(client, "404 Not Found", "application/json", "{\"success\":false,\"message\":\"Patient not found.\"}");
        }
        return;
    }

    if (strcmp(method, "GET") == 0 && strcmp(path, "/api/history") == 0) {
        ifstream in(HISTORY_FILE);
        strcat(response, "{\"history\":[");
        char line[600]; bool first = true;
        while (in.getline(line, sizeof(line))) {
            char copy[600]; strncpy(copy, line, sizeof(copy)-1); copy[sizeof(copy)-1] = '\0';
            char* parts[6]; int count = 0;
            char* tok = strtok(copy, "|");
            while (tok && count < 6) { parts[count++] = tok; tok = strtok(NULL, "|"); }
            if (count < 6) continue;
            char name[200], condition[300]; jsonEscape(parts[1], name, sizeof(name)); jsonEscape(parts[4], condition, sizeof(condition));
            char item[900];
            snprintf(item, sizeof(item), "%s{\"id\":%s,\"name\":\"%s\",\"age\":%s,\"priority\":%s,\"condition\":\"%s\",\"treatedAt\":%s}",
                first ? "" : ",", parts[0], name, parts[2], parts[3], condition, parts[5]);
            if (strlen(response) + strlen(item) + 5 >= sizeof(response)) break;
            strcat(response, item); first = false;
        }
        strcat(response, "]}");
        sendResponse(client, "200 OK", "application/json; charset=utf-8", response);
        return;
    }

    sendResponse(client, "404 Not Found", "application/json", "{\"success\":false,\"message\":\"API endpoint not found.\"}");
}

static bool readRequest(socket_t client, char* method, size_t methodCap, char* path, size_t pathCap, char* body, size_t bodyCap) {
    char request[REQUEST_CAP];
    int used = 0;
    while (used < REQUEST_CAP - 1) {
        int n = recv(client, request + used, REQUEST_CAP - 1 - used, 0);
        if (n <= 0) break;
        used += n;
        request[used] = '\0';
        if (strstr(request, "\r\n\r\n")) break;
    }
    if (used <= 0) return false;
    request[used] = '\0';

    char* lineEnd = strstr(request, "\r\n");
    if (!lineEnd) return false;
    *lineEnd = '\0';
    char version[32];
    if (sscanf(request, "%15s %255s %31s", method, path, version) != 3) return false;

    char* headerEnd = strstr(lineEnd + 2, "\r\n\r\n");
    if (!headerEnd) { body[0] = '\0'; return true; }
    int contentLength = 0;
    char* cl = strstr(lineEnd + 2, "Content-Length:");
    if (cl && cl < headerEnd) contentLength = atoi(cl + 15);
    char* bodyStart = headerEnd + 4;
    int already = used - (int)(bodyStart - request);
    while (already < contentLength && used < REQUEST_CAP - 1) {
        int n = recv(client, request + used, REQUEST_CAP - 1 - used, 0);
        if (n <= 0) break;
        used += n; already += n;
    }
    if (contentLength >= (int)bodyCap) contentLength = (int)bodyCap - 1;
    if (contentLength > already) contentLength = already;
    if (contentLength > 0) memcpy(body, bodyStart, contentLength);
    body[contentLength] = '\0';
    return true;
}

static void serveStatic(socket_t client, const char* path) {
    char filePath[512];
    if (strcmp(path, "/") == 0) strcpy(filePath, "web/index.html");
    else if (strncmp(path, "/web/", 5) == 0) snprintf(filePath, sizeof(filePath), "%s", path + 1);
    else { sendResponse(client, "404 Not Found", "text/plain", "Not found"); return; }

    const char* ext = strrchr(filePath, '.');
    const char* type = "text/plain";
    if (ext && strcmp(ext, ".html") == 0) type = "text/html; charset=utf-8";
    else if (ext && strcmp(ext, ".css") == 0) type = "text/css; charset=utf-8";
    else if (ext && strcmp(ext, ".js") == 0) type = "application/javascript; charset=utf-8";

    ifstream in(filePath, ios::binary);
    if (!in) { sendResponse(client, "404 Not Found", "text/plain", "Frontend file not found. Run from the project folder."); return; }
    ostringstream stream; stream << in.rdbuf();
    string content = stream.str();
    sendResponse(client, "200 OK", type, content.c_str());
}

int main() {
#ifdef _WIN32
    WSADATA wsa;
    if (WSAStartup(MAKEWORD(2, 2), &wsa) != 0) {
        cerr << "Winsock startup failed.\n";
        return 1;
    }
#endif

    socket_t server = socket(AF_INET, SOCK_STREAM, 0);
    if (server == INVALID_SOCKET) {
        cerr << "Could not create server socket.\n";
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    int reuse = 1;
#ifdef _WIN32
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, (const char*)&reuse, sizeof(reuse));
#else
    setsockopt(server, SOL_SOCKET, SO_REUSEADDR, &reuse, sizeof(reuse));
#endif

    sockaddr_in address;
    memset(&address, 0, sizeof(address));
    address.sin_family = AF_INET;
    address.sin_addr.s_addr = htonl(INADDR_LOOPBACK); // Run the server only on this computer.
    address.sin_port = htons(PORT);

    if (bind(server, (sockaddr*)&address, sizeof(address)) == SOCKET_ERROR || listen(server, 8) == SOCKET_ERROR) {
        cerr << "Could not start local server on port " << PORT << ". Is it already in use?\n";
        CLOSESOCKET(server);
#ifdef _WIN32
        WSACleanup();
#endif
        return 1;
    }

    cout << "Emergency Patient Queue System backend is running.\n";
    cout << "Open http://127.0.0.1:" << PORT << " in your browser.\n";
    cout << "Keep this window open while using the dashboard. Press Ctrl+C to stop.\n";

#ifdef _WIN32
    ShellExecuteA(NULL, "open", ("http://127.0.0.1:" + to_string(PORT)).c_str(), NULL, NULL, SW_SHOWNORMAL);
#endif

    HospitalQueue queue;
    while (true) {
        sockaddr_in clientAddress;
#ifdef _WIN32
        int addrLen = sizeof(clientAddress);
#else
        socklen_t addrLen = sizeof(clientAddress);
#endif
        socket_t client = accept(server, (sockaddr*)&clientAddress, &addrLen);
        if (client == INVALID_SOCKET) continue;

        char method[16] = {0}, path[256] = {0}, body[REQUEST_CAP] = {0};
        if (readRequest(client, method, sizeof(method), path, sizeof(path), body, sizeof(body))) {
            if (strncmp(path, "/api/", 5) == 0) handleApi(client, method, path, body, queue);
            else if (strcmp(method, "GET") == 0) serveStatic(client, path);
            else sendResponse(client, "405 Method Not Allowed", "text/plain", "Method not allowed");
        }
        CLOSESOCKET(client);
    }

    CLOSESOCKET(server);
#ifdef _WIN32
    WSACleanup();
#endif
    return 0;
}
