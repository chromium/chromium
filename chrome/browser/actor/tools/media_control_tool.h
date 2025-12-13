// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_
#define CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_

#include "chrome/browser/actor/tools/media_control_tool_request.h"
#include "chrome/browser/actor/tools/tool.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "components/tabs/public/tab_interface.h"

namespace actor {

class MediaControlTool : public Tool {
 public:
  MediaControlTool(TaskId task_id,
                   ToolDelegate& tool_delegate,
                   tabs::TabInterface& tab,
                   MediaControl media_control);
  ~MediaControlTool() override;

  // Tool:
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
  MediaControl media_control_;
};

}  // namespace actor

#endif  // CHROME_BROWSER_ACTOR_TOOLS_MEDIA_CONTROL_TOOL_H_
