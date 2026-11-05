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
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/actor/tools/tool_request_visitor_functor.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/browser_window/public/profile_browser_collection.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/sessions/core/session_id.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

// Default description for the `navigate` tool.
constexpr std::string_view kNavigateToolDescription =
    "Opens a URL in the browser.";

// Description for the `url` parameter of the `navigate` tool.
constexpr std::string_view kUrlParamDescription =
    "The complete URL to open (e.g. \"https://example.com\").";

// Returns the profile's most recently active normal browser window, or an
// invalid ID if there is none. Other window types (e.g. popups and apps) are
// skipped since a new foreground tab can't be opened in them.
SessionID GetActiveWindowId(ToolDelegate& tool_delegate) {
  ProfileBrowserCollection* collection =
      ProfileBrowserCollection::GetForProfile(&tool_delegate.GetProfile());
  if (!collection) {
    return SessionID::InvalidValue();
  }
  BrowserWindowInterface* browser = collection->FindTabbedBrowser();
  return browser ? browser->GetSessionID() : SessionID::InvalidValue();
}

}  // namespace

NavigateToolRequest::NavigateToolRequest(
    ActorSurfaceHandle actor_surface_handle,
    GURL url,
    bool open_in_new_tab)
    : actor_surface_handle_(actor_surface_handle),
      url_(url),
      open_in_new_tab_(open_in_new_tab) {
  // The given handle need not be valid - the handle is validated at time of
  // dereferencing when instantiating a tool. It must be null exactly when
  // opening a new tab.
  CHECK_EQ(open_in_new_tab_, actor_surface_handle_.is_null());
}

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
  if (open_in_new_tab_) {
    return {std::make_unique<NavigateTool>(
                task_id, tool_delegate, GetActiveWindowId(tool_delegate), url_),
            MakeOkResult()};
  }

  ActorSurface* actor_surface = GetActorSurfaceHandle().Get();
  if (!actor_surface) {
    return {/*tool=*/nullptr,
            MakeResult(mojom::ActionResultCode::kTabWentAway,
                       /*requires_page_stabilization=*/false,
                       "The target surface is no longer present.")};
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

ActorSurfaceHandle NavigateToolRequest::GetActorSurfaceHandle() const {
  return actor_surface_handle_;
}

GURL NavigateToolRequest::GetURLForJournal() const {
  if (ActorSurface* actor_surface = GetActorSurfaceHandle().Get()) {
    return actor_surface->GetWebContents()->GetLastCommittedURL();
  }
  return ToolRequest::GetURLForJournal();
}

std::optional<url::Origin> NavigateToolRequest::AssociatedOriginGrant() const {
  return url::Origin::Create(url_);
}

}  // namespace actor
