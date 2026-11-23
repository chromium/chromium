// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_H_

#include <memory>
#include <optional>
#include <string>

#include "chrome/browser/actor/actor_surface_handle.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_search_utils.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/actor/core/task_id.h"
#include "components/sessions/core/session_id.h"
#include "components/tabs/public/tab_interface.h"

class BrowserWindowInterface;

namespace actor {

class TabManagementTool;
class ToolDelegate;

// Brings an already-open background tab to the foreground, choosing it by
// fuzzy-matching a query against the titles and URLs of the tabs in a single
// window.
//
// Validate() resolves the query via `FindMatchingTab()`:
// 1. Ignores the currently active tab whenever at least one background tab
//    matches, selecting the most recently active matching background tab.
// 2. Fails with `kPageSearchAlreadyOnMatchingTab` if only the active tab
//    matches, or `kPageSearchNoMatch` if no tab matches.
// 3. Validates the matched tab's URL via
//    `ValidateUrlIsAcceptableNavigationDestination()`.
//
// Once the query resolves to an allowed background tab, the activation itself
// is delegated to an owned TabManagementTool so that its tab-strip observation
// and completion machinery isn't duplicated here.
class SwitchTabTool : public Tool {
 public:
  SwitchTabTool(TaskId task_id,
                ToolDelegate& tool_delegate,
                SessionID window_id,
                std::string query);
  ~SwitchTabTool() override;

  // actor::Tool:
  void Validate(DeprecatedToolCallback callback) override;
  void Invoke(DeprecatedToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  ActorSurfaceHandle GetTargetActorSurface() const override;

 private:
  // The window `query_` is searched in, and the only window this tool will
  // ever activate a tab in. Null if the window has gone away.
  BrowserWindowInterface* GetBrowser() const;

  SessionID window_id_;
  std::string query_;

  // The background tab `query_` matched. Set by a successful Validate().
  std::optional<TabMatch> match_;

  // Performs the activation. Created in Invoke(), once `match_` is known to
  // still resolve to a live tab in `window_id_`.
  std::unique_ptr<TabManagementTool> activate_tool_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_SWITCH_TAB_TOOL_H_
