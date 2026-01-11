// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/mcp_server.h"

#include "base/logging.h"
#include "base/memory/raw_ptr.h"
#include "base/no_destructor.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"

namespace mcp_server {

// Internal implementation of MCPServer
class MCPServer::Impl {
 public:
  Impl() : pref_service_(nullptr), running_(false), port_(0) {}
  ~Impl() = default;

  void SetPrefService(PrefService* pref_service) {
    pref_service_ = pref_service;
  }

  bool Start(int port) {
    if (running_) {
      LOG(WARNING) << "MCP Server already running on port " << port_;
      return false;
    }

    // If port is 0, use port from preferences
    if (port == 0) {
      port = GetPortFromPrefs();
    }

    // Validate port range
    if (port < 1024 || port > 65535) {
      LOG(ERROR) << "Invalid port number: " << port;
      return false;
    }

    port_ = port;
    running_ = true;

    LOG(INFO) << "MCP Server started on localhost:" << port_;

    // Save running state to preferences
    SaveStateToPrefs();

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

    // Save stopped state to preferences
    SaveStateToPrefs();
  }

  bool IsRunning() const { return running_; }

  int GetPort() const { return port_; }

  bool IsEnabledInPrefs() const {
    if (!pref_service_) {
      return false;
    }
    return pref_service_->GetBoolean(prefs::kMCPServerEnabled);
  }

  void SetEnabledInPrefs(bool enabled) {
    if (!pref_service_) {
      LOG(WARNING) << "PrefService not set, cannot save enabled state";
      return;
    }
    pref_service_->SetBoolean(prefs::kMCPServerEnabled, enabled);
  }

  void SaveStateToPrefs() {
    if (!pref_service_) {
      LOG(WARNING) << "PrefService not set, cannot save state";
      return;
    }

    pref_service_->SetBoolean(prefs::kMCPServerEnabled, running_);
    if (running_) {
      pref_service_->SetInteger(prefs::kMCPServerPort, port_);
    }
  }

 private:
  int GetPortFromPrefs() const {
    if (!pref_service_) {
      return 9224;  // Default port
    }
    return pref_service_->GetInteger(prefs::kMCPServerPort);
  }

  raw_ptr<PrefService> pref_service_;
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

void MCPServer::SaveStateToPrefs() {
  impl_->SaveStateToPrefs();
}

}  // namespace mcp_server
