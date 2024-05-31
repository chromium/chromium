// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/extensions/api/passwords_private/password_check_delegate.h"

#include <stddef.h>

#include <algorithm>
#include <iterator>
#include <map>
#include <memory>
#include <optional>
#include <string>
#include <utility>

#include "base/containers/flat_set.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/memory/ref_counted.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/safe_conversions.h"
#include "base/ranges/algorithm.h"
#include "base/strings/escape.h"
#include "base/strings/utf_string_conversions.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/time.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/profile_password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/affiliations/core/browser/affiliation_utils.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/password_manager_client.h"
#include "components/password_manager/core/browser/password_manager_util.h"
#include "components/password_manager/core/browser/ui/credential_ui_entry.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
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
using password_manager::PasswordForm;
using ui::TimeFormat;

using State = password_manager::BulkLeakCheckService::State;

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
  void IncrementCounts(const CredentialUIEntry& password) {
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
  PasswordCheckData(
      scoped_refptr<PasswordCheckProgress> progress,
      password_manager::TriggerBackendNotification should_trigger_notification)
      : progress_(std::move(progress)),
        should_trigger_notification_(should_trigger_notification) {}
  ~PasswordCheckData() override = default;

  std::unique_ptr<Data> Clone() override {
    return std::make_unique<PasswordCheckData>(progress_,
                                               should_trigger_notification_);
  }

  password_manager::TriggerBackendNotification should_trigger_notification()
      const {
    return should_trigger_notification_;
  }

 private:
  scoped_refptr<PasswordCheckProgress> progress_;
  // Certain client use cases require to notify backend if new leaked
  // credentials are found. This member indicate whether that should happen.
  const password_manager::TriggerBackendNotification
      should_trigger_notification_;
};

api::passwords_private::PasswordCheckState ConvertPasswordCheckState(
    State state) {
  switch (state) {
    case State::kIdle:
      return api::passwords_private::PasswordCheckState::kIdle;
    case State::kRunning:
      return api::passwords_private::PasswordCheckState::kRunning;
    case State::kCanceled:
      return api::passwords_private::PasswordCheckState::kCanceled;
    case State::kSignedOut:
      return api::passwords_private::PasswordCheckState::kSignedOut;
    case State::kNetworkError:
      return api::passwords_private::PasswordCheckState::kOffline;
    case State::kQuotaLimit:
      return api::passwords_private::PasswordCheckState::kQuotaLimit;
    case State::kTokenRequestFailure:
    case State::kHashingFailure:
    case State::kServiceError:
      return api::passwords_private::PasswordCheckState::kOtherError;
  }

  NOTREACHED_IN_MIGRATION();
  return api::passwords_private::PasswordCheckState::kNone;
}

std::string FormatElapsedTime(base::Time time) {
  const base::TimeDelta elapsed_time = base::Time::Now() - time;
  if (elapsed_time < base::Minutes(1))
    return l10n_util::GetStringUTF8(IDS_PASSWORD_MANAGER_UI_JUST_NOW);

  return base::UTF16ToUTF8(TimeFormat::SimpleWithMonthAndYear(
      TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG, elapsed_time, true));
}

std::vector<api::passwords_private::CompromiseType> GetCompromiseType(
    const CredentialUIEntry& entry) {
  std::vector<api::passwords_private::CompromiseType> types;
  for (const auto& issue : entry.password_issues) {
    switch (issue.first) {
      case InsecureType::kLeaked:
        types.push_back(api::passwords_private::CompromiseType::kLeaked);
        break;
      case InsecureType::kPhished:
        types.push_back(api::passwords_private::CompromiseType::kPhished);
        break;
      case InsecureType::kReused:
        types.push_back(api::passwords_private::CompromiseType::kReused);
        break;
      case InsecureType::kWeak:
        types.push_back(api::passwords_private::CompromiseType::kWeak);
        break;
    }
  }
  DCHECK(!types.empty());
  return types;
}

api::passwords_private::CompromisedInfo CreateCompromiseInfo(
    const CredentialUIEntry& credential) {
  api::passwords_private::CompromisedInfo compromise_info;
  // Weak credentials don't have compromise time, they also can't be muted.
  if (IsCompromised(credential)) {
    compromise_info.compromise_time =
        credential.GetLastLeakedOrPhishedTime()
            .InMillisecondsFSinceUnixEpochIgnoringNull();
    compromise_info.elapsed_time_since_compromise =
        FormatElapsedTime(credential.GetLastLeakedOrPhishedTime());
    compromise_info.is_muted = credential.IsMuted();
  }
  compromise_info.compromise_types = GetCompromiseType(credential);
  return compromise_info;
}

}  // namespace

