// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_
#define CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_

#include <map>
#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/values.h"

namespace mcp_server {

class TabController;

// HTTP request context
struct RequestContext {
  RequestContext();
  ~RequestContext();

  std::string method;        // GET, POST, DELETE, etc.
  std::string path;          // /mcp/tabs
  std::map<std::string, std::string> params;  // Path parameters (:id)
  base::Value::Dict body;    // Parsed JSON body (empty for GET)
};

// HTTP response
struct Response {
  Response();
  ~Response();

  // Move-only type (because base::Value::Dict is move-only)
  Response(Response&&);
  Response& operator=(Response&&);

  int status_code;           // 200, 404, 500, etc.
  base::Value::Dict body;    // JSON response body

  // Helper to create success response
  static Response Ok(base::Value::Dict data);

  // Helper to create error response
  static Response Error(int code, const std::string& message);
};

// Route handler function type
using RouteHandler = base::RepeatingCallback<Response(const RequestContext&)>;

// Dispatcher handles HTTP request routing and dispatching
// Routes incoming API requests to appropriate handlers
class Dispatcher {
 public:
  Dispatcher();
  ~Dispatcher();

  // Set tab controller for routing
  void SetTabController(TabController* tab_controller);

  // Register all API routes
  void RegisterRoutes();

  // Handle incoming HTTP request
  // Returns HTTP response as JSON string with status code
  Response HandleRequest(const std::string& method,
                        const std::string& path,
                        const std::string& body);

 private:
  // Register a route with handler
  void RegisterRoute(const std::string& method,
                     const std::string& path_pattern,
                     RouteHandler handler);

  // Find matching route and extract path parameters
  RouteHandler* FindRoute(const std::string& method,
                          const std::string& path,
                          std::map<std::string, std::string>* params);

  // Parse path pattern to check if it matches request path
  // Extracts parameters like :id from /mcp/tabs/:id
  bool MatchPath(const std::string& pattern,
                 const std::string& path,
                 std::map<std::string, std::string>* params);

  // Route handlers for tab management
  Response HandleListTabs(const RequestContext& ctx);
  Response HandleCreateTab(const RequestContext& ctx);
  Response HandleCloseTab(const RequestContext& ctx);
  Response HandleActivateTab(const RequestContext& ctx);
  Response HandleGetTabState(const RequestContext& ctx);

  // Route storage: "METHOD /path/pattern" -> handler
  std::map<std::string, RouteHandler> routes_;

  // Tab controller (not owned)
  raw_ptr<TabController> tab_controller_ = nullptr;
};

}  // namespace mcp_server

#endif  // CHROME_BROWSER_MCP_SERVER_DISPATCHER_DISPATCHER_H_
