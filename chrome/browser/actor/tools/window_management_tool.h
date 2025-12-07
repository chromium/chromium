// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_

#include "base/callback_list.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/task_id.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

// A tool to manage browser windows, e.g. create, close, activate, etc.
class WindowManagementTool : public Tool {
 public:
  enum class Action { kCreate, kActivate, kClose };

  // Create constructor
  explicit WindowManagementTool(TaskId task_id, ToolDelegate& tool_delegate);

  // Activate|Close constructor.
  WindowManagementTool(Action action,
                       TaskId task_id,
                       ToolDelegate& tool_delegate,
                       int32_t window_id);
  ~WindowManagementTool() override;

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
  void UpdateTaskAfterInvoke(ActorTask& task,
                             mojom::ActionResultPtr result,
                             ToolCallback callback) const override;
  tabs::TabHandle GetTargetTab() const override;

 private:
  // Called when the browser with `window_id_` has closed.
  void OnBrowserDidClose(BrowserWindowInterface* browser);

  void OnBrowserDidBecomeActive(BrowserWindowInterface* Browser);
  void OnInvokeFinished(mojom::ActionResultPtr result);

  Action action_;
  std::optional<int32_t> window_id_;

  // If creating a window, this will be set to the handle of the initial tab.
  std::optional<tabs::TabHandle> created_tab_handle_;

  ToolCallback callback_;

  // Subscription to the close event for the Browser corresponding to
  // `window_id_`.
  base::CallbackListSubscription browser_did_close_subscription_;

  base::CallbackListSubscription browser_did_become_active_subscription_;

  base::WeakPtrFactory<WindowManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_WINDOW_MANAGEMENT_TOOL_H_
