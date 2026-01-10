// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_MCP_SERVER_H_
#define CHROME_BROWSER_MCP_SERVER_MCP_SERVER_H_

#include <memory>
#include <string>

#include "base/memory/singleton.h"
#include "base/no_destructor.h"

namespace content {
class BrowserContext;
}  // namespace content

namespace mcp_server {

// MCPServer provides a local HTTP/WebSocket server for AI agent control.
// This server runs on localhost:9224 and provides APIs for:
// - Tab management (create, close, activate, list)
// - UI interactions (click, type, scroll)
// - DOM querying and HTML snapshots
// - Console log collection
// - Network request monitoring
//
// Security Model:
// - Localhost-only binding (no external network access)
// - Developer-only feature (disabled by default)
// - No authentication (relies on localhost binding)
// - Undetectable (no Navigator.webdriver or headless indicators)
//
// Usage:
//   MCPServer* server = MCPServer::GetInstance();
//   server->Start(9224);  // Start on port 9224
//   server->Stop();       // Stop the server
class MCPServer {
 public:
  // Get the singleton instance
  static MCPServer* GetInstance();

  MCPServer(const MCPServer&) = delete;
  MCPServer& operator=(const MCPServer&) = delete;

  // Start the MCP server on the specified port
  // Returns true if server started successfully
  bool Start(int port = 9224);

  // Stop the MCP server
  void Stop();

  // Check if the server is currently running
  bool IsRunning() const;

  // Get the current port the server is running on
  int GetPort() const;

 private:
  friend class base::NoDestructor<MCPServer>;

  MCPServer();
  ~MCPServer();

  // Internal implementation details
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_MCP_SERVER_H_
