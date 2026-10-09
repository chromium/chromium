// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/find_and_highlight_tool_request.h"

#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/find_and_highlight_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"

namespace actor {

namespace {

// Default description for the `find_and_highlight` tool.
constexpr std::string_view kFindAndHighlightToolDescription =
    "Highlight and scroll to specific text on the page.";

// Description for the `query` parameter of the `find_and_highlight` tool.
constexpr std::string_view kQueryParamDescription =
    "A short phrase copied exactly as it appears in the page content.";

}  // namespace

FindAndHighlightToolRequest::FindAndHighlightToolRequest(
    tabs::TabHandle tab_handle,
    std::string query)
    : TabToolRequest(tab_handle), query_(std::move(query)) {}

FindAndHighlightToolRequest::~FindAndHighlightToolRequest() = default;

FindAndHighlightToolRequest::FindAndHighlightToolRequest(
    const FindAndHighlightToolRequest&) = default;
FindAndHighlightToolRequest& FindAndHighlightToolRequest::operator=(
    const FindAndHighlightToolRequest&) = default;

std::optional<ToolDefinition> FindAndHighlightToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kFindAndHighlight, kModelFacingName,
                               kFindAndHighlightToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddStringProperty(
          kQueryParam, kQueryParamDescription))
      .Build();
}

ToolRequest::CreateToolResult FindAndHighlightToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (query_.empty()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "Query cannot be empty.")};
  }

  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<FindAndHighlightTool>(task_id, tool_delegate,
                                                 *actor_surface, query_),
          MakeOkResult()};
}

void FindAndHighlightToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view FindAndHighlightToolRequest::Name() const {
  return kName;
}

}  // namespace actor
