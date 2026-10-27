// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/move_mouse_tool_request.h"

#include <optional>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

namespace {

// Default description for the `move_mouse` tool.
constexpr std::string_view kMoveMouseToolDescription =
    "Move the mouse pointer over a specific element on the active webpage.";

// Description for the `dom_node_id` parameter of the `move_mouse` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The numeric DOM node ID of the target element to move over.";

}  // namespace


MoveMouseToolRequest::MoveMouseToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& target)
    : PageToolRequest(actor_surface_handle, target) {}

MoveMouseToolRequest::~MoveMouseToolRequest() = default;

// static
std::optional<ToolDefinition> MoveMouseToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kMoveMouse, kModelFacingName,
                               kMoveMouseToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kDomNodeIdParam, kDomNodeIdParamDescription))
      .Build();
}

void MoveMouseToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view MoveMouseToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr MoveMouseToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto move_mouse = mojom::MouseMoveAction::New();
  return mojom::ToolAction::NewMouseMove(std::move(move_mouse));
}

std::unique_ptr<PageToolRequest> MoveMouseToolRequest::Clone() const {
  return std::make_unique<MoveMouseToolRequest>(*this);
}

}  // namespace actor
