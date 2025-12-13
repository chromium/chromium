// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "chrome/browser/ui/tabs/tab_strip_model_observer.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

class ObservationDelayController;

// A tool to manage the tabs in a browser window, e.g. create, close,
// activate, etc.
// TODO(crbug.com/445993857): Implement actions other than create.
class TabManagementTool : public Tool, public TabStripModelObserver {
 public:
  enum Action { kCreate, kActivate, kClose };

  // Create constructor
  TabManagementTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    int32_t window,
                    WindowOpenDisposition create_disposition);
  // Activate|Close constructor.
  TabManagementTool(TaskId task_id,
                    ToolDelegate& tool_delegate,
                    Action action,
                    tabs::TabHandle target_tab);

  ~TabManagementTool() override;

  // actor::Tool:
  void Validate(ToolCallback callback) override;
  void Invoke(ToolCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer(
      ObservationDelayController::PageStabilityConfig page_stability_config)
      override;
  void UpdateTaskAfterInvoke(ActorTask& task,
                             mojom::ActionResultPtr result,
                             ToolCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

  // TabStripModelObserver
  void OnTabStripModelChanged(TabStripModel* tab_strip_model,
                              const TabStripModelChange& change,
                              const TabStripSelectionChange& selection) final;

 private:
  // Called when the browser with `window_id_` has closed.
  void OnBrowserDidClose(BrowserWindowInterface* browser);

  Action action_;

  ToolCallback callback_;

  // Used for activate or close action. For the create action, this will be
  // updated to the newly created tab after `Invoke()` is called.
  std::optional<tabs::TabHandle> target_tab_;

  // If creating a tab, whether to create in the foreground.
  std::optional<WindowOpenDisposition> create_disposition_;

  // If creating a tab, the window in which to create the tab.
  std::optional<int32_t> window_id_;

  // Subscription to the close event for the Browser corresponding to
  // `window_id_`.
  base::CallbackListSubscription browser_did_close_subscription_;

  base::WeakPtrFactory<TabManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
