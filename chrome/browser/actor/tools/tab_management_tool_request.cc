// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/tab_management_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>

#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/registry/tool_schema_builder.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

namespace {

// Default description for the `close_current_tab` tool.
constexpr std::string_view kCloseTabToolDescription =
    "Close the current browser tab.";

// Default description for the `create_tab` tool.
constexpr std::string_view kCreateTabToolDescription =
    "Opens a new tab in the browser.";

// Default description for the `activate_tab` tool.
constexpr std::string_view kActivateTabToolDescription =
    "Switches to an open tab by its ID.";

// Description for the `tab_id` parameter of the `activate_tab` tool.
constexpr std::string_view kTabIdParamDescription =
    "The ID of the tab to activate.";

}  // namespace
using tabs::TabInterface;

CreateTabToolRequest::CreateTabToolRequest(int32_t window_id,
                                           WindowOpenDisposition disposition)
    : window_id_(window_id), disposition_(disposition) {}

CreateTabToolRequest::~CreateTabToolRequest() = default;

// static
std::optional<ToolDefinition> CreateTabToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kCreateTab, kModelFacingName,
                               kCreateTabToolDescription)
      .Build();
}

bool CreateTabToolRequest::AddsTabToObservationSet() const {
  return true;
}

ToolRequest::CreateToolResult CreateTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return {std::make_unique<TabManagementTool>(task_id, tool_delegate,
                                              window_id_, disposition_),
          MakeOkResult()};
}

void CreateTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CreateTabToolRequest::Name() const {
  return kName;
}

ActivateTabToolRequest::ActivateTabToolRequest(
    ActorSurfaceHandle actor_surface_handle)
    : TabToolRequest(actor_surface_handle) {}

ActivateTabToolRequest::~ActivateTabToolRequest() = default;

// static
std::optional<ToolDefinition> ActivateTabToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kActivateTab, kModelFacingName,
                               kActivateTabToolDescription)
      .SetToolParameterSchema(ToolSchemaBuilder().AddIntegerProperty(
          kTabIdParam, kTabIdParamDescription))
      .Build();
}

ToolRequest::CreateToolResult ActivateTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetActorSurfaceHandle().GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<TabManagementTool>(
              task_id, tool_delegate, TabManagementTool::kActivate,
              GetActorSurfaceHandle().GetTabHandle()),
          MakeOkResult()};
}

void ActivateTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ActivateTabToolRequest::Name() const {
  return kName;
}

CloseTabToolRequest::CloseTabToolRequest(
    ActorSurfaceHandle actor_surface_handle)
    : TabToolRequest(actor_surface_handle) {}

CloseTabToolRequest::~CloseTabToolRequest() = default;

// static
std::optional<ToolDefinition> CloseTabToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kCloseTab, kModelFacingName,
                               kCloseTabToolDescription)
      .Build();
}

ToolRequest::CreateToolResult CloseTabToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  TabInterface* tab = GetActorSurfaceHandle().GetTabHandle().Get();
  if (!tab) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }
  return {std::make_unique<TabManagementTool>(
              task_id, tool_delegate, TabManagementTool::kClose,
              GetActorSurfaceHandle().GetTabHandle()),
          MakeOkResult()};
}

void CloseTabToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view CloseTabToolRequest::Name() const {
  return kName;
}

}  // namespace actor
