// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "chrome/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"

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

  // If there is an ongoing navigation to a different domain, then the
  // `WebContentsModalDialogManager` will close the onboarding dialog
  // automatically on finishing the navigation. To avoid this, we check whether
  // such a navigation is ongoing and delay opening the dialog until completes.
  content::NavigationEntry* entry =
      web_contents_->GetController().GetActiveEntry();
  if (entry &&
      !net::registry_controlled_domains::SameDomainOrHost(
          web_contents_->GetLastCommittedURL(), entry->GetURL(),
          net::registry_controlled_domains::INCLUDE_PRIVATE_REGISTRIES)) {
    dialog_launcher_ = std::make_unique<DialogLauncher>(
        web_contents_,
        base::BindOnce(&ApcOnboardingCoordinatorImpl::OpenOnboardingDialog,
                       base::Unretained(this)));
  } else {
    // Otherwise, launch directly.
    OpenOnboardingDialog();
  }
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

void ApcOnboardingCoordinatorImpl::OpenOnboardingDialog() {
  // Always invalidate the dialog launcher.
  dialog_launcher_.reset();

  dialog_controller_ = CreateOnboardingController(
      ApcOnboardingCoordinator::CreateOnboardingInformation());
  dialog_controller_->Show(
      CreateOnboardingPrompt(dialog_controller_->GetWeakPtr()),
      base::BindOnce(
          &ApcOnboardingCoordinatorImpl::OnControllerResponseReceived,
          base::Unretained(this)));
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

ApcOnboardingCoordinatorImpl::DialogLauncher::DialogLauncher(
    content::WebContents* web_contents,
    base::OnceClosure open_dialog)
    : content::WebContentsObserver(web_contents),
      open_dialog_(std::move(open_dialog)) {}

ApcOnboardingCoordinatorImpl::DialogLauncher::~DialogLauncher() = default;

void ApcOnboardingCoordinatorImpl::DialogLauncher::DidFinishNavigation(
    content::NavigationHandle* navigation_handle) {
  if (!navigation_handle->IsInPrimaryMainFrame() ||
      !navigation_handle->HasCommitted()) {
    return;
  }
  std::move(open_dialog_).Run();
}
