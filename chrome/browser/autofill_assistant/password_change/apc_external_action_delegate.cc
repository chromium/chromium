// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

#include <string>
#include <vector>

#include "base/bind.h"
#include "base/logging.h"
#include "base/memory/weak_ptr.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"
#include "chrome/grit/generated_resources.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

using autofill_assistant::password_change::GenericPasswordChangeSpecification;

// TODO(crbug.com/1324089): Implement once the side panel and
// UpdateDesktopSideAction are available.
ApcExternalActionDelegate::ApcExternalActionDelegate(
    AssistantDisplayDelegate* display_delegate)
    : display_delegate_(display_delegate) {
  DCHECK(display_delegate_);
}

ApcExternalActionDelegate::~ApcExternalActionDelegate() = default;

void ApcExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const autofill_assistant::external::Result& result)>
        end_action_callback) {
  end_action_callback_ = std::move(end_action_callback);
  start_dom_checks_callback_ = std::move(start_dom_checks_callback);

  GenericPasswordChangeSpecification spec;
  if (!spec.ParseFromString(action.info().action_payload())) {
    DLOG(ERROR) << "unable to parse GenericPasswordChangeSpecification";
    EndAction(false);
    return;
  }

  switch (spec.specification_case()) {
    case GenericPasswordChangeSpecification::SpecificationCase::kBasePrompt:
      HandleBasePrompt(spec.base_prompt());
      break;
    case GenericPasswordChangeSpecification::SpecificationCase::
        kUseGeneratedPasswordPrompt:
      HandleGeneratedPasswordPrompt(spec.use_generated_password_prompt());
      break;
    case GenericPasswordChangeSpecification::SpecificationCase::
        kUpdateSidePanel:
      HandleUpdateSidePanel(spec.update_side_panel());
      break;
    case GenericPasswordChangeSpecification::SpecificationCase::
        SPECIFICATION_NOT_SET:
      DLOG(ERROR) << "unknown password change action";
      EndAction(false);
      break;
  }
}

void ApcExternalActionDelegate::SetupDisplay() {
  Show(PasswordChangeRunDisplay::Create(GetWeakPtr(), display_delegate_.get()));
}

void ApcExternalActionDelegate::OnInterruptStarted() {
  DCHECK(!model_before_interrupt_.has_value());
  model_before_interrupt_ = model_;

  // Reset the current model. The progress step remains the same, so we do not
  // touch it.
  SetTitle(std::u16string());
  SetDescription(std::u16string());
}

void ApcExternalActionDelegate::OnInterruptFinished() {
  DCHECK(model_before_interrupt_.has_value());

  // Restore the state from prior to the interrupt. We reset the model
  // by calling the setters instead of just restoring state to ensure that
  // the view is informed about the updates.
  PasswordChangeRunController::Model model = model_before_interrupt_.value();
  SetTopIcon(model.top_icon);
  SetTitle(model.title);
  SetDescription(model.description);

  model_before_interrupt_.reset();
}

// PasswordChangeRunController

void ApcExternalActionDelegate::SetTopIcon(
    autofill_assistant::password_change::TopIcon top_icon) {
  DCHECK(password_change_run_display_);
  model_.top_icon = top_icon;
  password_change_run_display_->SetTopIcon(top_icon);
}

void ApcExternalActionDelegate::SetTitle(const std::u16string& title) {
  DCHECK(password_change_run_display_);
  model_.title = title;
  password_change_run_display_->SetTitle(title);
}

void ApcExternalActionDelegate::SetDescription(
    const std::u16string& description) {
  DCHECK(password_change_run_display_);
  model_.description = description;
  password_change_run_display_->SetDescription(description);
}

void ApcExternalActionDelegate::SetProgressBarStep(
    autofill_assistant::password_change::ProgressStep progress_step) {
  DCHECK(password_change_run_display_);
  model_.progress_step = progress_step;
  password_change_run_display_->SetProgressBarStep(progress_step);
}

void ApcExternalActionDelegate::ShowBasePrompt(
    const autofill_assistant::password_change::BasePromptSpecification&
        base_prompt) {
  // Showing the prompt will override the description, so set the model value
  // to empty to ensure that it reflects the state of the view.
  model_.description = std::u16string();

  std::vector<PasswordChangeRunDisplay::PromptChoice> choices;
  choices.reserve(base_prompt.choices_size());
  base_prompt_return_values_.clear();
  base_prompt_return_values_.reserve(base_prompt.choices_size());

  for (const auto& choice : base_prompt.choices()) {
    choices.push_back(PasswordChangeRunDisplay::PromptChoice{
        .text = base::UTF8ToUTF16(choice.text()),
        .highlighted = choice.highlighted()});
    base_prompt_return_values_.push_back(choice.tag());
  }

  SetTitle(base::UTF8ToUTF16(base_prompt.title()));
  password_change_run_display_->ShowBasePrompt(choices);
}

