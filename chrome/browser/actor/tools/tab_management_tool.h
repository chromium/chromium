// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_

#include "base/memory/weak_ptr.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_request.h"
#include "ui/base/window_open_disposition.h"

namespace actor {

class ObservationDelayController;

// A tool to manage the tabs in a browser window, e.g. create, close,
// activate, etc.
// TODO(crbug.com/411462297): Implement actions other than create.
class TabManagementTool : public Tool {
 public:
  enum Action { kCreate, kActivate, kClose };

  // Create constructor
  TabManagementTool(TaskId task_id,
                    AggregatedJournal& journal,
                    int32_t window,
                    WindowOpenDisposition create_disposition);
  // Activate|Close constructor.
  TabManagementTool(TaskId task_id,
                    AggregatedJournal& journal,
                    Action action,
                    tabs::TabHandle target_tab);

  ~TabManagementTool() override;

  // actor::Tool:
  void Validate(ValidateCallback callback) override;
  void Invoke(InvokeCallback callback) override;
  std::string DebugString() const override;
  std::string JournalEvent() const override;
  std::unique_ptr<ObservationDelayController> GetObservationDelayer()
      const override;

 private:
  Action action_;

  // Used for activate or close action.
  std::optional<tabs::TabHandle> target_tab_;

  // If creating a tab, whether to create in the foreground.
  std::optional<WindowOpenDisposition> create_disposition_;

  // If creating a tab, the window in which to create the tab.
  std::optional<int32_t> window_id_;

  base::WeakPtrFactory<TabManagementTool> weak_ptr_factory_{this};
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_TAB_MANAGEMENT_TOOL_H_