PasswordCheckDelegate::PasswordCheckDelegate(
    Profile* profile,
    password_manager::SavedPasswordsPresenter* presenter,
    IdGenerator* id_generator,
    PasswordsPrivateEventRouter* event_router)
    : profile_(profile),
      saved_passwords_presenter_(presenter),
      insecure_credentials_manager_(presenter),
      bulk_leak_check_service_adapter_(
          presenter,
          BulkLeakCheckServiceFactory::GetForProfile(profile_),
          profile_->GetPrefs()),
      id_generator_(id_generator),
      event_router_(event_router) {
  DCHECK(id_generator);
  observed_saved_passwords_presenter_.Observe(saved_passwords_presenter_.get());
  observed_insecure_credentials_manager_.Observe(
      &insecure_credentials_manager_);
  observed_bulk_leak_check_service_.Observe(
      BulkLeakCheckServiceFactory::GetForProfile(profile_));
}

PasswordCheckDelegate::~PasswordCheckDelegate() = default;

std::vector<api::passwords_private::PasswordUiEntry>
PasswordCheckDelegate::GetInsecureCredentials() {
  std::vector<CredentialUIEntry> credentials =
      insecure_credentials_manager_.GetInsecureCredentialEntries();

  std::vector<api::passwords_private::PasswordUiEntry> insecure_credentials;
  insecure_credentials.reserve(credentials.size());
  for (auto& credential : credentials) {
    insecure_credentials.push_back(
        ConstructInsecureCredentialUiEntry(std::move(credential)));
  }

  return insecure_credentials;
}

std::vector<api::passwords_private::PasswordUiEntryList>
PasswordCheckDelegate::GetCredentialsWithReusedPassword() {
  // Group credentials by password value.
  std::map<std::u16string, std::vector<api::passwords_private::PasswordUiEntry>>
      password_to_credentials;
  for (auto& credential :
       insecure_credentials_manager_.GetInsecureCredentialEntries()) {
    if (credential.IsReused()) {
      password_to_credentials[credential.password].push_back(
          ConstructInsecureCredentialUiEntry(credential));
    }
  }

  std::vector<api::passwords_private::PasswordUiEntryList> result;
  result.reserve(password_to_credentials.size());
  for (auto& pair : password_to_credentials) {
    // This check is relevant in the cases where the password store has changed
    // after the password check was already run. (e.g if a reused password has
    // been deleted)
    if (pair.second.size() < 2) {
      continue;
    }
    api::passwords_private::PasswordUiEntryList api_result;
    api_result.entries = std::move(pair.second);
    result.push_back(std::move(api_result));
  }
  return result;
}

bool PasswordCheckDelegate::MuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* entry = id_generator_->TryGetKey(credential.id);
  if (!entry)
    return false;

  return insecure_credentials_manager_.MuteCredential(*entry);
}

bool PasswordCheckDelegate::UnmuteInsecureCredential(
    const api::passwords_private::PasswordUiEntry& credential) {
  // Try to obtain the original CredentialUIEntry. Return false if fails.
  const CredentialUIEntry* entry = id_generator_->TryGetKey(credential.id);
  if (!entry)
    return false;

  return insecure_credentials_manager_.UnmuteCredential(*entry);
}

void PasswordCheckDelegate::StartPasswordCheck(
    password_manager::LeakDetectionInitiator initiator,
    StartPasswordCheckCallback callback) {
  // Calls to StartPasswordCheck() will be only processed after
  // OnSavedPasswordsChanged() is called. Meaning that all client calls
  // happening before that will be stored in memory until all conditions are
  // met. Thus initiator value must be stored to ensure that when this method is
  // run, it has the correct value.
  password_check_initiator_ = initiator;

  // If the delegate isn't initialized yet, enqueue the callback and return
  // early.
  if (!is_initialized_) {
    start_check_callbacks_.push_back(std::move(callback));
    return;
  }

  // Also return early if the check is already running.
  if (bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
      State::kRunning) {
    std::move(callback).Run(State::kRunning);
    return;
  }

  StartPasswordAnalyses(std::move(callback));
}

