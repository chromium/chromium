// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"

ApcOnboardingCoordinatorImpl::ApcOnboardingCoordinatorImpl(
    content::WebContents* web_contents)
    : web_contents_(web_contents) {}

ApcOnboardingCoordinatorImpl::~ApcOnboardingCoordinatorImpl() = default;

void ApcOnboardingCoordinatorImpl::PerformOnboarding(Callback callback) {
  callback_ = std::move(callback);
  // Check preferences and see whether they are already set.
  if (IsOnboardingAlreadyAccepted()) {
    std::move(callback_).Run(true);
    return;
  }

  // If not, construct controller and view and wait for signal.
  AssistantOnboardingInformation info =
      ApcOnboardingCoordinator::CreateOnboardingInformation();

  dialog_controller_ = CreateOnboardingController(info);
  dialog_controller_->Show(
      CreateOnboardingPrompt(dialog_controller_->GetWeakPtr()),
      base::BindOnce(
          &ApcOnboardingCoordinatorImpl::OnControllerResponseReceived,
          base::Unretained(this)));
}

std::unique_ptr<AssistantOnboardingController>
ApcOnboardingCoordinatorImpl::CreateOnboardingController(
    const AssistantOnboardingInformation& onboarding_information) {
  return AssistantOnboardingController::Create(onboarding_information,
                                               web_contents_);
}

base::WeakPtr<AssistantOnboardingPrompt>
ApcOnboardingCoordinatorImpl::CreateOnboardingPrompt(
    base::WeakPtr<AssistantOnboardingController> controller) {
  return AssistantOnboardingPrompt::Create(controller);
}

bool ApcOnboardingCoordinatorImpl::IsOnboardingAlreadyAccepted() {
  return GetPrefs()->GetBoolean(prefs::kAutofillAssistantOnDesktopEnabled);
}

void ApcOnboardingCoordinatorImpl::OnControllerResponseReceived(bool success) {
  if (success) {
    GetPrefs()->SetBoolean(prefs::kAutofillAssistantOnDesktopEnabled, true);
  }
  std::move(callback_).Run(success);
}

PrefService* ApcOnboardingCoordinatorImpl::GetPrefs() {
  return Profile::FromBrowserContext(web_contents_->GetBrowserContext())
      ->GetPrefs();
}
