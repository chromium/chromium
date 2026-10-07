// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/change_password_tool.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor/action_result.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"

namespace actor {

ChangePasswordTool::ChangePasswordTool(TaskId task_id,
                                       ToolDelegate& tool_delegate,
                                       tabs::TabInterface& tab)
    : Tool(task_id, tool_delegate), tab_handle_(tab.GetHandle()) {}

ChangePasswordTool::~ChangePasswordTool() = default;

void ChangePasswordTool::Validate(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

void ChangePasswordTool::Invoke(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string ChangePasswordTool::DebugString() const {
  return "ChangePasswordTool";
}

std::string ChangePasswordTool::JournalEvent() const {
  return "ChangePassword";
}

std::unique_ptr<ObservationDelayController>
ChangePasswordTool::GetObservationDelayer(
    ObservationDelayController::PageStabilityConfig page_stability_config) {
  tabs::TabInterface* tab = tab_handle_.Get();
  if (!tab || !tab->GetContents()) {
    return nullptr;
  }

  content::RenderFrameHost* rfh = tab->GetContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

void ChangePasswordTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                                ToolCallback callback) const {
  task.AddTab(tab_handle_, /*stop_task_on_detach=*/true, std::move(callback));
}

tabs::TabHandle ChangePasswordTool::GetTargetTab() const {
  return tab_handle_;
}

}  // namespace actor
