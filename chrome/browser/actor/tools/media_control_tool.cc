// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/media_control_tool.h"

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/web_contents.h"

namespace actor {

namespace {

content::RenderFrameHost& GetPrimaryMainFrameOfTab(tabs::TabHandle tab_handle) {
  return *tab_handle.Get()->GetContents()->GetPrimaryMainFrame();
}

}  // namespace

MediaControlTool::MediaControlTool(TaskId task_id,
                                   ToolDelegate& tool_delegate,
                                   tabs::TabInterface& tab,
                                   MediaControl media_control)
    : Tool(task_id, tool_delegate),
      tab_handle_(tab.GetHandle()),
      media_control_(media_control) {}

MediaControlTool::~MediaControlTool() = default;

void MediaControlTool::Validate(ValidateCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void MediaControlTool::Invoke(InvokeCallback callback) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kTabWentAway));
    return;
  }

  invoke_callback_ = std::move(callback);
}

std::string MediaControlTool::DebugString() const {
  return "MediaControlTool";
}

std::string MediaControlTool::JournalEvent() const {
  return "MediaControl";
}

std::unique_ptr<ObservationDelayController>
MediaControlTool::GetObservationDelayer(
    std::optional<ObservationDelayController::PageStabilityConfig>
        page_stability_config) {
  if (!tab_handle_.Get()) {
    return nullptr;
  }
  return std::make_unique<ObservationDelayController>(
      GetPrimaryMainFrameOfTab(tab_handle_), task_id(), journal(),
      page_stability_config);
}

void MediaControlTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                              InvokeCallback callback) const {
  task.AddTab(tab_handle_, std::move(callback));
}

tabs::TabHandle MediaControlTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
