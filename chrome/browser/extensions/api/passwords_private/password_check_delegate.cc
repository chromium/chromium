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
#include "base/memory/ref_counted.h"
#include "base/numerics/safe_conversions.h"
#include "base/optional.h"
#include "base/stl_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/threading/sequenced_task_runner_handle.h"
#include "base/time/time.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_event_router_factory.h"
#include "chrome/browser/extensions/api/passwords_private/passwords_private_utils.h"
#include "chrome/browser/password_manager/account_password_store_factory.h"
#include "chrome/browser/password_manager/bulk_leak_check_service_factory.h"
#include "chrome/browser/password_manager/password_store_factory.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/extensions/api/passwords_private.h"
#include "chrome/grit/generated_resources.h"
#include "components/keyed_service/core/service_access_type.h"
#include "components/password_manager/core/browser/android_affiliation/affiliation_utils.h"
#include "components/password_manager/core/browser/bulk_leak_check_service.h"
#include "components/password_manager/core/browser/insecure_credentials_table.h"
#include "components/password_manager/core/browser/leak_detection/bulk_leak_check.h"
#include "components/password_manager/core/browser/leak_detection/encryption_utils.h"
#include "components/password_manager/core/browser/password_form.h"
#include "components/password_manager/core/browser/ui/credential_utils.h"
#include "components/password_manager/core/browser/ui/insecure_credentials_manager.h"
#include "components/password_manager/core/browser/ui/saved_passwords_presenter.h"
#include "components/password_manager/core/browser/well_known_change_password_util.h"
#include "components/password_manager/core/common/password_manager_features.h"
#include "components/password_manager/core/common/password_manager_pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/url_formatter/elide_url.h"
#include "components/url_formatter/url_formatter.h"
#include "net/base/escape.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "url/gurl.h"

namespace extensions {

namespace {

using password_manager::CanonicalizeUsername;
using password_manager::CredentialWithPassword;
using password_manager::InsecureCredentialTypeFlags;
using password_manager::LeakCheckCredential;
using password_manager::PasswordForm;
using ui::TimeFormat;

using InsecureCredentialsView =
    password_manager::InsecureCredentialsManager::CredentialsView;
using SavedPasswordsView =
    password_manager::SavedPasswordsPresenter::SavedPasswordsView;
using State = password_manager::BulkLeakCheckService::State;

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
  if (elapsed_time < base::TimeDelta::FromMinutes(1))
    return l10n_util::GetStringUTF8(IDS_SETTINGS_PASSWORDS_JUST_NOW);

