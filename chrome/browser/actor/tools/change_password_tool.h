// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_H_

#include <memory>
#include <string>

#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class ChangePasswordTool : public Tool {
 public:
  ChangePasswordTool(TaskId task_id,
                     ToolDelegate& tool_delegate,
                     tabs::TabInterface& tab);
  ~ChangePasswordTool() override;

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
  tabs::TabHandle tab_handle_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_CHANGE_PASSWORD_TOOL_H_
