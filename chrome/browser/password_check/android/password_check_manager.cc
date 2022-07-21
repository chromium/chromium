// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/password_check/android/password_check_manager.h"

#include "base/feature_list.h"
#include "base/strings/string_piece.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/password_check/android/password_check_bridge.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/grit/generated_resources.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_ui_utils.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/strings/grit/components_strings.h"
#include "components/url_formatter/url_formatter.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

using password_manager::PasswordForm;

std::u16string GetDisplayUsername(const std::u16string& username) {
  return username.empty()
             ? l10n_util::GetStringUTF16(IDS_PASSWORD_MANAGER_EMPTY_LOGIN)
             : username;
}

}  // namespace

using CredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;
using PasswordCheckUIStatus = password_manager::PasswordCheckUIStatus;
using State = password_manager::BulkLeakCheckService::State;
using SyncState = password_manager::SyncState;
using CredentialUIEntry = password_manager::CredentialUIEntry;
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
  insecure_credentials_manager_.Init();

  if (!ShouldFetchPasswordScripts()) {
    // Ensure that scripts are treated as initialized if they are unnecessary.
    FulfillPrecondition(kScriptsCachePrewarmed);
  }
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
  bulk_leak_check_service_adapter_.StartBulkLeakCheck();
}

void PasswordCheckManager::StopCheck() {
  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
}

