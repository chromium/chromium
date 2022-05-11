// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/bind.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"

ApcOnboardingCoordinatorImpl::ApcOnboardingCoordinatorImpl(
    AssistantDisplayDelegate* display_delegate)
    : display_delegate_(display_delegate) {}

ApcOnboardingCoordinatorImpl::~ApcOnboardingCoordinatorImpl() = default;

void ApcOnboardingCoordinatorImpl::PerformOnboarding(Callback callback) {
  callback_ = std::move(callback);
  // Check preferences and see whether they are already set.
  if (IsOnboardingAlreadyAccepted()) {
    std::move(callback_).Run(true);
    return;
  }

  // If not, construct controller and view and wait for signal.
  // TODO(crbug.com/1322387): User proper consent texts.
  AssistantOnboardingInformation info;
  info.consent_caption = u"Do you give consent to use Autofill Assistant?";
  info.consent_text = u"This is what you agree to.";

  dialog_controller_ = CreateOnboardingController(info);
  dialog_controller_->Show(
      CreateOnboardingPrompt(dialog_controller_.get(), display_delegate_),
      base::BindOnce(
          &ApcOnboardingCoordinatorImpl::OnControllerResponseReceived,
          base::Unretained(this)));
}

std::unique_ptr<AssistantOnboardingController> CreateOnboardingController(
    const AssistantOnboardingInformation& onboarding_information) {
  return AssistantOnboardingController::Create(onboarding_information);
}

AssistantOnboardingPrompt* ApcOnboardingCoordinatorImpl::CreateOnboardingPrompt(
    AssistantOnboardingController* controller,
    AssistantDisplayDelegate* display_delegate) {
  return AssistantOnboardingPrompt::Create(controller, display_delegate);
}

bool ApcOnboardingCoordinatorImpl::IsOnboardingAlreadyAccepted() {
  // TODO(crbug.com/1322387): Check preference key.
  return false;
}

void ApcOnboardingCoordinatorImpl::OnControllerResponseReceived(bool success) {
  std::move(callback_).Run(success);
}
