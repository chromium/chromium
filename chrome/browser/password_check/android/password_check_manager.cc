// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include <string_view>

#include "base/feature_list.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_check/android/password_check_bridge.h"
#include "chrome/browser/signin/identity_manager_factory.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/password_manager/core/browser/leak_detection/leak_detection_check_impl.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_sync_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/well_known_change_password/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

using password_manager::PasswordForm;
using PasswordCheckUIStatus = password_manager::PasswordCheckUIStatus;
using State = password_manager::BulkLeakCheckService::State;
using SyncState = password_manager::sync_util::SyncState;
using CredentialUIEntry = password_manager::CredentialUIEntry;
using CredentialFacet = password_manager::CredentialFacet;
using CompromisedCredentialForUI =
    PasswordCheckManager::CompromisedCredentialForUI;

CompromisedCredentialForUI::CompromisedCredentialForUI(
    const CredentialUIEntry& credential)
    : CredentialUIEntry(credential) {}

CompromisedCredentialForUI::CompromisedCredentialForUI(
    const CompromisedCredentialForUI& other) = default;
CompromisedCredentialForUI::CompromisedCredentialForUI(
    CompromisedCredentialForUI&& other) = default;
CompromisedCredentialForUI& CompromisedCredentialForUI::operator=(
    const CompromisedCredentialForUI& other) = default;
CompromisedCredentialForUI& CompromisedCredentialForUI::operator=(
    CompromisedCredentialForUI&& other) = default;
CompromisedCredentialForUI::~CompromisedCredentialForUI() = default;

PasswordCheckManager::PasswordCheckManager(Profile* profile, Observer* observer)
    : observer_(observer), profile_(profile) {
  observed_saved_passwords_presenter_.Observe(&saved_passwords_presenter_);
  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);
  observed_bulk_leak_check_service_.Observe(
      BulkLeakCheckServiceFactory::GetForProfile(profile));

  // Instructs the presenter and provider to initialize and build their caches.
  // This will soon after invoke OnCompromisedCredentialsChanged(). Calls to
  // GetCompromisedCredentials() that might happen until then will return an
  // empty list.
  saved_passwords_presenter_.Init();
}

PasswordCheckManager::~PasswordCheckManager() = default;

void PasswordCheckManager::StartCheck() {
  if (!IsPreconditionFulfilled(kAll)) {
    was_start_requested_ = true;
    return;
  }

  // The request is being handled, so reset the boolean.
  was_start_requested_ = false;
  is_check_running_ = true;
  progress_ = std::make_unique<PasswordCheckProgress>();
  for (const auto& password : saved_passwords_presenter_.GetSavedPasswords())
    progress_->IncrementCounts(password);
  observer_->OnPasswordCheckProgressChanged(progress_->already_processed(),
                                            progress_->remaining_in_queue());
  bulk_leak_check_service_adapter_.StartBulkLeakCheck(
      password_manager::LeakDetectionInitiator::kBulkSyncedPasswordsCheck);
}

void PasswordCheckManager::StopCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
}

base::Time PasswordCheckManager::GetLastCheckTimestamp() {
  return base::Time::FromSecondsSinceUnixEpoch(profile_->GetPrefs()->GetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted));
}

int PasswordCheckManager::GetCompromisedCredentialsCount() const {
  return insecure_credentials_manager_.GetInsecureCredentialEntries().size();
}

int PasswordCheckManager::GetSavedPasswordsCount() const {
  return saved_passwords_presenter_.GetSavedPasswords().size();
}

std::vector<CompromisedCredentialForUI>
PasswordCheckManager::GetCompromisedCredentials() const {
  std::vector<CredentialUIEntry> credentials =
      insecure_credentials_manager_.GetInsecureCredentialEntries();
  std::vector<CompromisedCredentialForUI> ui_credentials;
  ui_credentials.reserve(credentials.size());
  for (const auto& credential : credentials) {
    ui_credentials.push_back(MakeUICredential(credential));
  }
  return ui_credentials;
}

