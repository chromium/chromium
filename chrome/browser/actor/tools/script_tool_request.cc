// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/script_tool_request.h"

#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/actor_constants.h"

namespace actor {

using ::tabs::TabHandle;

ScriptToolRequest::ScriptToolRequest(tabs::TabHandle tab_handle,
                                     const DomNode& target,
                                     const std::string& name,
                                     const std::string& input_arguments)
    : PageToolRequest(tab_handle, target),
      name_(name),
      input_arguments_(input_arguments) {
  // Script tools target the Document and are not bound to any specific
  // DOM node.
  CHECK_EQ(target.node_id, kRootElementDomNodeId);
}

ScriptToolRequest::~ScriptToolRequest() = default;

std::string_view ScriptToolRequest::Name() const {
  return kName;
}

void ScriptToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

mojom::ToolActionPtr ScriptToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto script = mojom::ScriptToolAction::New(name_, input_arguments_);
  return mojom::ToolAction::NewScriptTool(std::move(script));
}

std::unique_ptr<PageToolRequest> ScriptToolRequest::Clone() const {
  return std::make_unique<ScriptToolRequest>(*this);
}

}  // namespace actor
