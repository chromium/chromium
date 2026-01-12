// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/mcp_server/dispatcher/dispatcher.h"

#include "base/json/json_reader.h"
#include "base/json/json_writer.h"
#include "base/logging.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"
#include "chrome/browser/mcp_server/accessibility_snapshot/accessibility_snapshot.h"
#include "chrome/browser/mcp_server/action_runner/action_runner.h"
#include "chrome/browser/mcp_server/tab_controller/tab_controller.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "content/public/browser/web_contents.h"

namespace mcp_server {

// RequestContext implementation
RequestContext::RequestContext() = default;
RequestContext::~RequestContext() = default;

// Response implementation
Response::Response() : status_code(200) {}
Response::~Response() = default;
Response::Response(Response&&) = default;
Response& Response::operator=(Response&&) = default;

// Response helper implementations
Response Response::Ok(base::Value::Dict data) {
  Response response;
  response.status_code = 200;
  response.body = std::move(data);
  return response;
}

Response Response::Error(int code, const std::string& message) {
  Response response;
  response.status_code = code;
  response.body.Set("error", message);
  response.body.Set("code", code);
  return response;
}

// Dispatcher implementation
Dispatcher::Dispatcher() {
  LOG(INFO) << "Dispatcher initialized";
}

Dispatcher::~Dispatcher() = default;

void Dispatcher::SetTabController(TabController* tab_controller) {
  tab_controller_ = tab_controller;
}

void Dispatcher::SetActionRunner(ActionRunner* action_runner) {
  action_runner_ = action_runner;
}

void Dispatcher::RegisterRoutes() {
  // Tab management routes
  RegisterRoute("GET", "/mcp/tabs",
                base::BindRepeating(&Dispatcher::HandleListTabs,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs",
                base::BindRepeating(&Dispatcher::HandleCreateTab,
                                    base::Unretained(this)));
  RegisterRoute("DELETE", "/mcp/tabs/:id",
                base::BindRepeating(&Dispatcher::HandleCloseTab,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/activate",
                base::BindRepeating(&Dispatcher::HandleActivateTab,
                                    base::Unretained(this)));
  RegisterRoute("GET", "/mcp/tabs/:id/state",
                base::BindRepeating(&Dispatcher::HandleGetTabState,
                                    base::Unretained(this)));

  // Action routes
  RegisterRoute("POST", "/mcp/tabs/:id/click",
                base::BindRepeating(&Dispatcher::HandleClickAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/type",
                base::BindRepeating(&Dispatcher::HandleTypeAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/hover",
                base::BindRepeating(&Dispatcher::HandleHoverAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/select",
                base::BindRepeating(&Dispatcher::HandleSelectAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/wait",
                base::BindRepeating(&Dispatcher::HandleWaitAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/evaluate",
                base::BindRepeating(&Dispatcher::HandleEvaluateAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/screenshot",
                base::BindRepeating(&Dispatcher::HandleScreenshotAction,
                                    base::Unretained(this)));

  // Accessibility route
  RegisterRoute("GET", "/mcp/tabs/:id/accessibility",
                base::BindRepeating(&Dispatcher::HandleAccessibilitySnapshot,
                                    base::Unretained(this)));

  // Ref-based action routes
  RegisterRoute("POST", "/mcp/tabs/:id/click-ref",
                base::BindRepeating(&Dispatcher::HandleClickByRefAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/type-ref",
                base::BindRepeating(&Dispatcher::HandleTypeByRefAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/hover-ref",
                base::BindRepeating(&Dispatcher::HandleHoverByRefAction,
                                    base::Unretained(this)));
  RegisterRoute("POST", "/mcp/tabs/:id/select-ref",
                base::BindRepeating(&Dispatcher::HandleSelectByRefAction,
                                    base::Unretained(this)));

  // Scroll action route
  RegisterRoute("POST", "/mcp/tabs/:id/scroll",
                base::BindRepeating(&Dispatcher::HandleScrollAction,
                                    base::Unretained(this)));

  LOG(INFO) << "Registered " << routes_.size() << " routes";
}

Response Dispatcher::HandleRequest(const std::string& method,
                                    const std::string& path,
                                    const std::string& body) {
  LOG(INFO) << "Handling request: " << method << " " << path;

  // Find matching route and extract path parameters
  std::map<std::string, std::string> params;
  RouteHandler* handler = FindRoute(method, path, &params);

  if (!handler) {
    LOG(WARNING) << "No route found for: " << method << " " << path;
    return Response::Error(404, "Route not found");
  }

  // Parse JSON body (if present)
  base::Value::Dict body_dict;
  if (!body.empty()) {
    auto parsed = base::JSONReader::ReadDict(body, base::JSON_PARSE_RFC);
    if (!parsed.has_value()) {
      LOG(ERROR) << "Failed to parse JSON body: " << body;
      return Response::Error(400, "Invalid JSON in request body");
    }
    body_dict = std::move(parsed.value());
  }

  // Build request context
  RequestContext ctx;
  ctx.method = method;
  ctx.path = path;
  ctx.params = std::move(params);
  ctx.body = std::move(body_dict);

  // Call the handler
  return handler->Run(ctx);
}

void Dispatcher::RegisterRoute(const std::string& method,
                                const std::string& path_pattern,
                                RouteHandler handler) {
  std::string key = method + " " + path_pattern;
  routes_[key] = std::move(handler);
  LOG(INFO) << "Registered route: " << key;
}

RouteHandler* Dispatcher::FindRoute(
    const std::string& method,
    const std::string& path,
    std::map<std::string, std::string>* params) {
  // Try each registered route pattern
  for (auto& [route_key, handler] : routes_) {
    // Extract method and pattern from key "METHOD /path/pattern"
    size_t space_pos = route_key.find(' ');
    if (space_pos == std::string::npos) {
      continue;
    }

    std::string route_method = route_key.substr(0, space_pos);
    std::string route_pattern = route_key.substr(space_pos + 1);

    // Check if method matches
    if (route_method != method) {
      continue;
    }

    // Check if path matches pattern
    if (MatchPath(route_pattern, path, params)) {
      return &handler;
    }
  }

  return nullptr;
}

bool Dispatcher::MatchPath(const std::string& pattern,
                            const std::string& path,
                            std::map<std::string, std::string>* params) {
  // Split both pattern and path by '/'
  std::vector<std::string> pattern_parts = base::SplitString(
      pattern, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  std::vector<std::string> path_parts = base::SplitString(
      path, "/", base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  // Must have same number of parts
  if (pattern_parts.size() != path_parts.size()) {
    return false;
  }

  // Match each part
  params->clear();
  for (size_t i = 0; i < pattern_parts.size(); ++i) {
    const std::string& pattern_part = pattern_parts[i];
    const std::string& path_part = path_parts[i];

    // Check if this is a parameter (starts with :)
    if (base::StartsWith(pattern_part, ":")) {
      // Extract parameter name (remove leading ':')
      std::string param_name = pattern_part.substr(1);
      (*params)[param_name] = path_part;
    } else {
      // Exact match required
      if (pattern_part != path_part) {
        return false;
      }
    }
  }

  return true;
}

// Route handlers for tab management

Response Dispatcher::HandleListTabs(const RequestContext& ctx) {
  if (!tab_controller_) {
    LOG(ERROR) << "Tab controller not set";
    return Response::Error(500, "Tab controller not available");
  }

  // TODO: Tab controller will return base::Value::Dict instead of string
  std::string tabs_json = tab_controller_->ListTabs();

  // For now, parse the string response and wrap it
  auto parsed = base::JSONReader::ReadDict(tabs_json, base::JSON_PARSE_RFC);
  if (!parsed.has_value()) {
    LOG(ERROR) << "Failed to parse tab controller response";
    return Response::Error(500, "Internal server error");
  }

  return Response::Ok(std::move(parsed.value()));
}

Response Dispatcher::HandleCreateTab(const RequestContext& ctx) {
  if (!tab_controller_) {
    LOG(ERROR) << "Tab controller not set";
    return Response::Error(500, "Tab controller not available");
  }

  // Extract URL from request body
  const std::string* url = ctx.body.FindString("url");
  if (!url) {
    return Response::Error(400, "Missing required field: url");
  }

  // TODO: Tab controller will return base::Value::Dict instead of string
  std::string result = tab_controller_->CreateTab(*url);

  auto parsed = base::JSONReader::ReadDict(result, base::JSON_PARSE_RFC);
  if (!parsed.has_value()) {
    LOG(ERROR) << "Failed to parse tab controller response";
    return Response::Error(500, "Internal server error");
  }

  return Response::Ok(std::move(parsed.value()));
}

Response Dispatcher::HandleCloseTab(const RequestContext& ctx) {
  if (!tab_controller_) {
    LOG(ERROR) << "Tab controller not set";
    return Response::Error(500, "Tab controller not available");
  }

  // Extract tab ID from path parameters
  auto it = ctx.params.find("id");
  if (it == ctx.params.end()) {
    return Response::Error(400, "Missing required parameter: id");
  }

  // Parse tab ID as 64-bit integer (tab IDs are pointer values)
  int64_t tab_id;
  if (!base::StringToInt64(it->second, &tab_id)) {
    return Response::Error(400, "Invalid tab ID format");
  }

  bool success = tab_controller_->CloseTab(tab_id);
  if (!success) {
    return Response::Error(404, "Tab not found");
  }

  base::Value::Dict result;
  result.Set("success", true);
  result.Set("tab_id", static_cast<double>(tab_id));
  return Response::Ok(std::move(result));
}

Response Dispatcher::HandleActivateTab(const RequestContext& ctx) {
  if (!tab_controller_) {
    LOG(ERROR) << "Tab controller not set";
    return Response::Error(500, "Tab controller not available");
  }

  // Extract tab ID from path parameters
  auto it = ctx.params.find("id");
  if (it == ctx.params.end()) {
    return Response::Error(400, "Missing required parameter: id");
  }

  // Parse tab ID as 64-bit integer (tab IDs are pointer values)
  int64_t tab_id;
  if (!base::StringToInt64(it->second, &tab_id)) {
    return Response::Error(400, "Invalid tab ID format");
  }

  bool success = tab_controller_->ActivateTab(tab_id);
  if (!success) {
    return Response::Error(404, "Tab not found");
  }

  base::Value::Dict result;
  result.Set("success", true);
  result.Set("tab_id", static_cast<double>(tab_id));
  return Response::Ok(std::move(result));
}

Response Dispatcher::HandleGetTabState(const RequestContext& ctx) {
  if (!tab_controller_) {
    LOG(ERROR) << "Tab controller not set";
    return Response::Error(500, "Tab controller not available");
  }

  // Extract tab ID from path parameters
  auto it = ctx.params.find("id");
  if (it == ctx.params.end()) {
    return Response::Error(400, "Missing required parameter: id");
  }

  // Parse tab ID as 64-bit integer (tab IDs are pointer values)
  int64_t tab_id;
  if (!base::StringToInt64(it->second, &tab_id)) {
    return Response::Error(400, "Invalid tab ID format");
  }

  // TODO: Tab controller will return base::Value::Dict instead of string
  std::string state_json = tab_controller_->GetTabState(tab_id);

  auto parsed = base::JSONReader::ReadDict(state_json, base::JSON_PARSE_RFC);
  if (!parsed.has_value()) {
    LOG(ERROR) << "Failed to parse tab controller response";
    return Response::Error(500, "Internal server error");
  }

  return Response::Ok(std::move(parsed.value()));
}

// Helper: Get WebContents from tab ID in params
content::WebContents* Dispatcher::GetWebContentsFromParams(
    const std::map<std::string, std::string>& params) {
  auto it = params.find("id");
  if (it == params.end()) {
    return nullptr;
  }

  int64_t tab_id = 0;
  if (!base::StringToInt64(it->second, &tab_id)) {
    return nullptr;
  }

  // Find WebContents by tab ID (pointer value)
  for (Browser* browser : *BrowserList::GetInstance()) {
    TabStripModel* tab_strip = browser->tab_strip_model();
    for (int i = 0; i < tab_strip->count(); ++i) {
      content::WebContents* web_contents = tab_strip->GetWebContentsAt(i);
      if (reinterpret_cast<intptr_t>(web_contents) == static_cast<intptr_t>(tab_id)) {
        return web_contents;
      }
    }
  }

  return nullptr;
}

// Action Handlers

Response Dispatcher::HandleClickAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* selector = ctx.body.FindString("selector");
  if (!selector) {
    return Response::Error(400, "Missing required field: selector");
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Click(
      web_contents, *selector,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleTypeAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* selector = ctx.body.FindString("selector");
  const std::string* text = ctx.body.FindString("text");

  if (!selector) {
    return Response::Error(400, "Missing required field: selector");
  }
  if (!text) {
    return Response::Error(400, "Missing required field: text");
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Type(
      web_contents, *selector, *text,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleHoverAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* selector = ctx.body.FindString("selector");
  if (!selector) {
    return Response::Error(400, "Missing required field: selector");
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Hover(
      web_contents, *selector,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleSelectAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* selector = ctx.body.FindString("selector");
  const std::string* value = ctx.body.FindString("value");

  if (!selector) {
    return Response::Error(400, "Missing required field: selector");
  }
  if (!value) {
    return Response::Error(400, "Missing required field: value");
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->SelectOption(
      web_contents, *selector, *value,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleWaitAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* selector = ctx.body.FindString("selector");
  if (!selector) {
    return Response::Error(400, "Missing required field: selector");
  }

  // Optional timeout (default 30 seconds)
  int timeout_ms = ctx.body.FindInt("timeout_ms").value_or(30000);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->WaitForSelector(
      web_contents, *selector, timeout_ms,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleEvaluateAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* js_code = ctx.body.FindString("code");
  if (!js_code) {
    return Response::Error(400, "Missing required field: code");
  }

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Evaluate(
      web_contents, *js_code,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleScreenshotAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  // Optional full_page parameter (default false)
  bool full_page = ctx.body.FindBool("full_page").value_or(false);

  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Screenshot(
      web_contents, full_page,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleAccessibilitySnapshot(const RequestContext& ctx) {
  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  // Use RunLoop to wait for async snapshot
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  AccessibilitySnapshot::TakeSnapshot(
      web_contents,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool snapshot_success,
             const std::string& snapshot_error, base::Value::Dict snapshot_data) {
            *out_success = snapshot_success;
            *out_error = snapshot_error;
            *out_data = std::move(snapshot_data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

// ===== Ref-based action handlers =====

Response Dispatcher::HandleClickByRefAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* ref_id = ctx.body.FindString("ref");
  if (!ref_id) {
    return Response::Error(400, "Missing required field: ref");
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->ClickByRef(
      web_contents, *ref_id,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleTypeByRefAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* ref_id = ctx.body.FindString("ref");
  const std::string* text = ctx.body.FindString("text");

  if (!ref_id) {
    return Response::Error(400, "Missing required field: ref");
  }
  if (!text) {
    return Response::Error(400, "Missing required field: text");
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->TypeByRef(
      web_contents, *ref_id, *text,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleHoverByRefAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* ref_id = ctx.body.FindString("ref");
  if (!ref_id) {
    return Response::Error(400, "Missing required field: ref");
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->HoverByRef(
      web_contents, *ref_id,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

Response Dispatcher::HandleSelectByRefAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  const std::string* ref_id = ctx.body.FindString("ref");
  const std::string* value = ctx.body.FindString("value");

  if (!ref_id) {
    return Response::Error(400, "Missing required field: ref");
  }
  if (!value) {
    return Response::Error(400, "Missing required field: value");
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->SelectOptionByRef(
      web_contents, *ref_id, *value,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

// ===== Scroll action handler =====

Response Dispatcher::HandleScrollAction(const RequestContext& ctx) {
  if (!action_runner_) {
    return Response::Error(500, "Action runner not initialized");
  }

  content::WebContents* web_contents = GetWebContentsFromParams(ctx.params);
  if (!web_contents) {
    return Response::Error(404, "Tab not found");
  }

  // Get scroll parameters
  const std::string* mode = ctx.body.FindString("mode");
  if (!mode) {
    return Response::Error(400, "Missing required field: mode");
  }

  // Default values
  int x = 0;
  int y = 0;
  std::string selector = "";
  std::string behavior = "auto";

  // Extract optional parameters
  std::optional<int> x_val = ctx.body.FindInt("x");
  if (x_val.has_value()) {
    x = x_val.value();
  }
  std::optional<int> y_val = ctx.body.FindInt("y");
  if (y_val.has_value()) {
    y = y_val.value();
  }
  if (const std::string* sel = ctx.body.FindString("selector")) {
    selector = *sel;
  }
  if (const std::string* behav = ctx.body.FindString("behavior")) {
    behavior = *behav;
  }

  // Use RunLoop to wait for async callback
  base::RunLoop run_loop(base::RunLoop::Type::kNestableTasksAllowed);
  bool success = false;
  std::string error_message;
  base::Value::Dict result_data;

  action_runner_->Scroll(
      web_contents, *mode, x, y, selector, behavior,
      base::BindOnce(
          [](base::RunLoop* loop, bool* out_success, std::string* out_error,
             base::Value::Dict* out_data, bool success,
             const std::string& error, base::Value::Dict data) {
            *out_success = success;
            *out_error = error;
            *out_data = std::move(data);
            loop->Quit();
          },
          &run_loop, &success, &error_message, &result_data));

  run_loop.Run();

  if (!success) {
    return Response::Error(500, error_message);
  }

  return Response::Ok(std::move(result_data));
}

}  // namespace mcp_server
