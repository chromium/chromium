// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_scripts_fetcher_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/content/browser/password_change_success_tracker_factory.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_change_success_tracker.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_metrics_util.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/password_scripts_fetcher.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "third_party/abseil-cpp/absl/types/optional.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace extensions {

namespace {

using password_manager::CanonicalizeUsername;
using password_manager::CredentialUIEntry;
using password_manager::InsecureType;
using password_manager::LeakCheckCredential;
using password_manager::PasswordChangeSuccessTracker;
using password_manager::PasswordForm;
using password_manager::PasswordScriptsFetcher;
using password_manager::metrics_util::PasswordCheckScriptsCacheState;
using ui::TimeFormat;

using InsecureCredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using State = password_manager::BulkLeakCheckService::State;

constexpr char kPasswordCheckScriptsCacheStateUmaKey[] =
    "PasswordManager.BulkCheck.ScriptsCacheState";

std::unique_ptr<std::string> GetChangePasswordUrl(const GURL& url) {
  return std::make_unique<std::string>(
      password_manager::CreateChangePasswordUrl(url).spec());
}

}  // namespace

// Key used to attach UserData to a LeakCheckCredential.
constexpr char kPasswordCheckDataKey[] = "password-check-data-key";

// Class remembering the state required to update the progress of an ongoing
// Password Check.
class PasswordCheckProgress : public base::RefCounted<PasswordCheckProgress> {
 public:
  base::WeakPtr<PasswordCheckProgress> GetWeakPtr() {
    return weak_ptr_factory_.GetWeakPtr();
  }

  size_t remaining_in_queue() const { return remaining_in_queue_; }
  size_t already_processed() const { return already_processed_; }

  // Increments the counts corresponding to |password|. Intended to be called
  // for each credential that is passed to the bulk check.
  void IncrementCounts(const PasswordForm& password) {
    ++remaining_in_queue_;
    ++counts_[password];
  }

  // Updates the counts after a |credential| has been processed by the bulk
  // check.
  void OnProcessed(const LeakCheckCredential& credential) {
    auto it = counts_.find(credential);
    const int num_matching = it != counts_.end() ? it->second : 0;
    already_processed_ += num_matching;
    remaining_in_queue_ -= num_matching;
  }

 private:
  friend class base::RefCounted<PasswordCheckProgress>;
  ~PasswordCheckProgress() = default;

  // Count variables needed to correctly show the progress of the check to the
  // user. |already_processed_| contains the number of credentials that have
  // been checked already, while |remaining_in_queue_| remembers how many
  // passwords still need to be checked.
  // Since the bulk leak check tries to be as efficient as possible, it performs
  // a deduplication step before starting to check passwords. In this step it
  // canonicalizes each credential, and only processes the combinations that are
  // unique. Since this number likely does not match the total number of saved
  // passwords, we remember in |counts_| how many saved passwords a given
  // canonicalized credential corresponds to.
  size_t already_processed_ = 0;
  size_t remaining_in_queue_ = 0;
  std::map<password_manager::CanonicalizedCredential, size_t> counts_;

  base::WeakPtrFactory<PasswordCheckProgress> weak_ptr_factory_{this};
};

namespace {

// A class attached to each LeakCheckCredential that holds a shared handle to
// the PasswordCheckProgress and is able to update the progress accordingly.
class PasswordCheckData : public LeakCheckCredential::Data {
 public:
  explicit PasswordCheckData(scoped_refptr<PasswordCheckProgress> progress)
      : progress_(std::move(progress)) {}
  ~PasswordCheckData() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<PasswordCheckData>(progress_);
  }

 private:
  scoped_refptr<PasswordCheckProgress> progress_;
};

api::passwords_private::PasswordCheckState ConvertPasswordCheckState(
    State state) {
  switch (state) {
    case State::kIdle:
      return api::passwords_private::PASSWORD_CHECK_STATE_IDLE;
    case State::kRunning:
      return api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;
    case State::kCanceled:
      return api::passwords_private::PASSWORD_CHECK_STATE_CANCELED;
    case State::kSignedOut:
      return api::passwords_private::PASSWORD_CHECK_STATE_SIGNED_OUT;
    case State::kNetworkError:
      return api::passwords_private::PASSWORD_CHECK_STATE_OFFLINE;
    case State::kQuotaLimit:
      return api::passwords_private::PASSWORD_CHECK_STATE_QUOTA_LIMIT;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return api::passwords_private::PASSWORD_CHECK_STATE_OTHER_ERROR;
  }

  NOTREACHED();
  return api::passwords_private::PASSWORD_CHECK_STATE_NONE;
}

