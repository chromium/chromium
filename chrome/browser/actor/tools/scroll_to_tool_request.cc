// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/scroll_to_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"

namespace actor {

namespace {

// Default description for the `scroll_to` tool.
constexpr std::string_view kScrollToToolDescription =
    "Scrolls a specific element into view.";

// Description for the `dom_node_id` parameter of the `scroll_to` tool.
constexpr std::string_view kDomNodeIdParamDescription =
    "The DOM node ID of the target element to scroll into view.";

}  // namespace


ScrollToToolRequest::ScrollToToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    const PageTarget& target)
    : PageToolRequest(actor_surface_handle, target) {}

ScrollToToolRequest::~ScrollToToolRequest() = default;

// static
std::optional<ToolDefinition> ScrollToToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kScrollTo, kModelFacingName,
                               kScrollToToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kDomNodeIdParam, kDomNodeIdParamDescription))
      .Build();
}

void ScrollToToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ScrollToToolRequest::Name() const {
  return kName;
}

mojom::ToolActionPtr ScrollToToolRequest::ToMojoToolAction(
    content::RenderFrameHost& frame) const {
  auto action = mojom::ScrollToAction::New();
  return mojom::ToolAction::NewScrollTo(std::move(action));
}

std::unique_ptr<PageToolRequest> ScrollToToolRequest::Clone() const {
  return std::make_unique<ScrollToToolRequest>(*this);
}

}  // namespace actor
