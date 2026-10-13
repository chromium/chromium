// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <memory>
#include <string>
#include <utility>

#include "chrome/browser/actor/actor_surface.h"
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
                                       ActorSurface& actor_surface)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()) {}

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
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  if (!actor_surface || !actor_surface->GetWebContents()) {
    return nullptr;
  }

  content::RenderFrameHost* rfh =
      actor_surface->GetWebContents()->GetPrimaryMainFrame();
  if (!rfh) {
    return nullptr;
  }

  return std::make_unique<ObservationDelayController>(
      *rfh, task_id(), journal(), std::move(page_stability_config));
}

void ChangePasswordTool::UpdateTaskBeforeInvoke(ActorTask& task,
                                                ToolCallback callback) const {
  task.AddActorSurface(actor_surface_handle_, /*stop_task_on_detach=*/true,
                       std::move(callback));
}

ActorSurfaceHandle ChangePasswordTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