void PasswordCheckDelegate::StartPasswordAnalyses(
    StartPasswordCheckCallback callback) {
  // Start the weakness check, and notify observers once done.
  insecure_credentials_manager_.StartWeakCheck(base::BindOnce(
      &PasswordCheckDelegate::RecordAndNotifyAboutCompletedWeakPasswordCheck,
      weak_ptr_factory_.GetWeakPtr()));
  insecure_credentials_manager_.StartReuseCheck(
      base::BindOnce(&PasswordCheckDelegate::NotifyPasswordCheckStatusChanged,
                     weak_ptr_factory_.GetWeakPtr()));
  auto progress = base::MakeRefCounted<PasswordCheckProgress>();
  for (const auto& password : saved_passwords_presenter_->GetSavedPasswords())
    progress->IncrementCounts(password);

  password_check_progress_ = progress->GetWeakPtr();
  PasswordCheckData data(
      std::move(progress),
      password_manager::ShouldTriggerBackendNotificationForInitiator(
          password_check_initiator_));

  is_check_running_ = bulk_leak_check_service_adapter_.StartBulkLeakCheck(
      password_check_initiator_, kPasswordCheckDataKey, &data);

  DCHECK(is_check_running_);
  std::move(callback).Run(
      bulk_leak_check_service_adapter_.GetBulkLeakCheckState());
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
        FormatElapsedTime(last_check_completed);
  }

  State state = bulk_leak_check_service_adapter_.GetBulkLeakCheckState();

  result.total_number_of_passwords =
      saved_passwords_presenter_->GetSavedPasswords().size();

  // Handle the currently running case first, only then consider errors.
  if (state == State::kRunning) {
    result.state = api::passwords_private::PasswordCheckState::kRunning;

    if (password_check_progress_) {
      result.already_processed = password_check_progress_->already_processed();
      result.remaining_in_queue =
          password_check_progress_->remaining_in_queue();
    } else {
      result.already_processed = 0;
      result.remaining_in_queue = 0;
    }

    return result;
  }

  if (result.total_number_of_passwords == 0) {
    result.state = api::passwords_private::PasswordCheckState::kNoPasswords;
    return result;
  }

  result.state = ConvertPasswordCheckState(state);
  return result;
}

password_manager::InsecureCredentialsManager*
PasswordCheckDelegate::GetInsecureCredentialsManager() {
  return &insecure_credentials_manager_;
}

void PasswordCheckDelegate::OnBulkCheckServiceShutDown() {
  // Stop observing BulkLeakCheckService when the service shuts down.
  CHECK(observed_bulk_leak_check_service_.IsObservingSource(
      BulkLeakCheckServiceFactory::GetForProfile(profile_)));
  observed_bulk_leak_check_service_.Reset();
}

void PasswordCheckDelegate::OnSavedPasswordsChanged(
    const password_manager::PasswordStoreChangeList& changes) {
  // Getting the first notification about a change in saved passwords implies
  // that the delegate is initialized, and start check callbacks can be invoked,
  // if any.
  if (!std::exchange(is_initialized_, true)) {
    for (auto&& callback : std::exchange(start_check_callbacks_, {}))
      StartPasswordCheck(password_check_initiator_, std::move(callback));
  }

  // A change in the saved passwords might result in leaving or entering the
  // NO_PASSWORDS state, thus we need to trigger a notification.
  NotifyPasswordCheckStatusChanged();
}

void PasswordCheckDelegate::OnInsecureCredentialsChanged() {
  if (event_router_) {
    event_router_->OnInsecureCredentialsChanged(GetInsecureCredentials());
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
    password_manager::TriggerBackendNotification should_trigger_notification =
        credential.GetUserData(kPasswordCheckDataKey)
            ? static_cast<PasswordCheckData*>(
                  credential.GetUserData(kPasswordCheckDataKey))
                  ->should_trigger_notification()
            : password_manager::TriggerBackendNotification(false);
    insecure_credentials_manager_.SaveInsecureCredential(
        credential, should_trigger_notification);
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

void PasswordCheckDelegate::
    RecordAndNotifyAboutCompletedCompromisedPasswordCheck() {
  profile_->GetPrefs()->SetDouble(
      password_manager::prefs::kLastTimePasswordCheckCompleted,
      base::Time::Now().InSecondsFSinceUnixEpoch());

  // Delay the last Check Status update by a second. This avoids flickering of
  // the UI if the full check ran from start to finish almost immediately.
  base::SequencedTaskRunner::GetCurrentDefault()->PostDelayedTask(
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
  if (event_router_) {
    event_router_->OnPasswordCheckStatusChanged(GetPasswordCheckStatus());
  }
}

api::passwords_private::PasswordUiEntry
PasswordCheckDelegate::ConstructInsecureCredentialUiEntry(
    CredentialUIEntry entry) {
  api::passwords_private::PasswordUiEntry api_credential;
  api_credential.username = base::UTF16ToUTF8(entry.username);
  api_credential.stored_in = StoreSetFromCredential(entry);
  api_credential.compromised_info = CreateCompromiseInfo(entry);
  std::optional<GURL> change_password_url = entry.GetChangePasswordURL();
  if (change_password_url.has_value()) {
    api_credential.change_password_url = change_password_url->spec();
  }
  CredentialUIEntry copy(std::move(entry));
  // Weak and reused flags should be cleaned before obtaining id. Otherwise
  // weak or reused flag will be saved to the database whenever credential is
  // modified.
  // TODO(crbug.com/40869244): Update this once saving weak and reused issues is
  // supported.
  copy.password_issues.erase(InsecureType::kWeak);
  copy.password_issues.erase(InsecureType::kReused);
  api_credential.id = id_generator_->GenerateId(std::move(copy));

  return api_credential;
}

}  // namespace extensions
