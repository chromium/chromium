// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server.h"

#include "base/logging.h"
#include "base/no_destructor.h"

namespace mcp_server {

// Internal implementation of MCPServer
class MCPServer::Impl {
 public:
  Impl() : running_(false), port_(0) {}
  ~Impl() = default;

  bool Start(int port) {
    if (running_) {
      LOG(WARNING) << "MCP Server already running on port " << port_;
      return false;
    }

    port_ = port;
    running_ = true;

    LOG(INFO) << "MCP Server started on localhost:" << port_;
    // TODO: Initialize HTTP server
    // TODO: Initialize WebSocket server
    // TODO: Register API routes

    return true;
  }

  void Stop() {
    if (!running_) {
      return;
    }

    LOG(INFO) << "MCP Server stopping on port " << port_;

    // TODO: Stop HTTP server
    // TODO: Stop WebSocket server
    // TODO: Clean up connections

    running_ = false;
    port_ = 0;
  }

  bool IsRunning() const { return running_; }

  int GetPort() const { return port_; }

 private:
  bool running_;
  int port_;
};

// Static
MCPServer* MCPServer::GetInstance() {
  static base::NoDestructor<MCPServer> instance;
  return instance.get();
}

MCPServer::MCPServer() : impl_(std::make_unique<Impl>()) {}

MCPServer::~MCPServer() = default;

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

}  // namespace mcp_server
