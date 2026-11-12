// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/actor/tools/password_manager/change_password/change_password_tool.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <variant>
#include <vector>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/string_util.h"
#include "chrome/browser/actor/actor_surface.h"
#include "chrome/browser/actor/actor_task.h"
#include "chrome/browser/actor/tools/click_tool_request.h"
#include "chrome/browser/actor/tools/observation_delay_controller.h"
#include "chrome/browser/actor/tools/page_target_util.h"
#include "chrome/browser/actor/tools/tool_callbacks.h"
#include "chrome/browser/actor/tools/tool_delegate.h"
#include "chrome/common/actor.mojom.h"
#include "chrome/common/actor/action_result.h"
#include "components/actor/core/shared_types.h"
#include "components/actor/public/mojom/actor_types.mojom-forward.h"
#include "components/autofill/content/browser/renderer_forms_from_browser_form.h"
#include "components/autofill/core/common/form_data.h"
#include "components/autofill/core/common/form_field_data.h"
#include "components/autofill/core/common/unique_ids.h"
#include "components/password_manager/content/browser/content_password_manager_driver.h"
#include "components/password_manager/core/browser/password_form_cache.h"
#include "components/password_manager/core/browser/password_form_manager.h"
#include "components/password_manager/core/browser/password_manager_interface.h"
#include "components/password_manager/core/browser/password_string.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "mojo/public/cpp/bindings/callback_helpers.h"
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

bool WasFieldFilled(const autofill::FormData& form_data,
                    autofill::FieldRendererId renderer_id) {
  CHECK(renderer_id);
  auto it = std::ranges::find(form_data.fields(), renderer_id,
                              &autofill::FormFieldData::renderer_id);
  return it != form_data.fields().end() &&
         it->is_autofilled_according_to_renderer();
}

// Returns the cached `PasswordFormManager` in `driver`'s frame that manages
// `renderer_id` as a password input element, or `nullptr` if none matches.
// Requires `driver` to be non-null and `IsRenderFrameHostSupported()`. The
// returned pointer is owned by `PasswordFormCache` and must not be retained
// across asynchronous boundaries.
password_manager::PasswordFormManager* FindPasswordFormManagerForField(
    password_manager::ContentPasswordManagerDriver* driver,
    autofill::FieldRendererId renderer_id) {
  CHECK(driver && driver->IsRenderFrameHostSupported());
  if (!renderer_id) {
    return nullptr;
  }
  password_manager::PasswordManagerInterface* password_manager =
      driver->GetPasswordManager();
  password_manager::PasswordFormCache* form_cache =
      password_manager ? password_manager->GetPasswordFormCache() : nullptr;
  if (!form_cache) {
    return nullptr;
  }
  for (const std::unique_ptr<password_manager::PasswordFormManager>&
           form_manager : form_cache->GetFormManagers()) {
    if (!form_manager->DoesManage(renderer_id, driver)) {
      continue;
    }
    const autofill::FormData* observed_form = form_manager->observed_form();
    auto it = std::ranges::find(observed_form->fields(), renderer_id,
                                &autofill::FormFieldData::renderer_id);
    if (it != observed_form->fields().end() && it->IsPasswordInputElement()) {
      return form_manager.get();
    }
  }
  return nullptr;
}

// Returns an OK result if every provided target password field is managed as
// a password input element in `driver`'s frame, or an error result listing
// the invalid fields.
mojom::ActionResultPtr ValidateTargetPasswordFields(
    password_manager::ContentPasswordManagerDriver* driver,
    autofill::FieldRendererId current_password_field_id,
    autofill::FieldRendererId new_password_field_id,
    autofill::FieldRendererId confirm_new_password_field_id) {
  CHECK(driver && driver->IsRenderFrameHostSupported());
  std::vector<std::string> invalid_fields;
  for (auto [label, renderer_id] :
       {std::pair{"current_password_field", current_password_field_id},
        std::pair{"new_password_field", new_password_field_id},
        std::pair{"confirm_new_password_field",
                  confirm_new_password_field_id}}) {
    if (!renderer_id) {
      continue;
    }
    if (!FindPasswordFormManagerForField(driver, renderer_id)) {
      invalid_fields.push_back(
          absl::StrFormat("%s (id %v)", label, renderer_id.value()));
    }
  }
  if (!invalid_fields.empty()) {
    // TODO(crbug.com/584683291): Introduce a dedicated ActionResultCode for
    // target fields that are not managed password inputs.
    return MakeResult(mojom::ActionResultCode::kFormFillingUnknownAutofillError,
                      /*requires_page_stabilization=*/false,
                      absl::StrFormat("Target field is not a valid password "
                                      "field: %s.",
                                      base::JoinString(invalid_fields, ", ")));
  }
  return MakeOkResult();
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
  if (!current_password_field_.has_value() &&
      !new_password_field_.has_value() &&
      !confirm_new_password_field_.has_value()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                   /*requires_page_stabilization=*/false,
                   "At least one password field must be provided."));
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

  autofill::LocalFrameToken target_frame_token;
  mojom::ActionResultPtr extract_field_ids_result =
      ExtractFieldIds(last_observation, target_frame_token);
  if (!IsOk(*extract_field_ids_result)) {
    return extract_field_ids_result;
  }

  content::RenderFrameHost* rfh =
      autofill::FindRenderFrameHostByToken(*web_contents, target_frame_token);
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
  driver_ = driver->AsWeakPtrImpl();
  mojom::ActionResultPtr validate_target_password_fields_result =
      ValidateTargetPasswordFields(driver,
                                   current_password_field_id_.renderer_id,
                                   new_password_field_id_.renderer_id,
                                   confirm_new_password_field_id_.renderer_id);
  if (!IsOk(*validate_target_password_fields_result)) {
    return validate_target_password_fields_result;
  }

  return MakeOkResult();
}

