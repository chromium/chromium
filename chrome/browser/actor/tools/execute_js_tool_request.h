// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_REQUEST_H_
#define CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_REQUEST_H_

#include <cstdint>
#include <memory>
#include <string>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/page_tool_request.h"
#include "chrome/common/actor/actor_constants.h"

namespace actor {

// Executes a dynamic JavaScript code string in the document, in the actor's
// isolated world.
class ExecuteJsToolRequest : public TabToolRequest {
 public:
  static constexpr char kName[] = "ExecuteJsTool";

  ExecuteJsToolRequest(ActorSurfaceHandle actor_surface_handle,
                       const base::UnguessableToken& target_document_id,
                       const std::string& script_code,
                       uint32_t timeout_ms = static_cast<uint32_t>(
                           kExecuteJsDefaultTimeout.InMilliseconds()));
  ExecuteJsToolRequest(const ExecuteJsToolRequest&);
  ExecuteJsToolRequest& operator=(const ExecuteJsToolRequest&);
  ExecuteJsToolRequest(ExecuteJsToolRequest&&);
  ExecuteJsToolRequest& operator=(ExecuteJsToolRequest&&);
  ~ExecuteJsToolRequest() override;

  // ToolRequest
  std::string_view Name() const override;
  void Apply(ToolRequestVisitorFunctor&) const override;
  ToolRequest::CreateToolResult CreateTool(
      TaskId task_id,
      ToolDelegate& tool_delegate) const override;

 private:
  base::UnguessableToken target_document_id_;
  std::string script_code_;
  uint32_t timeout_ms_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_EXECUTE_JS_TOOL_REQUEST_H_