void PasswordCheckManager::UpdateCredential(
    const password_manager::CredentialUIEntry& credential,
    std::string_view new_password) {
  CredentialUIEntry updated_credential = credential;
  updated_credential.password = base::UTF8ToUTF16(new_password);
  saved_passwords_presenter_.EditSavedCredentials(credential,
                                                  updated_credential);
}

void PasswordCheckManager::OnEditCredential(
    const password_manager::CredentialUIEntry& credential,
    const base::android::JavaParamRef<jobject>& context) {
  std::vector<password_manager::PasswordForm> forms =
      saved_passwords_presenter_.GetCorrespondingPasswordForms(credential);
  if (forms.empty() || credential_edit_bridge_)
    return;

  bool is_using_account_store = forms[0].IsUsingAccountStore();

  credential_edit_bridge_ = CredentialEditBridge::MaybeCreate(
      password_manager::CredentialUIEntry(forms[0]),
      CredentialEditBridge::IsInsecureCredential(true),
      GetUsernamesForRealm(saved_passwords_presenter_.GetSavedCredentials(),
                           credential.GetFirstSignonRealm(),
                           is_using_account_store),
      &saved_passwords_presenter_,
      base::BindOnce(&PasswordCheckManager::OnEditUIDismissed,
                     weak_ptr_factory_.GetWeakPtr()),
      context);
}

void PasswordCheckManager::RemoveCredential(
    const password_manager::CredentialUIEntry& credential) {
  saved_passwords_presenter_.RemoveCredential(credential);
}

PasswordCheckManager::PasswordCheckProgress::PasswordCheckProgress() = default;
PasswordCheckManager::PasswordCheckProgress::~PasswordCheckProgress() = default;

void PasswordCheckManager::PasswordCheckProgress::IncrementCounts(
    const password_manager::CredentialUIEntry& password) {
  ++remaining_in_queue_;
  ++counts_[password];
}

void PasswordCheckManager::PasswordCheckProgress::OnProcessed(
    const password_manager::LeakCheckCredential& credential) {
  auto it = counts_.find(credential);
  const int num_matching = it != counts_.end() ? it->second : 0;
  already_processed_ += num_matching;
  remaining_in_queue_ -= num_matching;
}

void PasswordCheckManager::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  size_t passwords_count =
      saved_passwords_presenter_.GetSavedPasswords().size();

  if (!IsPreconditionFulfilled(kSavedPasswordsAvailable)) {
    observer_->OnSavedPasswordsFetched(passwords_count);
    FulfillPrecondition(kSavedPasswordsAvailable);
  }

  if (passwords_count == 0) {
    observer_->OnPasswordCheckStatusChanged(
        PasswordCheckUIStatus::kErrorNoPasswords);
    was_start_requested_ = false;
    return;
  }

  if (was_start_requested_) {
    StartCheck();
  }
}

void PasswordCheckManager::OnInsecureCredentialsChanged() {
  FulfillPrecondition(kKnownCredentialsFetched);
  observer_->OnCompromisedCredentialsChanged(GetCompromisedCredentialsCount());
}

void PasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Save the time at which the last successful check finished.
    profile_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().InSecondsFSinceUnixEpoch());
  }

  if (state != State::kRunning) {
    progress_.reset();
    is_check_running_ = false;
    if (saved_passwords_presenter_.GetSavedPasswords().empty()) {
      observer_->OnPasswordCheckStatusChanged(
          PasswordCheckUIStatus::kErrorNoPasswords);
      return;
    }
  }

  observer_->OnPasswordCheckStatusChanged(GetUIStatus(state));
}

void PasswordCheckManager::OnCredentialDone(
    const password_manager::LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (progress_) {
    progress_->OnProcessed(credential);
    observer_->OnPasswordCheckProgressChanged(progress_->already_processed(),
                                              progress_->remaining_in_queue());
  }
  if (is_leaked) {
    // TODO(crbug.com/40134591): Trigger single-credential update.
    insecure_credentials_manager_.SaveInsecureCredential(
        credential, password_manager::TriggerBackendNotification(false));
  }
}

