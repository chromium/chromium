// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/execute_js_tool_request.h"

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/execute_js_tool_host.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

ExecuteJsToolRequest::ExecuteJsToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const base::UnguessableToken& target_document_id,
    const std::string& script_code,
    uint32_t timeout_ms)
    : TabToolRequest(actor_surface_handle),
      target_document_id_(target_document_id),
      script_code_(script_code),
      timeout_ms_(timeout_ms) {}

ExecuteJsToolRequest::ExecuteJsToolRequest(const ExecuteJsToolRequest&) =
    default;
ExecuteJsToolRequest& ExecuteJsToolRequest::operator=(
    const ExecuteJsToolRequest&) = default;
ExecuteJsToolRequest::ExecuteJsToolRequest(ExecuteJsToolRequest&&) = default;
ExecuteJsToolRequest& ExecuteJsToolRequest::operator=(ExecuteJsToolRequest&&) =
    default;

ExecuteJsToolRequest::~ExecuteJsToolRequest() = default;

std::string_view ExecuteJsToolRequest::Name() const {
  return kName;
}

void ExecuteJsToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult ExecuteJsToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurfaceHandle actor_surface_handle = GetActorSurfaceHandle();
  if (!actor_surface_handle.Get()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kTabWentAway,
                       /*requires_page_stabilization=*/false,
                       "The target surface is no longer present.")};
  }

  auto action = mojom::ExecuteJsAction::New(script_code_, timeout_ms_);
  return {std::make_unique<ExecuteJsToolHost>(
              task_id, tool_delegate, actor_surface_handle, target_document_id_,
              mojom::ToolAction::NewExecuteJs(std::move(action))),
          MakeOkResult()};
}

}  // namespace actor
