// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_request.h"

#include "chrome/browser/actor/tools/script_tool_host.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "chrome/common/actor/actor_constants.h"

namespace actor {

using ::tabs::TabHandle;

ScriptToolRequest::ScriptToolRequest(
    tabs::TabHandle tab_handle,
    const base::UnguessableToken& target_document_id,
    const std::string& name,
    const std::string& input_arguments)
    : TabToolRequest(tab_handle),
      target_document_id_(target_document_id),
      name_(name),
      input_arguments_(input_arguments) {}

ScriptToolRequest::~ScriptToolRequest() = default;

std::string_view ScriptToolRequest::Name() const {
  return kName;
}

void ScriptToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

ToolRequest::CreateToolResult ScriptToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (!GetTabHandle().Get()) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  auto script = mojom::ScriptToolAction::New(name_, input_arguments_);
  return {std::make_unique<ScriptToolHost>(
              task_id, tool_delegate, GetTabHandle(), target_document_id_,
              mojom::ToolAction::NewScriptTool(std::move(script))),
          MakeOkResult()};
}

}  // namespace actor
