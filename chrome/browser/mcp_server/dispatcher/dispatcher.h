// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_
#define CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_

#include <string>

namespace mcp_server {

// Dispatcher handles HTTP request routing and dispatching
// Routes incoming API requests to appropriate handlers
class Dispatcher {
 public:
  Dispatcher();
  ~Dispatcher();

  // Register API routes
  void RegisterRoutes();

  // Handle incoming HTTP request
  // Returns HTTP response as JSON string
  std::string HandleRequest(const std::string& method,
                            const std::string& path,
                            const std::string& body);

 private:
  // TODO: Add route handlers
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_
