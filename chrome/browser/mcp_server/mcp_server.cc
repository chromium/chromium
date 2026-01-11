// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server.h"

#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/task/bind_post_task.h"
#include "chrome/browser/mcp_server/dispatcher/dispatcher.h"
#include "chrome/browser/mcp_server/mcp_server_prefs.h"
#include "chrome/browser/mcp_server/tab_controller/tab_controller.h"
#include "components/prefs/pref_change_registrar.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"
#include "net/base/ip_address.h"
#include "net/base/ip_endpoint.h"
#include "net/base/net_errors.h"
#include "net/server/http_server.h"
#include "net/server/http_server_request_info.h"
#include "net/server/http_server_response_info.h"
#include "net/socket/tcp_server_socket.h"
#include "net/traffic_annotation/network_traffic_annotation.h"

namespace mcp_server {

namespace {

// Network traffic annotation for MCP Server
constexpr net::NetworkTrafficAnnotationTag kMCPServerTrafficAnnotation =
    net::DefineNetworkTrafficAnnotation("mcp_server", R"(
      semantics {
        sender: "MCP Server"
        description:
          "MCP (Model Control Protocol) Server provides localhost HTTP/WebSocket "
          "APIs for AI agent control of the browser. Only accessible from "
          "localhost (127.0.0.1)."
        trigger: "User enables MCP Server in chrome://settings/ai"
        data: "Browser state, tab information, DOM snapshots, console logs, "
              "network requests. No data leaves localhost."
        destination: LOCAL
      }
      policy {
        cookies_allowed: NO
        setting: "Users can enable/disable in chrome://settings/ai"
        policy_exception_justification:
          "Not implemented. This is a developer-only feature for local "
          "browser automation."
      })");

}  // namespace

// Internal implementation of MCPServer
class MCPServer::Impl : public net::HttpServer::Delegate {
 public:
  Impl() : pref_service_(nullptr), running_(false), port_(0) {
    // Create Dispatcher and TabController on UI thread
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MCPServer::Impl::InitializeOnUIThread,
                       base::Unretained(this)));
  }
  ~Impl() override {
    if (http_server_) {
      Stop();
    }
  }

  void InitializeOnUIThread() {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Create TabController on UI thread
    tab_controller_ = std::make_unique<TabController>();

    // Create Dispatcher on UI thread
    dispatcher_ = std::make_unique<Dispatcher>();
    dispatcher_->SetTabController(tab_controller_.get());
    dispatcher_->RegisterRoutes();

    LOG(INFO) << "Dispatcher and TabController initialized on UI thread";
  }

  // net::HttpServer::Delegate implementation
  void OnConnect(int connection_id) override {
    LOG(INFO) << "MCP Server: Client connected, connection_id=" << connection_id;
  }

  void OnHttpRequest(int connection_id,
                     const net::HttpServerRequestInfo& info) override {
    LOG(INFO) << "MCP Server: HTTP " << info.method << " " << info.path;

    // Route the request
    HandleHttpRequest(connection_id, info);
  }

  void OnWebSocketRequest(int connection_id,
                          const net::HttpServerRequestInfo& info) override {
    LOG(INFO) << "MCP Server: WebSocket upgrade request for " << info.path;

    // Accept WebSocket connection
    http_server_->AcceptWebSocket(connection_id, info,
                                   kMCPServerTrafficAnnotation);
  }

  void OnWebSocketMessage(int connection_id, std::string data) override {
    LOG(INFO) << "MCP Server: WebSocket message from connection_id="
              << connection_id << ", data=" << data;

    // TODO: Handle WebSocket messages (Week 2)
    // For now, echo back
    http_server_->SendOverWebSocket(connection_id, data,
                                     kMCPServerTrafficAnnotation);
  }

  void OnClose(int connection_id) override {
    LOG(INFO) << "MCP Server: Client disconnected, connection_id="
              << connection_id;
  }

  void SetPrefService(PrefService* pref_service) {
    pref_service_ = pref_service;

    if (!pref_service_) {
      return;
    }

    // Set up preference observer to auto-start/stop server
    // Note: Callback runs on UI thread (where PrefService lives)
    pref_change_registrar_ = std::make_unique<PrefChangeRegistrar>();
    pref_change_registrar_->Init(pref_service_);
    pref_change_registrar_->Add(
        kMcpServerEnabled,
        base::BindRepeating(&MCPServer::Impl::OnMcpServerEnabledChanged,
                            base::Unretained(this)));

    // Start server if preference is enabled (post to IO thread)
    if (IsEnabledInPrefs()) {
      // Read port from prefs NOW (on UI thread) before posting to IO thread
      int port = GetPortFromPrefs();
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MCPServer::Impl::StartAsync, base::Unretained(this), port));
    }
  }

  // Async version for posting to IO thread
  void StartAsync(int port) {
    Start(port);
  }

  // Async version for posting to IO thread
  void StopAsync() {
    Stop();
  }

  bool Start(int port) {
    // This runs on IO thread - port value must be passed in from UI thread
    // (cannot access PrefService from IO thread)

    if (running_) {
      LOG(WARNING) << "MCP Server already running on port " << port_;
      return false;
    }

    // Validate port range
    if (port < 1024 || port > 65535) {
      LOG(ERROR) << "Invalid port number: " << port;
      return false;
    }

    // Try to start server, with retry on port collision
    const int kMaxRetries = 5;
    for (int attempt = 0; attempt < kMaxRetries; ++attempt) {
      int current_port = port + attempt;

      if (StartServerOnPort(current_port)) {
        port_ = current_port;
        running_ = true;

        LOG(INFO) << "MCP Server started on localhost:" << port_;

        return true;
      }

      LOG(WARNING) << "Failed to start MCP Server on port " << current_port
                   << ", trying next port...";
    }

    LOG(ERROR) << "Failed to start MCP Server after " << kMaxRetries
               << " attempts starting from port " << port;
    return false;
  }

  void Stop() {
    if (!running_) {
      return;
    }

    LOG(INFO) << "MCP Server stopping on port " << port_;

    // Stop HTTP server
    http_server_.reset();

    running_ = false;
    port_ = 0;
  }

  bool IsRunning() const { return running_; }

  int GetPort() const { return port_; }

  bool IsEnabledInPrefs() const {
    if (!pref_service_) {
      return false;
    }
    return pref_service_->GetBoolean(kMcpServerEnabled);
  }

  void SetEnabledInPrefs(bool enabled) {
    if (!pref_service_) {
      LOG(WARNING) << "PrefService not set, cannot save enabled state";
      return;
    }
    pref_service_->SetBoolean(kMcpServerEnabled, enabled);
  }

 private:
  int GetPortFromPrefs() const {
    if (!pref_service_) {
      return 9224;  // Default port
    }
    return pref_service_->GetInteger(kMcpServerPort);
  }

  bool StartServerOnPort(int port) {
    // Create TCP server socket
    auto server_socket =
        std::make_unique<net::TCPServerSocket>(nullptr, net::NetLogSource());

    // Bind to localhost (127.0.0.1) only - IPv4
    net::IPAddress localhost = net::IPAddress::IPv4Localhost();
    net::IPEndPoint endpoint(localhost, port);

    int result = server_socket->Listen(endpoint, 5, /*ipv6_only=*/std::nullopt);
    if (result != net::OK) {
      LOG(WARNING) << "Failed to listen on port " << port << ": "
                   << net::ErrorToString(result);
      return false;
    }

    // Create HTTP server with this socket
    http_server_ = std::make_unique<net::HttpServer>(std::move(server_socket),
                                                      this);

    LOG(INFO) << "HTTP server listening on 127.0.0.1:" << port;
    return true;
  }

  void HandleHttpRequest(int connection_id,
                         const net::HttpServerRequestInfo& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    // Handle special endpoints on IO thread (don't need UI thread)
    if (info.path == "/" || info.path == "/health") {
      HandleSpecialEndpoint(connection_id, info);
      return;
    }

    // For all other endpoints, post to UI thread for Dispatcher handling
    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MCPServer::Impl::HandleRequestOnUIThread,
                       base::Unretained(this), connection_id, info.method,
                       info.path, info.data));
  }

  void HandleSpecialEndpoint(int connection_id,
                             const net::HttpServerRequestInfo& info) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    std::string response_body;
    net::HttpStatusCode status_code = net::HTTP_OK;

    if (info.path == "/") {
      // Root endpoint - server info
      response_body = R"({
        "name": "MCP Server",
        "version": "1.0.0",
        "status": "running",
        "endpoints": {
          "tabs": "/mcp/tabs",
          "websocket": "ws://127.0.0.1:)" +
                      base::NumberToString(port_) + R"(/ws"
        }
      })";
    } else if (info.path == "/health") {
      // Health check endpoint
      response_body = R"({"status": "ok", "uptime": "running"})";
    }

    // Send JSON response
    http_server_->Send(connection_id, status_code, response_body,
                       "application/json", kMCPServerTrafficAnnotation);
  }

  void HandleRequestOnUIThread(int connection_id,
                                const std::string& method,
                                const std::string& path,
                                const std::string& body) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::UI);

    // Use Dispatcher to route and handle the request
    Response response = dispatcher_->HandleRequest(method, path, body);

    // Convert response to JSON string
    std::string response_json;
    base::JSONWriter::Write(response.body, &response_json);

    // Post back to IO thread to send response
    content::GetIOThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(&MCPServer::Impl::SendResponseOnIOThread,
                       base::Unretained(this), connection_id,
                       response.status_code, response_json));
  }

  void SendResponseOnIOThread(int connection_id,
                               int status_code,
                               const std::string& response_body) {
    DCHECK_CURRENTLY_ON(content::BrowserThread::IO);

    if (!http_server_) {
      LOG(WARNING) << "HTTP server not available, cannot send response";
      return;
    }

    // Map our status codes to net::HttpStatusCode
    net::HttpStatusCode http_status =
        static_cast<net::HttpStatusCode>(status_code);

    http_server_->Send(connection_id, http_status, response_body,
                       "application/json", kMCPServerTrafficAnnotation);
  }

  void OnMcpServerEnabledChanged() {
    // This callback runs on UI thread (where PrefService lives)
    bool enabled = IsEnabledInPrefs();
    LOG(INFO) << "MCP Server preference changed, enabled=" << enabled;

    // Post actual start/stop to IO thread (where server lives)
    if (enabled) {
      // Read port from prefs NOW (on UI thread) before posting to IO thread
      int port = GetPortFromPrefs();
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MCPServer::Impl::StartAsync, base::Unretained(this), port));
    } else {
      // Stop server
      content::GetIOThreadTaskRunner({})->PostTask(
          FROM_HERE,
          base::BindOnce(&MCPServer::Impl::StopAsync, base::Unretained(this)));
    }
  }

  raw_ptr<PrefService> pref_service_;
  bool running_;
  int port_;

  // HTTP/WebSocket server instance (IO thread)
  std::unique_ptr<net::HttpServer> http_server_;

  // Preference change observer
  std::unique_ptr<PrefChangeRegistrar> pref_change_registrar_;

  // Dispatcher and TabController (UI thread)
  std::unique_ptr<Dispatcher> dispatcher_;
  std::unique_ptr<TabController> tab_controller_;
};

// Static
MCPServer* MCPServer::GetInstance() {
  static base::NoDestructor<MCPServer> instance;
  return instance.get();
}

MCPServer::MCPServer() : impl_(std::make_unique<Impl>()) {}

MCPServer::~MCPServer() = default;

void MCPServer::SetPrefService(PrefService* pref_service) {
  impl_->SetPrefService(pref_service);
}

bool MCPServer::Start(int port) {
  return impl_->Start(port);
}

void MCPServer::Stop() {
  impl_->Stop();
}

bool MCPServer::IsRunning() const {
  return impl_->IsRunning();
}

int MCPServer::GetPort() const {
  return impl_->GetPort();
}

bool MCPServer::IsEnabledInPrefs() const {
  return impl_->IsEnabledInPrefs();
}

void MCPServer::SetEnabledInPrefs(bool enabled) {
  impl_->SetEnabledInPrefs(enabled);
}

}  // namespace mcp_server
