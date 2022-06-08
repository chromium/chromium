// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_external_action_delegate.h"

#include <string>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/autofill_assistant/password_change/proto/extensions.pb.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_display_delegate.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/password_change_run_display.h"

// TODO(crbug.com/1324089): Implement once the side panel and
// UpdateDesktopSideAction are available.
ApcExternalActionDelegate::ApcExternalActionDelegate(
    AssistantDisplayDelegate* display_delegate)
    : display_delegate_(display_delegate) {
  DCHECK(display_delegate_);
}

ApcExternalActionDelegate::~ApcExternalActionDelegate() = default;

void ApcExternalActionDelegate::OnActionRequested(
    const autofill_assistant::external::Action& action_info,
    base::OnceCallback<void(DomUpdateCallback)> start_dom_checks_callback,
    base::OnceCallback<void(const autofill_assistant::external::Result& result)>
        end_action_callback) {
  autofill_assistant::external::Result result;
  result.set_success(true);
  std::move(end_action_callback).Run(result);
}

void ApcExternalActionDelegate::SetupDisplay() {
  Show(PasswordChangeRunDisplay::Create(GetWeakPtr(), display_delegate_.get()));
}

void ApcExternalActionDelegate::OnInterruptStarted() {}
void ApcExternalActionDelegate::OnInterruptFinished() {}

// PasswordChangeRunController
void ApcExternalActionDelegate::Show(
    base::WeakPtr<PasswordChangeRunDisplay> password_change_run_display) {
  password_change_run_display_ = password_change_run_display;
  password_change_run_display_->Show();
}

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
    const autofill_assistant::password_change::BasePrompt& base_prompt) {}

void ApcExternalActionDelegate::OnBasePromptOptionSelected(int option_index) {}

void ApcExternalActionDelegate::ShowSuggestedPasswordPrompt(
    const std::u16string& suggested_password) {}

void ApcExternalActionDelegate::OnSuggestedPasswordSelected(bool selected) {}

base::WeakPtr<PasswordChangeRunController>
ApcExternalActionDelegate::GetWeakPtr() {
  return weak_ptr_factory_.GetWeakPtr();
}
