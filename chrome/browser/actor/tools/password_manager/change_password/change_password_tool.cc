// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/types/expected.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/browser/password_manager/chrome_password_change_service.h"
#include "chrome/browser/password_manager/password_change_service_factory.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/public/mojom/actor_types.mojom-shared.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_change/change_password_filling_helper.h"
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

mojom::ActionResultPtr ToErrorResult(
    const password_manager::ChangePasswordActionResult& result) {
  CHECK(!result.IsOk());
  return MakeResult(result.code, result.requires_page_stabilization,
                    result.message);
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
      confirm_button_(std::move(confirm_button)),
      filling_helper_(actor_surface.GetHandle().raw_value(),
                      PasswordChangeServiceFactory::GetForProfile(
                          &tool_delegate.GetProfile())) {}

ChangePasswordTool::~ChangePasswordTool() = default;

void ChangePasswordTool::Validate(DeprecatedToolCallback callback) {
  if (!IsValidPasswordFieldTarget(current_password_field_) ||
      !IsValidPasswordFieldTarget(new_password_field_) ||
      !IsValidPasswordFieldTarget(confirm_new_password_field_)) {
    PostResponseTask(std::move(callback),
                     MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                                /*requires_page_stabilization=*/false,
                                "Password field targets must be DOM nodes."));
    return;
  }
  if (password_manager::ChangePasswordActionResult result =
          password_manager::ChangePasswordFillingHelper::ValidateProvidedFields(
              current_password_field_, new_password_field_,
              confirm_new_password_field_);
      !result.IsOk()) {
    PostResponseTask(std::move(callback), ToErrorResult(result));
    return;
  }

  PostResponseTask(std::move(callback), MakeOkResult());
}

mojom::ActionResultPtr ChangePasswordTool::TimeOfUseValidation(
    const optimization_guide::proto::AnnotatedPageContent* last_observation) {
  ActorSurface* actor_surface = actor_surface_handle_.Get();
  content::WebContents* web_contents =
      actor_surface ? actor_surface->GetWebContents() : nullptr;
  if (!web_contents) {
    return MakeResult(mojom::ActionResultCode::kTabWentAway,
                      /*requires_page_stabilization=*/false,
                      "Target tab was destroyed before invocation.");
  }

  if (!last_observation) {
    return MakeResult(mojom::ActionResultCode::kFormFillingNoLastTabObservation,
                      /*requires_page_stabilization=*/false,
                      "Last tab observation is null.");
  }

  autofill::FieldGlobalId current_password_field_id;
  autofill::FieldGlobalId new_password_field_id;
  autofill::FieldGlobalId confirm_new_password_field_id;
  for (auto [target, field_id] :
       {std::pair{&current_password_field_, &current_password_field_id},
        std::pair{&new_password_field_, &new_password_field_id},
        std::pair{&confirm_new_password_field_,
                  &confirm_new_password_field_id}}) {
    if (!target->has_value()) {
      continue;
    }
    *field_id = GetFieldIdFromPageTarget(
        last_observation, actor_surface_handle_, target->value());
    //  TODO(crbug.com/574401623): we should be more lenient and be able to
    //  skip invalid fields (with a warning back to the agent). That would also
    //  allow us to skip fields that exist but are not password fields (by our
    //  classification).
    if (!*field_id) {
      return MakeResult(mojom::ActionResultCode::kFormFillingFieldNotFound,
                        /*requires_page_stabilization=*/false,
                        "Password field target not found in last observation.");
    }
  }

  base::expected<autofill::LocalFrameToken,
                 password_manager::ChangePasswordActionResult>
      frame_token_result = filling_helper_.ValidateAndSetFieldIds(
          current_password_field_id, new_password_field_id,
          confirm_new_password_field_id);
  if (!frame_token_result.has_value()) {
    return ToErrorResult(frame_token_result.error());
  }

  content::RenderFrameHost* rfh =
      autofill::FindRenderFrameHostByToken(*web_contents, *frame_token_result);
  password_manager::ContentPasswordManagerDriver* driver =
      rfh && rfh->IsActive() ? password_manager::ContentPasswordManagerDriver::
                                   GetForRenderFrameHost(rfh)
                             : nullptr;
  if (!driver || !driver->IsRenderFrameHostSupported()) {
    return MakeResult(
        mojom::ActionResultCode::kFrameWentAway,
        /*requires_page_stabilization=*/false,
        "Target frame or password manager driver is unavailable.");
  }
  password_manager::ChangePasswordActionResult validation_result =
      filling_helper_.ValidateDriverAndTargetFields(
          *driver,
          web_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  if (!validation_result.IsOk()) {
    return ToErrorResult(validation_result);
  }

  return MakeOkResult();
}

void ChangePasswordTool::Invoke(DeprecatedToolCallback callback) {
  filling_helper_.FillChangePasswordForm(
      base::BindOnce(&ChangePasswordTool::OnChangePasswordFormFilled,
                     weak_factory_.GetWeakPtr(), std::move(callback)));
}

void ChangePasswordTool::OnChangePasswordFormFilled(
    DeprecatedToolCallback callback,
    password_manager::ChangePasswordActionResult result) {
  if (!result.IsOk()) {
    std::move(callback).Run(ToErrorResult(result));
    return;
  }

  std::string message = std::move(result.message);
  if (confirm_button_.has_value()) {
    tool_delegate().EnqueueFollowupAction(std::make_unique<ClickToolRequest>(
        actor_surface_handle_, *confirm_button_, mojom::ClickType::kLeft,
        mojom::ClickCount::kSingle));
    if (const auto* dom_node = std::get_if<DomNode>(&*confirm_button_)) {
      absl::StrAppendFormat(&message,
                            " Triggered click on confirm button with id %d.",
                            dom_node->node_id);
    } else {
      absl::StrAppendFormat(&message, " Triggered click on confirm button %s.",
                            actor::DebugString(*confirm_button_));
    }
  }

  std::move(callback).Run(
      MakeOkResultWithMessage(result.requires_page_stabilization, message));
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

ActorSurfaceHandle ChangePasswordTool::GetTargetActorSurface() const {
  return actor_surface_handle_;
}

}  // namespace actor
