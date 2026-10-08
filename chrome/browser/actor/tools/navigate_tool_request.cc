// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/navigate_tool_request.h"

#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/navigate_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

namespace {

// Default description for the `navigate` tool.
constexpr std::string_view kNavigateToolDescription =
    "Opens a URL in the browser.";

// Description for the `url` parameter of the `navigate` tool.
constexpr std::string_view kUrlParamDescription =
    "The complete URL to open (e.g. \"https://example.com\").";

}  // namespace

using ::tabs::TabHandle;

NavigateToolRequest::NavigateToolRequest(TabHandle tab_handle, GURL url)
    : TabToolRequest(tab_handle), url_(url) {}

NavigateToolRequest::~NavigateToolRequest() = default;

std::optional<ToolDefinition> NavigateToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kNavigate, kModelFacingName,
                               kNavigateToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kUrlParam, kUrlParamDescription, ToolSchemaBuilder::kFormatUri))
      .Build();
}

ToolRequest::CreateToolResult NavigateToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<NavigateTool>(task_id, tool_delegate, *actor_surface,
                                         url_),
          MakeOkResult()};
}

bool NavigateToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A navigate tool is tab scoped but navigates *away* from the current URL.
  return false;
}

void NavigateToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view NavigateToolRequest::Name() const {
  return kName;
}

std::optional<url::Origin> NavigateToolRequest::AssociatedOriginGrant() const {
  return url::Origin::Create(url_);
}

}  // namespace actor