CompromisedCredentialForUI PasswordCheckManager::MakeUICredential(
    const CredentialUIEntry& credential) const {
  CompromisedCredentialForUI ui_credential(credential);

  std::vector<CredentialFacet> credential_facets;
  CredentialFacet credential_facet;
  credential_facet.display_name = credential.GetDisplayName();
  credential_facet.url = credential.GetURL();
  credential_facet.signon_realm = credential.GetFirstSignonRealm();
  credential_facet.affiliated_web_realm = credential.GetAffiliatedWebRealm();

  auto facet = affiliations::FacetURI::FromPotentiallyInvalidSpec(
      credential.GetFirstSignonRealm());

  if (facet.IsValidAndroidFacetURI()) {
    ui_credential.package_name = facet.android_package_name();

    if (credential.GetDisplayName().empty()) {
      // In case no affiliation information could be obtained show the
      // formatted package name to the user.
      ui_credential.display_origin = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
    } else {
      ui_credential.display_origin =
          base::UTF8ToUTF16(credential.GetDisplayName());
    }
    // In case no affiliated_web_realm could be obtained we should not have an
    // associated url for android credential.
    credential_facet.url = credential.GetAffiliatedWebRealm().empty()
                               ? GURL()
                               : GURL(credential.GetAffiliatedWebRealm());

  } else {
    ui_credential.display_origin = url_formatter::FormatUrl(
        credential.GetURL().DeprecatedGetOriginAsURL(),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
    ui_credential.change_password_url =
        password_manager::CreateChangePasswordUrl(credential_facet.url).spec();
  }

  ui_credential.display_username =
      password_manager::ToUsernameString(credential.username);

  credential_facets.push_back(std::move(credential_facet));
  ui_credential.facets = std::move(credential_facets);
  return ui_credential;
}

void PasswordCheckManager::OnBulkCheckServiceShutDown() {
  DCHECK(observed_bulk_leak_check_service_.IsObservingSource(
      BulkLeakCheckServiceFactory::GetForProfile(profile_)));
  observed_bulk_leak_check_service_.Reset();
}

PasswordCheckUIStatus PasswordCheckManager::GetUIStatus(State state) const {
  switch (state) {
    case State::kIdle:
      return PasswordCheckUIStatus::kIdle;
    case State::kRunning:
      return PasswordCheckUIStatus::kRunning;
    case State::kSignedOut:
      return PasswordCheckUIStatus::kErrorSignedOut;
    case State::kNetworkError:
      return PasswordCheckUIStatus::kErrorOffline;
    case State::kQuotaLimit:
      return CanUseAccountCheck()
                 ? PasswordCheckUIStatus::kErrorQuotaLimitAccountCheck
                 : PasswordCheckUIStatus::kErrorQuotaLimit;
    case State::kCanceled:
      return PasswordCheckUIStatus::kCanceled;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return PasswordCheckUIStatus::kErrorUnknown;
  }
  NOTREACHED_IN_MIGRATION();
  return PasswordCheckUIStatus::kIdle;
}

bool PasswordCheckManager::CanUseAccountCheck() const {
  SyncState sync_state = password_manager::sync_util::GetPasswordSyncState(
      SyncServiceFactory::GetForProfile(profile_));
  switch (sync_state) {
    case SyncState::kNotActive:
      ABSL_FALLTHROUGH_INTENDED;
    case SyncState::kActiveWithCustomPassphrase:
      return false;

    case SyncState::kActiveWithNormalEncryption:
      return true;
  }
}

bool PasswordCheckManager::IsPreconditionFulfilled(
    CheckPreconditions condition) const {
  return (fulfilled_preconditions_ & condition) == condition;
}

void PasswordCheckManager::FulfillPrecondition(CheckPreconditions condition) {
  fulfilled_preconditions_ |= condition;
  if (was_start_requested_)
    StartCheck();
}

void PasswordCheckManager::ResetPrecondition(CheckPreconditions condition) {
  fulfilled_preconditions_ &= ~condition;
}

void PasswordCheckManager::OnEditUIDismissed() {
  credential_edit_bridge_.reset();
}

bool PasswordCheckManager::HasAccountForRequest() {
  signin::IdentityManager* identity_manager =
      IdentityManagerFactory::GetForProfile(profile_);
  return password_manager::LeakDetectionCheckImpl::HasAccountForRequest(
      identity_manager);
}