void ApcExternalActionDelegate::OnBasePromptChoiceSelected(
    size_t choice_index) {
  password_change_run_display_->ClearPrompt();

  // If no `output_key` is specified, only signal that the prompt action was
  // successfully executed.
  if (!base_prompt_should_send_payload_) {
    EndAction(true);
    return;
  }

  DCHECK(choice_index < base_prompt_return_values_.size());
  autofill_assistant::password_change::BasePromptSpecification::Result result;
  result.set_selected_tag(base_prompt_return_values_[choice_index]);

  std::string serialized_result;
  if (!result.SerializeToString(&serialized_result)) {
    DLOG(ERROR) << "unable to base prompt result";
    EndAction(false);
    return;
  }
  EndAction(true, std::move(serialized_result));
}

void ApcExternalActionDelegate::ShowUseGeneratedPasswordPrompt(
    const autofill_assistant::password_change::
        UseGeneratedPasswordPromptSpecification& password_prompt,
    const std::u16string& generated_password) {
  // Showing the prompt will override both the title and the description. Since
  // they cannot be reconstructed from the model due to the additional field
  // for the password, we clear the model.
  model_.title = std::u16string();
  model_.description = std::u16string();
  password_change_run_display_->ShowUseGeneratedPasswordPrompt(
      base::UTF8ToUTF16(password_prompt.title()), generated_password,
      base::UTF8ToUTF16(password_prompt.description()),
      PasswordChangeRunDisplay::PromptChoice{
          .text = base::UTF8ToUTF16(
              password_prompt.manual_password_choice().text()),
          .highlighted =
              password_prompt.manual_password_choice().highlighted()},
      PasswordChangeRunDisplay::PromptChoice{
          .text = base::UTF8ToUTF16(
              password_prompt.generated_password_choice().text()),
          .highlighted =
              password_prompt.generated_password_choice().highlighted()});
}

void ApcExternalActionDelegate::OnGeneratedPasswordSelected(
    bool generated_password_accepted) {
  password_change_run_display_->ClearPrompt();
  SetTitle(std::u16string());

  autofill_assistant::password_change::UseGeneratedPasswordPromptSpecification::
      Result result;
  result.set_generated_password_accepted(generated_password_accepted);

  std::string serialized_result;
  if (!result.SerializeToString(&serialized_result)) {
    DLOG(ERROR) << "unable to base prompt result";
    EndAction(false);
    return;
  }
  EndAction(true, std::move(serialized_result));
}

void ApcExternalActionDelegate::ShowStartingScreen(const GURL& url) {
  SetTopIcon(
      autofill_assistant::password_change::TopIcon::TOP_ICON_UNSPECIFIED);
  SetProgressBarStep(
      autofill_assistant::password_change::ProgressStep::PROGRESS_STEP_START);
  SetTitle(l10n_util::GetStringFUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_STARTING_SCREEN_TITLE,
      base::UTF8ToUTF16(url.host_piece())));
  SetDescription(std::u16string());
}

void ApcExternalActionDelegate::Show(
    base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display) {
  password_change_run_display_ = password_change_run_display;
  password_change_run_display_->Show();
}

void ApcExternalActionDelegate::EndAction(bool success,
                                          std::string serialized_result) {
  autofill_assistant::external::Result result;
  result.set_success(success);
  // Only set a payload for a non-empty serialized result to avoid triggering
  // payload processing.
  if (!serialized_result.empty()) {
    result.mutable_result_info()->set_result_payload(serialized_result);
  }

  std::move(end_action_callback_).Run(std::move(result));
}

void ApcExternalActionDelegate::HandleBasePrompt(
    const autofill_assistant::password_change::BasePromptSpecification&
        specification) {
  base_prompt_should_send_payload_ = specification.has_output_key();

  // TODO(crbug.com/1331202): Set up DOM checking and matching of DOM conditions
  // to return values.

  ShowBasePrompt(specification);
}

void ApcExternalActionDelegate::HandleGeneratedPasswordPrompt(
    const autofill_assistant::password_change::
        UseGeneratedPasswordPromptSpecification& specification) {
  // TODO(crbug.com/1331202): Replace this hardcoded password with the real
  // generated one.
  ShowUseGeneratedPasswordPrompt(specification, u"verySecretPassword123");
}

void ApcExternalActionDelegate::HandleUpdateSidePanel(
    const autofill_assistant::password_change::UpdateSidePanelSpecification&
        specification) {
  EndAction(false);
}

base::WeakPtr<PasswordChangeRunController>
ApcExternalActionDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