void ChangePasswordTool::Invoke(ToolCallback callback) {
  CHECK(driver_);
  // Unset (null) or missing `FieldRendererId`s are ignored by the renderer;
  // `FillChangePasswordForm` fills whichever fields resolve to valid input
  // elements and returns `std::nullopt` only if none of them can be filled.
  // TODO(crbug.com/574401623): check actor_login::IsValidFrameAndOriginToFill
  driver_->FillChangePasswordForm(
      current_password_field_id_.renderer_id,
      new_password_field_id_.renderer_id,
      confirm_new_password_field_id_.renderer_id,
      /*old_password=*/
      password_manager::PasswordString(u"dummy_current_password"),
      /*new_password=*/password_manager::PasswordString(u"dummy_new_password"),
      mojo::WrapCallbackWithDefaultInvokeIfNotRun(
          base::BindOnce(&ChangePasswordTool::OnChangePasswordFormFilled,
                         weak_factory_.GetWeakPtr(), std::move(callback)),
          std::nullopt));
}

mojom::ActionResultPtr ChangePasswordTool::ExtractFieldIds(
    const optimization_guide::proto::AnnotatedPageContent* last_observation,
    autofill::LocalFrameToken& target_frame_token) {
  for (auto [target, field_id] :
       {std::pair{&current_password_field_, &current_password_field_id_},
        std::pair{&new_password_field_, &new_password_field_id_},
        std::pair{&confirm_new_password_field_,
                  &confirm_new_password_field_id_}}) {
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
    // Password form filling is scoped to a single RenderFrameHost, so all
    // targeted password fields must belong to the same frame.
    if (target_frame_token && field_id->frame_token != target_frame_token) {
      return MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                        /*requires_page_stabilization=*/false,
                        "All password fields must belong to the same frame.");
    }
    target_frame_token = field_id->frame_token;
  }

  if ((current_password_field_id_ &&
       (current_password_field_id_ == new_password_field_id_ ||
        current_password_field_id_ == confirm_new_password_field_id_)) ||
      (new_password_field_id_ &&
       new_password_field_id_ == confirm_new_password_field_id_)) {
    return MakeResult(mojom::ActionResultCode::kArgumentsInvalid,
                      /*requires_page_stabilization=*/false,
                      "Password field targets must be distinct.");
  }
  return MakeOkResult();
}

void ChangePasswordTool::OnChangePasswordFormFilled(
    ToolCallback callback,
    // TODO(crbug.com/580856538): if a page has password inputs that do not
    // share the same <form> element (or one is inside a <form> and another is
    // unowned), `FillChangePasswordForm()` only returns `FormData` which
    // contains part of what was actually filled.
    const std::optional<autofill::FormData>& form_data) {
  // TODO(crbug.com/580865855): Tab & frame liveness checks. Handle general
  // form-filling error reporting when a tab/frame is destroyed while
  // `FillChangePasswordForm()` is in flight (returning `kTabWentAway` /
  // `kFrameWentAway` instead of `kFormFillingUnknownAutofillError`)
  if (!form_data.has_value()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kFormFillingUnknownAutofillError,
                   /*requires_page_stabilization=*/false,
                   "Found no eligible form to fill."));
    return;
  }

  std::vector<std::string> filled_parts;
  std::vector<std::string> unfilled_parts;
  for (auto [label, renderer_id] :
       {std::pair{"old password", current_password_field_id_.renderer_id},
        std::pair{"new generated password", new_password_field_id_.renderer_id},
        std::pair{"confirmation password",
                  confirm_new_password_field_id_.renderer_id}}) {
    if (!renderer_id) {
      continue;
    }
    std::string part =
        absl::StrFormat("%s into field with id %v", label, renderer_id.value());
    if (WasFieldFilled(*form_data, renderer_id)) {
      filled_parts.push_back(std::move(part));
    } else {
      unfilled_parts.push_back(std::move(part));
    }
  }

  if (filled_parts.empty()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kFormFillingUnknownAutofillError,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("Failed to fill %s.",
                                   base::JoinString(unfilled_parts, " and "))));
    return;
  }

  std::string message =
      absl::StrFormat("Filled %s.", base::JoinString(filled_parts, " and "));
  if (!unfilled_parts.empty()) {
    PostResponseTask(
        std::move(callback),
        MakeResult(mojom::ActionResultCode::kFormFillingUnknownAutofillError,
                   /*requires_page_stabilization=*/false,
                   absl::StrFormat("%s Failed to fill %s.", message,
                                   base::JoinString(unfilled_parts, " and "))));
    return;
  }
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

  PostResponseTask(std::move(callback),
                   MakeOkResultWithMessage(
                       /*requires_page_stabilization=*/true, message));
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
