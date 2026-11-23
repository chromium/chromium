// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/switch_tab_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/check.h"
#include "chrome/browser/actor/tools/page_search_utils.h"
#include "chrome/browser/actor/tools/tab_management_tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/validate_url_util.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/public/mojom/actor_types.mojom.h"
#include "components/tabs/public/tab_interface.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

SwitchTabTool::SwitchTabTool(TaskId task_id,
                             ToolDelegate& tool_delegate,
                             SessionID window_id,
                             std::string query)
    : Tool(task_id, tool_delegate),
      window_id_(window_id),
      query_(std::move(query)) {}

SwitchTabTool::~SwitchTabTool() = default;

void SwitchTabTool::Validate(DeprecatedToolCallback callback) {
  BrowserWindowInterface* browser = GetBrowser();
  mojom::ActionResultPtr window_result = ValidateBrowserWindow(browser);
  if (!IsOk(*window_result)) {
    PostResponseTask(std::move(callback), std::move(window_result));
    return;
  }

  TabSearchResult search_result = FindMatchingTab(browser, query_);
  switch (search_result.status) {
    case TabSearchResult::Status::kAlreadyOnActiveTab:
      PostResponseTask(
          std::move(callback),
          MakeResult(mojom::ActionResultCode::kPageSearchAlreadyOnMatchingTab,
                     /*requires_page_stabilization=*/false,
                     absl::StrFormat("The active tab already matches \"%s\".",
                                     query_)));
      return;
    case TabSearchResult::Status::kNoMatch:
      PostResponseTask(
          std::move(callback),
          MakeResult(mojom::ActionResultCode::kPageSearchNoMatch,
                     /*requires_page_stabilization=*/false,
                     absl::StrFormat("No open tab matched \"%s\".", query_)));
      return;
    case TabSearchResult::Status::kMatchedBackgroundTab:
      match_ = std::move(*search_result.match);
      ValidateUrlIsAcceptableNavigationDestination(match_->url, tool_delegate(),
                                                   std::move(callback));
      return;
  }
}

void SwitchTabTool::Invoke(DeprecatedToolCallback callback) {
  CHECK(match_.has_value());

  // The tab may have been closed, or dragged into a different window, between
  // Validate() and now. ResolveTabInWindow() rejects both cases; acting on the
  // bare handle would let a tab switch pull focus to another window.
  BrowserWindowInterface* browser = GetBrowser();
  tabs::TabInterface* tab =
      browser ? ResolveTabInWindow(*match_, browser) : nullptr;
  if (!tab) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kPageSearchNoMatch,
                   /*requires_page_stabilization=*/false,
                   "The matched tab is no longer open in this window."));
    return;
  }

  // TODO(b/565736154): Replace this delegation once composite tools are
  // supported (b/555808355).
  activate_tool_ = std::make_unique<TabManagementTool>(
      task_id(), tool_delegate(), TabManagementTool::kActivate, match_->handle);
  activate_tool_->Invoke(std::move(callback));
}

std::string SwitchTabTool::DebugString() const {
  return absl::StrFormat("SwitchTabTool:query(%s)", query_);
}

std::string SwitchTabTool::JournalEvent() const {
  return "SwitchTab";
}

std::unique_ptr<ObservationDelayController>
SwitchTabTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  // Activating an already-loaded tab doesn't navigate, so there is nothing to
  // wait on.
  return nullptr;
}

ActorSurfaceHandle SwitchTabTool::GetTargetActorSurface() const {
  // Null until Validate() resolves the query.
  return match_.has_value() ? ActorSurfaceHandle::From(match_->handle)
                            : ActorSurfaceHandle::Null();
}

BrowserWindowInterface* SwitchTabTool::GetBrowser() const {
  return FindBrowserWindow(window_id_);
}

}  // namespace actor