  return base::UTF16ToUTF8(TimeFormat::SimpleWithMonthAndYear(
      TimeFormat::FORMAT_ELAPSED, TimeFormat::LENGTH_LONG, elapsed_time, true));
}

// Helper struct that bundles a CredentialWithPassword with a corresponding
// passwords_private::CompromiseType.
struct CompromisedCredentialAndType {
  CredentialWithPassword credential;
  api::passwords_private::CompromiseType type;
};

// Orders |compromised_credentials| in such a way that phished credentials
// precede leaked credentials, and that credentials of the same compromise type
// are ordered by recency.
std::vector<CompromisedCredentialAndType> OrderCompromisedCredentials(
    std::vector<CredentialWithPassword> compromised_credentials) {
  // Move all credentials into a single list, associating with the
  // corresponding CompromiseType.
  std::vector<CompromisedCredentialAndType> results;
  results.reserve(compromised_credentials.size());
  for (auto& credential : compromised_credentials) {
    // Since CompromiseType does not contain information about weakness of
    // credential, we need to unset this bit in the |credential.insecure_type|.
    auto type = static_cast<api::passwords_private::CompromiseType>(
        UnsetWeakCredentialTypeFlag(credential.insecure_type));
    results.push_back({std::move(credential), type});
  }
  // Reordering phished credential to the beginning.
  auto last_phished = std::partition(
      results.begin(), results.end(), [](const auto& credential) {
        return credential.type !=
               api::passwords_private::COMPROMISE_TYPE_LEAKED;
      });

  // By construction the phished credentials precede the leaked credentials in
  // |results|. Now sort both groups by their creation date so that most recent
  // compromises appear first in both lists.
  auto create_time_cmp = [](const auto& lhs, const auto& rhs) {
    return lhs.credential.create_time > rhs.credential.create_time;
  };
  std::sort(results.begin(), last_phished, create_time_cmp);
  std::sort(last_phished, results.end(), create_time_cmp);
  return results;
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
  observed_saved_passwords_presenter_.Observe(saved_passwords_presenter_);
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
  std::vector<CompromisedCredentialAndType>
      ordered_compromised_credential_and_types = OrderCompromisedCredentials(
          insecure_credentials_manager_.GetInsecureCredentials());

  std::vector<api::passwords_private::InsecureCredential>
      compromised_credentials;
  compromised_credentials.reserve(
      ordered_compromised_credential_and_types.size());
  for (const auto& credential_and_type :
       ordered_compromised_credential_and_types) {
    const CredentialWithPassword& credential = credential_and_type.credential;
    api::passwords_private::InsecureCredential api_credential =
        ConstructInsecureCredential(credential);
    api_credential.compromised_info =
        std::make_unique<api::passwords_private::CompromisedInfo>();
    api_credential.compromised_info->compromise_time =
        credential.create_time.ToJsTimeIgnoringNull();
    api_credential.compromised_info->elapsed_time_since_compromise =
        FormatElapsedTime(credential.create_time);
    api_credential.compromised_info->compromise_type = credential_and_type.type;
    compromised_credentials.push_back(std::move(api_credential));
  }

  return compromised_credentials;
}

std::vector<api::passwords_private::InsecureCredential>
PasswordCheckDelegate::GetWeakCredentials() {
  std::vector<CredentialWithPassword> weak_credentials =
      insecure_credentials_manager_.GetWeakCredentials();

  std::vector<api::passwords_private::InsecureCredential> api_credentials;
  api_credentials.reserve(weak_credentials.size());
  for (const auto& weak_credential : weak_credentials) {
    api_credentials.push_back(ConstructInsecureCredential(weak_credential));
  }

  return api_credentials;
}

base::Optional<api::passwords_private::InsecureCredential>
PasswordCheckDelegate::GetPlaintextInsecurePassword(
    api::passwords_private::InsecureCredential credential) const {
  const CredentialWithPassword* insecure_credential =
      FindMatchingInsecureCredential(credential);
  if (!insecure_credential)
    return base::nullopt;

  credential.password = std::make_unique<std::string>(
      base::UTF16ToUTF8(insecure_credential->password));
  return credential;
}

bool PasswordCheckDelegate::ChangeInsecureCredential(
    const api::passwords_private::InsecureCredential& credential,
    base::StringPiece new_password) {
  // Try to obtain the original CredentialWithPassword. Return false if fails.
  const CredentialWithPassword* insecure_credential =
      FindMatchingInsecureCredential(credential);
  if (!insecure_credential)
    return false;

  return insecure_credentials_manager_.UpdateCredential(*insecure_credential,
                                                        new_password);
}

bool PasswordCheckDelegate::RemoveInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) {
  // Try to obtain the original CredentialWithPassword. Return false if fails.
  const CredentialWithPassword* insecure_credential =
      FindMatchingInsecureCredential(credential);
  if (!insecure_credential)
    return false;

  return insecure_credentials_manager_.RemoveCredential(*insecure_credential);
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
  if (bulk_leak_check_service_adapter_.GetBulkLeakCheckState() ==
      State::kRunning) {
    std::move(callback).Run(State::kRunning);
    return;
  }

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
  SavedPasswordsView saved_passwords =
      saved_passwords_presenter_->GetSavedPasswords();

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

  if (saved_passwords.empty()) {
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

const CredentialWithPassword*
PasswordCheckDelegate::FindMatchingInsecureCredential(
    const api::passwords_private::InsecureCredential& credential) const {
  const CredentialWithPassword* insecure_credential =
      insecure_credential_id_generator_.TryGetKey(credential.id);
  if (!insecure_credential)
    return nullptr;

  if (credential.signon_realm != insecure_credential->signon_realm ||
      credential.username != base::UTF16ToUTF8(insecure_credential->username) ||
      (credential.password &&
       *credential.password !=
           base::UTF16ToUTF8(insecure_credential->password))) {
    return nullptr;
  }

  return insecure_credential;
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
      base::TimeDelta::FromSeconds(1));
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
    const CredentialWithPassword& credential) {
  api::passwords_private::InsecureCredential api_credential;
  auto facet = password_manager::FacetURI::FromPotentiallyInvalidSpec(
      credential.signon_realm);
  if (facet.IsValidAndroidFacetURI()) {
    api_credential.is_android_credential = true;
    // |formatted_orgin|, |detailed_origin| and |change_password_url| need
    // special handling for Android. Here we use affiliation information
    // instead of the origin.
    const PasswordForm& android_form =
        insecure_credentials_manager_.GetSavedPasswordsFor(credential)[0];
    if (!android_form.app_display_name.empty()) {
      api_credential.formatted_origin = android_form.app_display_name;
      api_credential.detailed_origin = android_form.app_display_name;
      api_credential.change_password_url =
          GetChangePasswordUrl(GURL(android_form.affiliated_web_realm));
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
            credential.url.GetOrigin(),
            url_formatter::kFormatUrlOmitDefaults |
                url_formatter::kFormatUrlOmitHTTPS |
                url_formatter::kFormatUrlOmitTrivialSubdomains |
                url_formatter::kFormatUrlTrimAfterHost,
            net::UnescapeRule::SPACES, nullptr, nullptr, nullptr));
    api_credential.detailed_origin = base::UTF16ToUTF8(
        url_formatter::FormatUrlForSecurityDisplay(credential.url.GetOrigin()));
    api_credential.change_password_url = GetChangePasswordUrl(credential.url);
  }

  api_credential.id = insecure_credential_id_generator_.GenerateId(credential);
  api_credential.signon_realm = credential.signon_realm;
  api_credential.username = base::UTF16ToUTF8(credential.username);

  return api_credential;
}

}  // namespace extensions