std::string FormatElapsedTime(base::Time time) {
  const base::TimeDelta elapsed_time = base::Time::Now() - time;
  if (elapsed_time < base::Minutes(1))
    return l10n_util::GetStringUTF8(IDS_SETTINGS_PASSWORDS_JUST_NOW);

  return base::UTF16ToUTF8(TimeFormat::SimpleWithMonthAndYear(
      TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG, elapsed_time, true));
}

api::passwords_private::CompromiseType GetCompromiseType(
    const CredentialUIEntry& entry) {
  if (entry.IsLeaked() && entry.IsPhished()) {
    return api::passwords_private::COMPROMISE_TYPE_PHISHED_AND_LEAKED;
  } else if (entry.IsLeaked()) {
    return api::passwords_private::COMPROMISE_TYPE_LEAKED;
  } else if (entry.IsPhished()) {
    return api::passwords_private::COMPROMISE_TYPE_PHISHED;
  }
  NOTREACHED();
  return api::passwords_private::COMPROMISE_TYPE_NONE;
}

bool IsCredentialMuted(const CredentialUIEntry& entry) {
  if (!entry.IsLeaked() && !entry.IsPhished())
    return false;

  bool is_muted = true;
  if (entry.IsLeaked()) {
    is_muted &=
        entry.password_issues.at(InsecureType::kLeaked).is_muted.value();
  }
  if (entry.IsPhished()) {
    is_muted &=
        entry.password_issues.at(InsecureType::kPhished).is_muted.value();
  }
  return is_muted;
}

// Orders |compromised_credentials| in such a way that phished credentials
// precede leaked credentials, and that credentials of the same compromise type
// are ordered by recency.
void OrderInsecureCredentials(std::vector<CredentialUIEntry>& credentials) {
  // Reordering phished credential to the beginning.
  auto non_phished = std::partition(
      credentials.begin(), credentials.end(),
      [](const auto& credential) { return credential.IsPhished(); });

  // By construction the phished credentials precede the leaked credentials in
  // |results|. Now sort both groups by their creation date so that most recent
  // compromises appear first in both lists.
  auto create_time_cmp = [](auto& lhs, auto& rhs) {
    return lhs.GetLastLeakedOrPhishedTime() > rhs.GetLastLeakedOrPhishedTime();
  };
  std::sort(credentials.begin(), non_phished, create_time_cmp);
  std::sort(non_phished, credentials.end(), create_time_cmp);
}

api::passwords_private::CompromisedInfo CreateCompromiseInfo(
    const CredentialUIEntry& form) {
  api::passwords_private::CompromisedInfo compromise_info;
  compromise_info.compromise_time =
      form.GetLastLeakedOrPhishedTime().ToJsTimeIgnoringNull();
  compromise_info.elapsed_time_since_compromise =
      FormatElapsedTime(form.GetLastLeakedOrPhishedTime());
  compromise_info.compromise_type = GetCompromiseType(form);
  compromise_info.is_muted = IsCredentialMuted(form);
  return compromise_info;
}

}  // namespace

PasswordCheckDelegate::PasswordCheckDelegate(
    Profile* profile,
    password_manager::SavedPasswordsPresenter* presenter)
    : profile_(profile),
      saved_passwords_presenter_(presenter),
      insecure_credentials_manager_(presenter,
                                    PasswordStoreFactory::GetForProfile(
                                        profile,
                                        ServiceAccessType::EXPLICIT_ACCESS),
                                    AccountPasswordStoreFactory::GetForProfile(
                                        profile,
                                        ServiceAccessType::EXPLICIT_ACCESS)),
      bulk_leak_check_service_adapter_(
          presenter,
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs()) {
  observed_saved_passwords_presenter_.Observe(saved_passwords_presenter_.get());
  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);
  observed_bulk_leak_check_service_.Observe(
      BulkLeakCheckServiceFactory::GetForProfile(profile_));

  // Instructs the provider to initialize and build its cache.
  // This will soon after invoke OnCompromisedCredentialsChanged(). Calls to
  // GetCompromisedCredentials() that might happen until then will return an
  // empty list.
  insecure_credentials_manager_.Init();
}

