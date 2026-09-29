// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_H_

#include <memory>
#include <optional>
#include <string>
#include <vector>

#include "base/memory/weak_ptr.h"
#include "base/task/cancelable_task_tracker.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_search_utils.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/actor/core/task_id.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"
#include "url/gurl.h"

class BrowserWindowInterface;

namespace actor {

class NavigateTool;
class TabManagementTool;
class ToolDelegate;

// Opens a known page matching `query_` in a single browser window
// (`window_id_`) using a strict cascading search order:
//   1. Tabs (Sync): Searches open tabs in `window_id_` via
//      `FindMatchingTabs()`.
//      - Ignores the currently active tab when a background tab also matches.
//      - Fails with `kPageSearchAlreadyOnMatchingTab` if only the active tab
//        matches, or `kPageSearchAmbiguousMatch` if more than one background
//        tab matches.
//   2. History (Async): If no open tab in `window_id_` matches, searches
//      browsing history via `FindMatchingHistory()`.
//   3. Bookmarks (Sync fallback): Inside the history callback, if history
//      returns no matches, searches bookmarks via `FindMatchingBookmarks()`.
//      Fails with `kPageSearchNoMatch` if bookmarks also return empty.
//
// Once a tab or URL is resolved, its destination URL is validated via
// `ValidateUrlIsAcceptableNavigationDestination()`. In `Invoke()`, tab matches
// are activated via an owned `TabManagementTool(kActivate)`, and URL matches
// from history or bookmarks are navigated in the window's active tab via an
// owned `NavigateTool`.
class OpenKnownPageTool : public Tool {
 public:
  OpenKnownPageTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    SessionID window_id,
                    std::string query);
  OpenKnownPageTool(const OpenKnownPageTool&) = delete;
  OpenKnownPageTool& operator=(const OpenKnownPageTool&) = delete;
  ~OpenKnownPageTool() override;

  // actor::Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskBeforeInvoke(ActorTask& task,
                              ToolCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  BrowserWindowInterface* GetBrowser() const;

  void OnHistoryMatches(ToolCallback callback,
                        std::vector<PageMatch> history_matches);

  void ValidateDestinationUrl(const GURL& url, ToolCallback callback);

  SessionID window_id_;
  std::string query_;

  // Set when Stage 1 (Tabs) resolves to a single open background tab.
  std::optional<TabMatch> matched_tab_;

  // Set when Stage 2 (History) or Stage 3 (Bookmarks) resolves to a URL to be
  // navigated in the active tab of `window_id_`.
  std::optional<TabMatch> navigation_target_;

  // Delegates tab activation when `matched_tab_` is set.
  std::unique_ptr<TabManagementTool> activate_tool_;

  // Delegates URL navigation when `navigation_target_` is set. Created in
  // `OnHistoryMatches()` so `UpdateTaskBeforeInvoke()` and
  // `GetObservationDelayer()` can delegate to it prior to `Invoke()`.
  std::unique_ptr<NavigateTool> navigate_tool_;

  base::CancelableTaskTracker cancelable_task_tracker_;
  base::WeakPtrFactory<OpenKnownPageTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_OPEN_KNOWN_PAGE_TOOL_H_