base::Time PasswordCheckManager::GetLastCheckTimestamp() {
  return base::Time::FromDoubleT(profile_->GetPrefs()->GetDouble(
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
    const password_manager::CredentialView& credential,
    base::StringPiece new_password) {
  insecure_credentials_manager_.UpdateCredential(credential, new_password);
}

void PasswordCheckManager::OnEditCredential(
    const password_manager::CredentialView& credential,
    const base::android::JavaParamRef<jobject>& context,
    const base::android::JavaParamRef<jobject>& settings_launcher) {
  password_manager::SavedPasswordsPresenter::SavedPasswordsView forms =
      insecure_credentials_manager_.GetSavedPasswordsFor(credential);
  if (forms.empty() || credential_edit_bridge_)
    return;

  const PasswordForm form =
      insecure_credentials_manager_.GetSavedPasswordsFor(credential)[0];
  bool is_using_account_store = form.IsUsingAccountStore();

  credential_edit_bridge_ = CredentialEditBridge::MaybeCreate(
      password_manager::CredentialUIEntry(form),
      CredentialEditBridge::IsInsecureCredential(true),
      GetUsernamesForRealm(saved_passwords_presenter_.GetSavedCredentials(),
                           credential.signon_realm, is_using_account_store),
      &saved_passwords_presenter_,
      base::BindOnce(&PasswordCheckManager::OnEditUIDismissed,
                     weak_ptr_factory_.GetWeakPtr()),
      context, settings_launcher);
}

void PasswordCheckManager::RemoveCredential(
    const password_manager::CredentialView& credential) {
  insecure_credentials_manager_.RemoveCredential(credential);
}

PasswordCheckManager::PasswordCheckProgress::PasswordCheckProgress() = default;
PasswordCheckManager::PasswordCheckProgress::~PasswordCheckProgress() = default;

void PasswordCheckManager::PasswordCheckProgress::IncrementCounts(
    const password_manager::PasswordForm& password) {
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
    password_manager::SavedPasswordsPresenter::SavedPasswordsView passwords) {
  if (!IsPreconditionFulfilled(kSavedPasswordsAvailable)) {
    observer_->OnSavedPasswordsFetched(passwords.size());
    FulfillPrecondition(kSavedPasswordsAvailable);
  }

  if (passwords.empty()) {
    observer_->OnPasswordCheckStatusChanged(
        PasswordCheckUIStatus::kErrorNoPasswords);
    was_start_requested_ = false;
    return;
  }

  if (was_start_requested_) {
    StartCheck();
  }
}

void PasswordCheckManager::OnInsecureCredentialsChanged(
    password_manager::InsecureCredentialsManager::CredentialsView credentials) {
  if (AreScriptsRefreshed()) {
    FulfillPrecondition(kKnownCredentialsFetched);
  } else {
    credentials_count_to_notify_ = credentials.size();
  }
  observer_->OnCompromisedCredentialsChanged(credentials.size());
}

void PasswordCheckManager::OnStateChanged(State state) {
  if (state == State::kIdle && is_check_running_) {
    // Save the time at which the last successful check finished.
    profile_->GetPrefs()->SetDouble(
        password_manager::prefs::kLastTimePasswordCheckCompleted,
        base::Time::Now().ToDoubleT());
    profile_->GetPrefs()->SetTime(
        password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
        base::Time::Now());
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
    // TODO(crbug.com/1092444): Trigger single-credential update.
    insecure_credentials_manager_.SaveInsecureCredential(credential);
  }
}

CompromisedCredentialForUI PasswordCheckManager::MakeUICredential(
    const CredentialUIEntry& credential) const {
  CompromisedCredentialForUI ui_credential(credential);
  // UI is only be created after the list of available password check
  // scripts has been refreshed.
  DCHECK(AreScriptsRefreshed());
  auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
      credential.signon_realm);

  if (facet.IsValidAndroidFacetURI()) {
    ui_credential.package_name = facet.android_package_name();

    if (credential.app_display_name.empty()) {
      // In case no affiliation information could be obtained show the
      // formatted package name to the user.
      ui_credential.display_origin = l10n_util::GetStringFUTF16(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
    } else {
      ui_credential.display_origin =
          base::UTF8ToUTF16(credential.app_display_name);
    }
    // In case no affiliated_web_realm could be obtained we should not have an
    // associated url for android credential.
    ui_credential.url = credential.affiliated_web_realm.empty()
                            ? GURL::EmptyGURL()
                            : GURL(credential.affiliated_web_realm);

  } else {
    ui_credential.display_origin = url_formatter::FormatUrl(
        credential.url.DeprecatedGetOriginAsURL(),
        url_formatter::kFormatUrlOmitDefaults |
            url_formatter::kFormatUrlOmitHTTPS |
            url_formatter::kFormatUrlOmitTrivialSubdomains |
            url_formatter::kFormatUrlTrimAfterHost,
        base::UnescapeRule::SPACES, nullptr, nullptr, nullptr);
    ui_credential.change_password_url =
        password_manager::CreateChangePasswordUrl(ui_credential.url).spec();
  }

  ui_credential.display_username = GetDisplayUsername(credential.username);
  ui_credential.has_startable_script =
      !credential.username.empty() && ShouldFetchPasswordScripts() &&
      password_script_fetcher_->IsScriptAvailable(
          url::Origin::Create(ui_credential.url.DeprecatedGetOriginAsURL()));
  ui_credential.has_auto_change_button =
      ui_credential.has_startable_script &&
      base::FeatureList::IsEnabled(
          password_manager::features::kPasswordChangeInSettings);

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
  NOTREACHED();
  return PasswordCheckUIStatus::kIdle;
}

bool PasswordCheckManager::CanUseAccountCheck() const {
  SyncState sync_state = password_manager_util::GetPasswordSyncState(
      SyncServiceFactory::GetForProfile(profile_));
  switch (sync_state) {
    case SyncState::kNotSyncing:
      ABSL_FALLTHROUGH_INTENDED;
    case SyncState::kSyncingWithCustomPassphrase:
      return false;

    case SyncState::kSyncingNormalEncryption:
      ABSL_FALLTHROUGH_INTENDED;
    case SyncState::kAccountPasswordsActiveNormalEncryption:
      return true;
  }
}

bool PasswordCheckManager::AreScriptsRefreshed() const {
  return IsPreconditionFulfilled(kScriptsCachePrewarmed);
}

void PasswordCheckManager::RefreshScripts() {
  if (!ShouldFetchPasswordScripts()) {
    FulfillPrecondition(kScriptsCachePrewarmed);
    return;
  }
  ResetPrecondition(kScriptsCachePrewarmed);
  password_script_fetcher_->RefreshScriptsIfNecessary(base::BindOnce(
      &PasswordCheckManager::OnScriptsFetched, weak_ptr_factory_.GetWeakPtr()));
}

void PasswordCheckManager::OnScriptsFetched() {
  FulfillPrecondition(kScriptsCachePrewarmed);
  if (credentials_count_to_notify_.has_value()) {
    // Inform the UI about compromised credentials another time because it was
    // not allowed to generate UI before the availability of password scripts is
    // known.
    FulfillPrecondition(kKnownCredentialsFetched);
    observer_->OnCompromisedCredentialsChanged(
        credentials_count_to_notify_.value());
    credentials_count_to_notify_.reset();
  }
}

bool PasswordCheckManager::ShouldFetchPasswordScripts() const {
  SyncState sync_state = password_manager_util::GetPasswordSyncState(
      SyncServiceFactory::GetForProfile(profile_));

  // Password change scripts are using password generation, so automatic
  // password change should not be offered to non sync users.
  switch (sync_state) {
    case SyncState::kNotSyncing:
      return false;

    case SyncState::kSyncingWithCustomPassphrase:
      ABSL_FALLTHROUGH_INTENDED;
    case SyncState::kSyncingNormalEncryption:
      ABSL_FALLTHROUGH_INTENDED;
    case SyncState::kAccountPasswordsActiveNormalEncryption:
      return password_manager::features::IsPasswordScriptsFetchingEnabled();
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