PasswordCheckDelegate::~PasswordCheckDelegate() = default;

std::vector<api::passwords_private::InsecureCredential>
PasswordCheckDelegate::GetCompromisedCredentials() {
  std::vector<CredentialUIEntry> ordered_credentials =
      insecure_credentials_manager_.GetInsecureCredentialEntries();
  OrderInsecureCredentials(ordered_credentials);

  std::vector<api::passwords_private::InsecureCredential>
      compromised_credentials;
  compromised_credentials.reserve(ordered_credentials.size());
  for (const auto& credential : ordered_credentials) {
    api::passwords_private::InsecureCredential api_credential =
        ConstructInsecureCredential(credential);
    api_credential.compromised_info =
        std::make_unique<api::passwords_private::CompromisedInfo>(
            CreateCompromiseInfo(credential));
    compromised_credentials.push_back(std::move(api_credential));
  }

  return compromised_credentials;
}

std::vector<api::passwords_private::InsecureCredential>
PasswordCheckDelegate::GetWeakCredentials() {
  std::vector<CredentialUIEntry> weak_credentials =
      insecure_credentials_manager_.GetWeakCredentialEntries();

  std::vector<api::passwords_private::InsecureCredential> api_credentials;
  api_credentials.reserve(weak_credentials.size());
  for (auto& weak_credential : weak_credentials) {
    api_credentials.push_back(ConstructInsecureCredential(weak_credential));
  }

  return api_credentials;
}

absl::optional<api::passwords_private::InsecureCredential>
PasswordCheckDelegate::GetPlaintextInsecurePassword(
    api::passwords_private::InsecureCredential credential) const {
  const CredentialUIEntry* entry = FindMatchingEntry(credential);
  if (!entry)
    return absl::nullopt;

  credential.password =
      std::make_unique<std::string>(base::UTF16ToUTF8(entry->password));
  return credential;
}

bool PasswordCheckDelegate::ChangeInsecureCredential(
    const api::passwords_private::InsecureCredential& credential,
    base::StringPiece new_password) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* original_credential = FindMatchingEntry(credential);
  if (!original_credential)
    return false;

  CredentialUIEntry updated_credential = *original_credential;
  updated_credential.password = base::UTF8ToUTF16(new_password);
  switch (saved_passwords_presenter_->EditSavedCredentials(
      *original_credential, updated_credential)) {
    case password_manager::SavedPasswordsPresenter::EditResult::kSuccess:
    case password_manager::SavedPasswordsPresenter::EditResult::kNothingChanged:
      return true;
    case password_manager::SavedPasswordsPresenter::EditResult::kNotFound:
    case password_manager::SavedPasswordsPresenter::EditResult::kAlreadyExisits:
    case password_manager::SavedPasswordsPresenter::EditResult::kEmptyPassword:
      return false;
  }
}

bool PasswordCheckDelegate::RemoveInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* entry = FindMatchingEntry(credential);
  if (!entry)
    return false;

  return saved_passwords_presenter_->RemoveCredential(*entry);
}

bool PasswordCheckDelegate::MuteInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* entry = FindMatchingEntry(credential);
  if (!entry)
    return false;

  return insecure_credentials_manager_.MuteCredential(*entry);
}

bool PasswordCheckDelegate::UnmuteInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* entry = FindMatchingEntry(credential);
  if (!entry)
    return false;

  return insecure_credentials_manager_.UnmuteCredential(*entry);
}

