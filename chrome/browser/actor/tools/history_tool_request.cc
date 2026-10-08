// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/history_tool_request.h"

#include <memory>
#include <optional>
#include <string_view>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/history_tool.h"
#include "chrome/browser/actor/tools/registry/tool_definition.h"
#include "chrome/browser/actor/tools/registry/tool_definition_builder.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

using ::tabs::TabHandle;

namespace {

// Default description for the `go_back` tool.
constexpr std::string_view kGoBackToolDescription =
    "Go back to the previous page in history.";

// Default description for the `go_forward` tool.
constexpr std::string_view kGoForwardToolDescription =
    "Go forward to the next page in history.";

// Default description for the `reload_page` tool.
constexpr std::string_view kReloadPageToolDescription =
    "Reload the current page.";

ToolRequest::CreateToolResult CreateHistoryTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurfaceHandle actor_surface_handle,
    HistoryTool::Direction direction) {
  ActorSurface* actor_surface = actor_surface_handle.Get();

  if (!actor_surface) {
    return {/*tool=*/nullptr, MakeResult(mojom::ActionResultCode::kTabWentAway,
                                         /*requires_page_stabilization=*/false,
                                         "The tab is no longer present.")};
  }

  return {std::make_unique<HistoryTool>(task_id, tool_delegate, *actor_surface,
                                        direction),
          MakeOkResult()};
}

}  // namespace

HistoryBackToolRequest::HistoryBackToolRequest(TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}
HistoryBackToolRequest::~HistoryBackToolRequest() = default;

// static
std::optional<ToolDefinition> HistoryBackToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kGoBack, kModelFacingName,
                               kGoBackToolDescription)
      .Build();
}

ToolRequest::CreateToolResult HistoryBackToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetActorSurfaceHandle(),
                           HistoryTool::Direction::kBack);
}

void HistoryBackToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view HistoryBackToolRequest::Name() const {
  return kName;
}

bool HistoryBackToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A history tool is tab scoped but navigates *away* from the current URL --
  // the destination URL is checked in HistoryTool::Validate().
  return false;
}

HistoryForwardToolRequest::HistoryForwardToolRequest(TabHandle tab_handle)
    : TabToolRequest(tab_handle) {}
HistoryForwardToolRequest::~HistoryForwardToolRequest() = default;

// static
std::optional<ToolDefinition> HistoryForwardToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kGoForward, kModelFacingName,
                               kGoForwardToolDescription)
      .Build();
}

ToolRequest::CreateToolResult HistoryForwardToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetActorSurfaceHandle(),
                           HistoryTool::Direction::kForward);
}

void HistoryForwardToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view HistoryForwardToolRequest::Name() const {
  return kName;
}

bool HistoryForwardToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A history tool is tab scoped but navigates *away* from the current URL --
  // the destination URL is checked in HistoryTool::Validate().
  return false;
}

ReloadPageToolRequest::ReloadPageToolRequest(TabHandle tab_handle,
                                             bool bypass_cache)
    : TabToolRequest(tab_handle), bypass_cache_(bypass_cache) {}
ReloadPageToolRequest::~ReloadPageToolRequest() = default;

// static
std::optional<ToolDefinition> ReloadPageToolRequest::GetToolDefinition() {
  return ToolDefinitionBuilder(ToolId::kReloadPage, kModelFacingName,
                               kReloadPageToolDescription)
      .Build();
}

ToolRequest::CreateToolResult ReloadPageToolRequest::CreateTool(
    TaskId task_id,
    ToolDelegate& tool_delegate) const {
  return CreateHistoryTool(task_id, tool_delegate, GetActorSurfaceHandle(),
                           bypass_cache_
                               ? HistoryTool::Direction::kReloadBypassingCache
                               : HistoryTool::Direction::kReload);
}

void ReloadPageToolRequest::Apply(ToolRequestVisitorFunctor& f) const {
  f.Apply(*this);
}

std::string_view ReloadPageToolRequest::Name() const {
  return kName;
}

bool ReloadPageToolRequest::RequiresUrlCheckInCurrentTab() const {
  // A reload tool checks the target entry URL in HistoryTool::Validate().
  return false;
}

}  // namespace actor
