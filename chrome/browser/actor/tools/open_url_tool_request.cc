// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/open_url_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/open_url_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "url/origin.h"

namespace actor {

namespace {

// Default description for the `open_url` tool.
constexpr std::string_view kOpenUrlToolDescription =
    "Opens a URL in the current tab or in a new tab.";

// Description for the `url` parameter of the `open_url` tool.
constexpr std::string_view kUrlParamDescription =
    "The complete URL to open (e.g. \"https://example.com\").";

// Description for the `new_tab` parameter of the `open_url` tool.
constexpr std::string_view kNewTabParamDescription =
    "Opens the URL in a new tab if true, otherwise opens the URL in the "
    "current tab.";

}  // namespace

OpenUrlToolRequest::OpenUrlToolRequest(ActorSurfaceHandle actor_surface_handle,
                                       GURL url,
                                       bool open_in_new_tab)
    : TabToolRequest(actor_surface_handle),
      url_(std::move(url)),
      open_in_new_tab_(open_in_new_tab) {}

OpenUrlToolRequest::~OpenUrlToolRequest() = default;

OpenUrlToolRequest::OpenUrlToolRequest(const OpenUrlToolRequest&) = default;
OpenUrlToolRequest& OpenUrlToolRequest::operator=(const OpenUrlToolRequest&) =
    default;

// static
std::optional<ToolDefinition> OpenUrlToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kOpenUrl, kModelFacingName,
                               kOpenUrlToolDescription)
      .SetToolParameterSchema(
          ToolSchemaBuilder()
              .AddStringProperty(kUrlParam, kUrlParamDescription,
                                 ToolSchemaBuilder::kFormatUri)
              .AddBooleanProperty(kNewTabParam, kNewTabParamDescription))
      .Build();
}

bool OpenUrlToolRequest::RequiresUrlCheckInCurrentTab() const {
  // This tool either navigates *away* from the current URL or opens a new tab;
  // it never acts within the current page.
  return false;
}

ToolRequest::CreateToolResult OpenUrlToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  if (!url_.is_valid()) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                       /*requires_page_stabilization=*/false,
                       "The URL is invalid.")};
  }

  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<OpenUrlTool>(task_id, tool_delegate, *actor_surface,
                                        url_, open_in_new_tab_),
          MakeOkResult()};
}

void OpenUrlToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view OpenUrlToolRequest::Name() const {
  return kName;
}

std::optional<url::Origin> OpenUrlToolRequest::AssociatedOriginGrant() const {
  // The navigation to `url_` runs as a follow-up `NavigateToolRequest`, which
  // doesn't get its implicit grant applied, so grant the origin up front.
  return url::Origin::Create(url_);
}

}  // namespace actor
