// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "third_party/abseil-cpp/absl/strings/str_format.h"

namespace actor {

namespace {

bool IsValidPasswordFieldTarget(const std::optional<PageTarget>& target) {
  // Password manager form filling requires DOM node IDs (DomNode) to locate the
  // form elements in the renderer.
  return !target.has_value() || std::holds_alternative<DomNode>(*target);
}

std::string MaybeTargetDebugString(const std::optional<PageTarget>& target) {
  return target ? DebugString(*target) : "null";
}

}  // namespace

ChangePasswordTool::ChangePasswordTool(
    TaskId task_id,
    ToolDelegate& tool_delegate,
    ActorSurface& actor_surface,
    std::optional<PageTarget> current_password_field,
    std::optional<PageTarget> new_password_field,
    std::optional<PageTarget> confirm_new_password_field,
    std::optional<PageTarget> confirm_button)
    : Tool(task_id, tool_delegate),
      actor_surface_handle_(actor_surface.GetHandle()),
      current_password_field_(std::move(current_password_field)),
      new_password_field_(std::move(new_password_field)),
      confirm_new_password_field_(std::move(confirm_new_password_field)),
      confirm_button_(std::move(confirm_button)) {}

ChangePasswordTool::~ChangePasswordTool() = default;

void ChangePasswordTool::Validate(ToolCallback callback) {
  if (!IsValidPasswordFieldTarget(current_password_field_) ||
      !IsValidPasswordFieldTarget(new_password_field_) ||
      !IsValidPasswordFieldTarget(confirm_new_password_field_)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                                /*requires_page_stabilization=*/false,
                                "Password field targets must be DOM nodes."));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

void ChangePasswordTool::Invoke(ToolCallback callback) {
  PostResponseTask(std::move(callback), MakeOkResult());
}

std::string ChangePasswordTool::DebugString() const {
  return absl::StrFormat(
      "ChangePasswordTool[current_password_field=%s, new_password_field=%s, "
      "confirm_new_password_field=%s, confirm_button=%s]",
      MaybeTargetDebugString(current_password_field_),
      MaybeTargetDebugString(new_password_field_),
      MaybeTargetDebugString(confirm_new_password_field_),
      MaybeTargetDebugString(confirm_button_));
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