// Records that a change password flow was started for |credential| and
// whether |is_manual_flow| applies to the flow.
void PasswordCheckDelegate::RecordChangePasswordFlowStarted(
    const api::passwords_private::InsecureCredential& credential,
    bool is_manual_flow) {
  // If the |credential| does not have a |change_password_url|, skip it.
  if (!credential.change_password_url)
    return;

  if (is_manual_flow) {
    GetPasswordChangeSuccessTracker()->OnManualChangePasswordFlowStarted(
        GURL(*credential.change_password_url), credential.username,
        PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
  } else {
    GetPasswordChangeSuccessTracker()->OnChangePasswordFlowStarted(
        GURL(*credential.change_password_url), credential.username,
        PasswordChangeSuccessTracker::StartEvent::kAutomatedFlow,
        PasswordChangeSuccessTracker::EntryPoint::kLeakCheckInSettings);
  }
}

void PasswordCheckDelegate::RefreshScriptsIfNecessary(
    RefreshScriptsIfNecessaryCallback callback) {
  if (PasswordScriptsFetcher* fetcher = GetPasswordScriptsFetcher()) {
    fetcher->RefreshScriptsIfNecessary(std::move(callback));
    return;
  }
  std::move(callback).Run();
}

void PasswordCheckDelegate::StartPasswordCheck(
    StartPasswordCheckCallback callback) {
  // If the delegate isn't initialized yet, enqueue the callback and return
  // early.
  if (!is_initialized_) {
    start_check_callbacks_.push_back(std::move(callback));
    return;
  }

  // Also return early if the check is already running.
  if (is_check_running_ ||
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
          State::kRunning) {
    std::move(callback).Run(State::kRunning);
    return;
  }

  // If automated password change from password check in settings is enabled,
  // we make sure that the cache is warm prior to analyzing passwords.
  is_check_running_ = true;
  if (base::FeatureList::IsEnabled(
          password_manager::features::kPasswordChange)) {
    if (GetPasswordScriptsFetcher()->IsCacheStale()) {
      // The UMA metric for a stale cache is recorded on callback.
      GetPasswordScriptsFetcher()->RefreshScriptsIfNecessary(
          base::BindOnce(&PasswordCheckDelegate::OnPasswordScriptsFetched,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
      return;
    }
    UMA_HISTOGRAM_ENUMERATION(kPasswordCheckScriptsCacheStateUmaKey,
                              PasswordCheckScriptsCacheState::kCacheFresh);
  }

  // Otherwise, call directly.
  StartPasswordAnalyses(std::move(callback));
}

void PasswordCheckDelegate::OnPasswordScriptsFetched(
    StartPasswordCheckCallback callback) {
  if (PasswordsPrivateEventRouter* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    // Only update if at least one credential now has a startable script.
    std::vector<api::passwords_private::InsecureCredential> credentials =
        GetCompromisedCredentials();
    if (base::ranges::any_of(credentials,
                             &api::passwords_private::InsecureCredential::
                                 has_startable_script)) {
      UMA_HISTOGRAM_ENUMERATION(
          kPasswordCheckScriptsCacheStateUmaKey,
          PasswordCheckScriptsCacheState::kCacheStaleAndUiUpdate);
      event_router->OnCompromisedCredentialsChanged(std::move(credentials));
    } else {
      UMA_HISTOGRAM_ENUMERATION(
          kPasswordCheckScriptsCacheStateUmaKey,
          PasswordCheckScriptsCacheState::kCacheStaleAndNoUiUpdate);
    }
  }
  StartPasswordAnalyses(std::move(callback));
}

void PasswordCheckDelegate::StartPasswordAnalyses(
    StartPasswordCheckCallback callback) {
  // This is set as soon as the script availability fetching is started.
  DCHECK(is_check_running_);

  // Start the weakness check, and notify observers once done.
  insecure_credentials_manager_.StartWeakCheck(base::BindOnce(
      &PasswordCheckDelegate::RecordAndNotifyAboutCompletedWeakPasswordCheck,
      weak_ptr_factory_.GetWeakPtr()));

  auto progress = base::MakeRefCounted<PasswordCheckProgress>();
  for (const auto& password : saved_passwords_presenter_->GetSavedPasswords())
    progress->IncrementCounts(password);

  password_check_progress_ = progress->GetWeakPtr();
  PasswordCheckData data(std::move(progress));
  is_check_running_ = bulk_leak_check_service_adapter_.StartBulkLeakCheck(
      kPasswordCheckDataKey, &data);
  DCHECK(is_check_running_);
  std::move(callback).Run(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
}

void PasswordCheckDelegate::StopPasswordCheck() {
  if (!is_initialized_) {
    for (auto&& callback : std::exchange(start_check_callbacks_, {}))
      std::move(callback).Run(State::kIdle);
    return;
  }

  bulk_leak_check_service_adapter_.StopBulkLeakCheck();
}

api::passwords_private::PasswordCheckStatus
PasswordCheckDelegate::GetPasswordCheckStatus() const {
  api::passwords_private::PasswordCheckStatus result;

  // Obtain the timestamp of the last completed password or weak check. This
  // will be null in case no check has completely ran before.
  base::Time last_check_completed =
      std::max(base::Time::FromTimeT(profile_->GetPrefs()->GetDouble(
                   password_manager::prefs::kLastTimePasswordCheckCompleted)),
               last_completed_weak_check_);
  if (!last_check_completed.is_null()) {
    result.elapsed_time_since_last_check =
        std::make_unique<std::string>(FormatElapsedTime(last_check_completed));
  }

  State state = bulk_leak_check_service_adapter_.GetBulkLeakCheckState();

  // Handle the currently running case first, only then consider errors.
  if (state == State::kRunning) {
    result.state = api::passwords_private::PASSWORD_CHECK_STATE_RUNNING;

    if (password_check_progress_) {
      result.already_processed =
          std::make_unique<int>(password_check_progress_->already_processed());
      result.remaining_in_queue =
          std::make_unique<int>(password_check_progress_->remaining_in_queue());
    } else {
      result.already_processed = std::make_unique<int>(0);
      result.remaining_in_queue = std::make_unique<int>(0);
    }

    return result;
  }

  if (saved_passwords_presenter_->GetSavedCredentials().empty()) {
    result.state = api::passwords_private::PASSWORD_CHECK_STATE_NO_PASSWORDS;
    return result;
  }

  result.state = ConvertPasswordCheckState(state);
  return result;
}

password_manager::InsecureCredentialsManager*
PasswordCheckDelegate::GetInsecureCredentialsManager() {
  return &insecure_credentials_manager_;
}

void PasswordCheckDelegate::OnSavedPasswordsChanged(SavedPasswordsView) {
  // Getting the first notification about a change in saved passwords implies
  // that the delegate is initialized, and start check callbacks can be invoked,
  // if any.
  if (!std::exchange(is_initialized_, true)) {
    for (auto&& callback : std::exchange(start_check_callbacks_, {}))
      StartPasswordCheck(std::move(callback));
  }

  // A change in the saved passwords might result in leaving or entering the
  // NO_PASSWORDS state, thus we need to trigger a notification.
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::OnInsecureCredentialsChanged(
    InsecureCredentialsView credentials) {
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnCompromisedCredentialsChanged(GetCompromisedCredentials());
  }
}

void PasswordCheckDelegate::OnWeakCredentialsChanged() {
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnWeakCredentialsChanged(GetWeakCredentials());
  }
}

void PasswordCheckDelegate::OnStateChanged(State state) {
  if (state == State::kIdle && std::exchange(is_check_running_, false)) {
    // When the service transitions from running into idle it has finished a
    // check.
    RecordAndNotifyAboutCompletedCompromisedPasswordCheck();
    return;
  }

  // NotifyPasswordCheckStatusChanged() invokes GetPasswordCheckStatus()
  // obtaining the relevant information. Thus there is no need to forward the
  // arguments passed to OnStateChanged().
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::OnCredentialDone(
    const LeakCheckCredential& credential,
    password_manager::IsLeaked is_leaked) {
  if (is_leaked) {
    insecure_credentials_manager_.SaveInsecureCredential(credential);
  }

  // Update the progress in case there is one.
  if (password_check_progress_)
    password_check_progress_->OnProcessed(credential);

  // While the check is still running trigger an update of the check status,
  // considering that the progress has changed.
  if (bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
      State::kRunning) {
    NotifyPasswordCheckStatusChanged();
  }
}

const CredentialUIEntry* PasswordCheckDelegate::FindMatchingEntry(
    const api::passwords_private::InsecureCredential& credential) const {
  const CredentialUIEntry* entry =
      insecure_credential_id_generator_.TryGetKey(credential.id);
  if (!entry)
    return nullptr;

  if (credential.signon_realm != entry->signon_realm ||
      credential.username != base::UTF16ToUTF8(entry->username) ||
      (credential.password &&
       *credential.password != base::UTF16ToUTF8(entry->password))) {
    return nullptr;
  }

  return entry;
}

void PasswordCheckDelegate::
    RecordAndNotifyAboutCompletedCompromisedPasswordCheck() {
  profile_->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      base::Time::Now().ToDoubleT());
  profile_->GetPrefs()->SetTime(
      password_manager::prefs::kSyncedLastTimePasswordCheckCompleted,
      base::Time::Now());

  // Delay the last Check Status update by a second. This avoids flickering of
  // the UI if the full check ran from start to finish almost immediately.
  base::SequencedTaskRunnerHandle::Get()->PostDelayedTask(
      FROM_HERE,
      base::BindOnce(&PasswordCheckDelegate::NotifyPasswordCheckStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()),
      base::Seconds(1));
}

void PasswordCheckDelegate::RecordAndNotifyAboutCompletedWeakPasswordCheck() {
  last_completed_weak_check_ = base::Time::Now();
  // Note: In contrast to the compromised password check we do not does not
  // artificially delay the response, Since this check is expected to complete
  // quickly.
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::NotifyPasswordCheckStatusChanged() {
  if (auto* event_router =
          PasswordsPrivateEventRouterFactory::GetForProfile(profile_)) {
    event_router->OnPasswordCheckStatusChanged(GetPasswordCheckStatus());
  }
}

api::passwords_private::InsecureCredential
PasswordCheckDelegate::ConstructInsecureCredential(
    const CredentialUIEntry& entry) {
  api::passwords_private::InsecureCredential api_credential;
  auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
      entry.signon_realm);
  if (facet.IsValidAndroidFacetURI()) {
    api_credential.is_android_credential = true;
    // |formatted_orgin|, |detailed_origin| and |change_password_url| need
    // special handling for Android. Here we use affiliation information
    // instead of the origin.
    if (!entry.app_display_name.empty()) {
      api_credential.formatted_origin = entry.app_display_name;
      api_credential.detailed_origin = entry.app_display_name;
      api_credential.change_password_url =
          GetChangePasswordUrl(GURL(entry.affiliated_web_realm));
    } else {
      // In case no affiliation information could be obtained show the
      // formatted package name to the user. An empty change_password_url will
      // be handled by the frontend, by not including a link in this case.
      api_credential.formatted_origin = l10n_util::GetStringFUTF8(
          IDS_SETTINGS_PASSWORDS_ANDROID_APP,
          base::UTF8ToUTF16(facet.android_package_name()));
      api_credential.detailed_origin = facet.android_package_name();
    }
  } else {
    api_credential.is_android_credential = false;
    api_credential.formatted_origin =
        base::UTF16ToUTF8(url_formatter::FormatUrl(
            entry.url.GetWithEmptyPath(),
            url_formatter::kFormatUrlOmitDefaults |
                url_formatter::kFormatUrlOmitHTTPS |
                url_formatter::kFormatUrlOmitTrivialSubdomains |
                url_formatter::kFormatUrlTrimAfterHost,
            base::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    api_credential.detailed_origin =
        base::UTF16ToUTF8(url_formatter::FormatUrlForSecurityDisplay(
            entry.url.GetWithEmptyPath()));
    api_credential.change_password_url = GetChangePasswordUrl(entry.url);
  }

  api_credential.id = insecure_credential_id_generator_.GenerateId(entry);
  api_credential.signon_realm = entry.signon_realm;
  api_credential.username = base::UTF16ToUTF8(entry.username);

  // For the time being, the automated password change is restricted to
  // compromised credentials. In the future, this requirement may be relaxed.
  if ((entry.IsPhished() || entry.IsLeaked()) &&
      IsAutomatedPasswordChangeFromSettingsEnabled() &&
      !entry.username.empty()) {
    GURL url = api_credential.is_android_credential
                   ? GURL(entry.affiliated_web_realm)
                   : entry.url;
    api_credential.has_startable_script =
        !url.is_empty() && GetPasswordScriptsFetcher()->IsScriptAvailable(
                               url::Origin::Create(entry.url));
  } else {
    api_credential.has_startable_script = false;
  }

  return api_credential;
}

PasswordChangeSuccessTracker*
PasswordCheckDelegate::GetPasswordChangeSuccessTracker() const {
  return password_manager::PasswordChangeSuccessTrackerFactory::
      GetForBrowserContext(profile_);
}

PasswordScriptsFetcher* PasswordCheckDelegate::GetPasswordScriptsFetcher()
    const {
  return PasswordScriptsFetcherFactory::GetForBrowserContext(profile_);
}

bool PasswordCheckDelegate::IsAutomatedPasswordChangeFromSettingsEnabled()
    const {
  // Do not offer password change to non-syncing users, as it is required
  // for generating passwords.
  if (password_manager_util::GetPasswordSyncState(
          SyncServiceFactory::GetForProfile(profile_)) ==
      password_manager::SyncState::kNotSyncing) {
    return false;
  }
  return base::FeatureList::IsEnabled(
      password_manager::features::kPasswordChange);
}

}  // namespace extensions
