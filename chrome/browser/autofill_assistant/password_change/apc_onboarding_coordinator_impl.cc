// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/autofill_assistant/password_change/apc_onboarding_coordinator_impl.h"

#include <memory>

#include "base/bind.h"
#include "base/callback.h"
#include "base/containers/flat_set.h"
#include "base/memory/weak_ptr.h"
#include "chrome/browser/consent_auditor/consent_auditor_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_controller.h"
#include "chrome/browser/ui/autofill_assistant/password_change/assistant_onboarding_prompt.h"
#include "components/autofill_assistant/browser/public/prefs.h"
#include "components/consent_auditor/consent_auditor.h"
#include "components/prefs/pref_service.h"
#include "components/signin/public/base/consent_level.h"
#include "components/signin/public/identity_manager/account_info.h"
#include "components/signin/public/identity_manager/identity_manager.h"
#include "components/sync/protocol/user_consent_types.pb.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/navigation_entry.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/web_contents.h"
#include "content/public/browser/web_contents_observer.h"
#include "net/base/registry_controlled_domains/registry_controlled_domain.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

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
      web_contents_->GetController().GetPendingEntry();
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

void ApcOnboardingCoordinatorImpl::RevokeConsent(
    const std::vector<int>& description_grd_ids) {
  sync_pb::UserConsentTypes::AutofillAssistantConsent consent;
  GetPrefs()->SetBoolean(autofill_assistant::prefs::kAutofillAssistantConsent,
                         false);
  consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                         UserConsentTypes_ConsentStatus_NOT_GIVEN);
  consent.mutable_description_grd_ids()->Assign(description_grd_ids.cbegin(),
                                                description_grd_ids.cend());
  WriteToConsentAuditor(consent);
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
  return GetPrefs()->GetBoolean(
      autofill_assistant::prefs::kAutofillAssistantConsent);
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

void ApcOnboardingCoordinatorImpl::OnControllerResponseReceived(
    bool success,
    absl::optional<int> confirmation_grd_id,
    const std::vector<int>& description_grd_ids) {
  if (success) {
    GetPrefs()->SetBoolean(autofill_assistant::prefs::kAutofillAssistantConsent,
                           true);
    CHECK(confirmation_grd_id.has_value());
    RecordConsentGiven(confirmation_grd_id.value(), description_grd_ids);
  }
  std::move(callback_).Run(success);
}

void ApcOnboardingCoordinatorImpl::RecordConsentGiven(
    int confirmation_grd_id,
    const std::vector<int>& description_grd_ids) {
  sync_pb::UserConsentTypes::AutofillAssistantConsent consent;

  // The only accepted resource ids are those contained in the model. Otherwise,
  // something is going seriously wrong and we should stop Chrome from sending
  // incorrect consent data.
  const AssistantOnboardingInformation model = CreateOnboardingInformation();
  CHECK_EQ(confirmation_grd_id, model.button_accept_text_id);
  consent.set_confirmation_grd_id(confirmation_grd_id);

  const base::flat_set<int> acceptable_ids = {
      model.title_id, model.description_id, model.consent_text_id,
      model.learn_more_title_id};
  CHECK_EQ(acceptable_ids.size(), description_grd_ids.size());
  for (int id : description_grd_ids) {
    CHECK(acceptable_ids.contains(id));
    consent.add_description_grd_ids(id);
  }
  consent.set_status(sync_pb::UserConsentTypes::ConsentStatus::
                         UserConsentTypes_ConsentStatus_GIVEN);

  WriteToConsentAuditor(consent);
}

void ApcOnboardingCoordinatorImpl::WriteToConsentAuditor(
    const sync_pb::UserConsentTypes::AutofillAssistantConsent& consent) {
  Profile* profile =
      Profile::FromBrowserContext(web_contents_->GetBrowserContext());
  ConsentAuditorFactory::GetForProfile(profile)->RecordAutofillAssistantConsent(
      IdentityManagerFactory::GetForProfile(profile)->GetPrimaryAccountId(
          signin::ConsentLevel::kSignin),
      consent);
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
