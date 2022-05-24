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
#include "chrome/grit/generated_resources.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/web_contents.h"
#include "ui/base/l10n/l10n_util.h"
#include "url/gurl.h"

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
  // TODO(crbug.com/1322387): Remove the "translateable='false'" tag in the
  // string definition once all strings are final.
  AssistantOnboardingInformation info;
  info.title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_TITLE);
  info.description = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_DESCRIPTION);
  info.consent_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_CONSENT_TEXT);
  info.learn_more_title = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_LEARN_MORE);
  info.button_cancel_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_BUTTON_CANCEL_TEXT);
  info.button_accept_text = l10n_util::GetStringUTF16(
      IDS_AUTOFILL_ASSISTANT_PASSWORD_CHANGE_ONBOARDING_BUTTON_ACCEPT_TEXT);

  // TODO(crbug.com/1322387): Update link so that it also applies to Desktop.
  info.learn_more_url = GURL(
      "https://support.google.com/assistant/answer/"
      "9201753?visit_id=637880404267471228-1286648363&p=fast_checkout&rd=1");

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

// static
std::unique_ptr<ApcOnboardingCoordinator> ApcOnboardingCoordinator::Create(
    content::WebContents* web_contents) {
  return std::make_unique<ApcOnboardingCoordinatorImpl>(web_contents);
}
