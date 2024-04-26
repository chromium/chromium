// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/saml/in_session_password_change_manager.h"

#include <algorithm>
#include <memory>
#include <string>
#include <utility>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/session/session_controller.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "base/task/task_traits.h"
#include "base/time/time.h"
#include "chrome/browser/ash/login/login_pref_names.h"
#include "chrome/browser/ash/login/saml/password_change_success_notification.h"
#include "chrome/browser/ash/login/saml/password_expiry_notification.h"
#include "chrome/browser/ash/login/saml/password_sync_token_fetcher.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/browser_process_platform_part_ash.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/webui/ash/in_session_password_change/password_change_dialogs.h"
#include "chrome/common/chrome_features.h"
#include "chromeos/ash/components/login/auth/auth_session_authenticator.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/key.h"
#include "chromeos/ash/components/login/auth/public/saml_password_attributes.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/prefs/pref_service.h"
#include "components/user_manager/known_user.h"
#include "components/user_manager/user_manager.h"
#include "content/public/browser/browser_task_traits.h"
#include "content/public/browser/browser_thread.h"

namespace ash {
namespace {

using PasswordSource = InSessionPasswordChangeManager::PasswordSource;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused. This must be kept in sync with
// SamlInSessionPasswordChangeEvent in tools/metrics/histograms/enums.xml
enum class InSessionPasswordChangeEvent {
  kManagerCreated = 0,
  kNotified = 1,
  kUrgentNotified = 2,
  kNotifiedAlreadyExpired = 3,
  kStartPasswordChange = 4,
  kSamlPasswordChanged = 5,
  kPasswordScrapeSuccess = 6,
  kPasswordScrapePartial = 7,
  kPasswordScrapeFailure = 8,
  kCryptohomePasswordChangeSuccessScraped = 9,
  kCryptohomePasswordChangeSuccessRetyped = 10,
  kCryptohomePasswordChangeFailureScraped = 11,
  kCryptohomePasswordChangeFailureRetyped = 12,
  kFinishPasswordChange = 13,
  kMaxValue = kFinishPasswordChange,
};

void RecordEvent(InSessionPasswordChangeEvent event) {
  UMA_HISTOGRAM_ENUMERATION("ChromeOS.SAML.InSessionPasswordChangeEvent",
                            event);
}

void RecordNotificationShown(bool is_expired, bool show_as_urgent) {
  if (is_expired) {
    RecordEvent(InSessionPasswordChangeEvent::kNotifiedAlreadyExpired);
  } else if (show_as_urgent) {
    RecordEvent(InSessionPasswordChangeEvent::kUrgentNotified);
  } else {
    RecordEvent(InSessionPasswordChangeEvent::kNotified);
  }
}

void RecordScrapingResult(bool old_password_scraped,
                          bool new_password_scraped) {
  if (old_password_scraped && new_password_scraped) {
    RecordEvent(InSessionPasswordChangeEvent::kPasswordScrapeSuccess);
  } else if (old_password_scraped || new_password_scraped) {
    RecordEvent(InSessionPasswordChangeEvent::kPasswordScrapePartial);
  } else {
    RecordEvent(InSessionPasswordChangeEvent::kPasswordScrapeFailure);
  }
}

void RecordCryptohomePasswordChangeSuccess(PasswordSource password_source) {
  if (password_source == PasswordSource::PASSWORDS_SCRAPED) {
    RecordEvent(
        InSessionPasswordChangeEvent::kCryptohomePasswordChangeSuccessScraped);
  } else {
    RecordEvent(
        InSessionPasswordChangeEvent::kCryptohomePasswordChangeSuccessRetyped);
  }
}

void RecordCryptohomePasswordChangeFailure(PasswordSource password_source) {
  if (password_source == PasswordSource::PASSWORDS_SCRAPED) {
    RecordEvent(
        InSessionPasswordChangeEvent::kCryptohomePasswordChangeFailureScraped);
  } else {
    RecordEvent(
        InSessionPasswordChangeEvent::kCryptohomePasswordChangeFailureRetyped);
  }
}

InSessionPasswordChangeManager* g_test_instance = nullptr;

// Traits for running RecheckPasswordExpiryTask.
// Runs from the UI thread to show notification.
const content::BrowserTaskTraits kRecheckUITaskTraits = {
    base::TaskPriority::BEST_EFFORT};

// A time delta of length one hour.
const base::TimeDelta kOneHour = base::Hours(1);

// A time delta of length one day.
const base::TimeDelta kOneDay = base::Days(1);

// A time delta with length of a half day.
const base::TimeDelta kHalfDay = base::Hours(12);

// A time delta with length zero.
const base::TimeDelta kZeroTime = base::TimeDelta();

// When the password will expire in `kUrgentWarningDays` or less, the
// UrgentPasswordExpiryNotification will be used - which is larger and actually
// a dialog (not a true notification) - instead of the normal notification.
const int kUrgentWarningDays = 3;

// Rounds to the nearest day - eg plus or minus 12 hours is zero days, 12 to 36
// hours is 1 day, -12 to -36 hours is -1 day, etc.
inline int RoundToDays(base::TimeDelta time_delta) {
  return (time_delta + kHalfDay).InDaysFloored();
}

}  // namespace

RecheckPasswordExpiryTask::RecheckPasswordExpiryTask() = default;

RecheckPasswordExpiryTask::~RecheckPasswordExpiryTask() = default;

void RecheckPasswordExpiryTask::Recheck() {
  CancelPendingRecheck();
  InSessionPasswordChangeManager::Get()->MaybeShowExpiryNotification();
}

void RecheckPasswordExpiryTask::RecheckAfter(base::TimeDelta delay) {
  CancelPendingRecheck();
  content::GetUIThreadTaskRunner(kRecheckUITaskTraits)
      ->PostDelayedTask(FROM_HERE,
                        base::BindOnce(&RecheckPasswordExpiryTask::Recheck,
                                       weak_ptr_factory_.GetWeakPtr()),
                        std::max(delay, kOneHour));
  // This always waits at least one hour before calling Recheck again - we don't
  // want some bug to cause this code to run every millisecond.
}

void RecheckPasswordExpiryTask::CancelPendingRecheck() {
  weak_ptr_factory_.InvalidateWeakPtrs();
}

// static
std::unique_ptr<InSessionPasswordChangeManager>
InSessionPasswordChangeManager::CreateIfEnabled(Profile* primary_profile) {
  if (base::FeatureList::IsEnabled(::features::kInSessionPasswordChange) &&
      primary_profile->GetPrefs()->GetBoolean(
          prefs::kSamlInSessionPasswordChangeEnabled)) {
    std::unique_ptr<InSessionPasswordChangeManager> manager =
        std::make_unique<InSessionPasswordChangeManager>(primary_profile);
    manager->MaybeShowExpiryNotification();
    RecordEvent(InSessionPasswordChangeEvent::kManagerCreated);
    return manager;
  } else {
    // If the policy is disabled, clear the SAML password attributes.
    SamlPasswordAttributes::DeleteFromPrefs(primary_profile->GetPrefs());
    return nullptr;
  }
}

// static
bool InSessionPasswordChangeManager::IsInitialized() {
  return GetNullable();
}

// static
InSessionPasswordChangeManager* InSessionPasswordChangeManager::Get() {
  InSessionPasswordChangeManager* result = GetNullable();
  CHECK(result);
  return result;
}

InSessionPasswordChangeManager::InSessionPasswordChangeManager(
    Profile* primary_profile)
    : primary_profile_(primary_profile),
      primary_user_(ProfileHelper::Get()->GetUserByProfile(primary_profile)),
      urgent_warning_days_(kUrgentWarningDays) {
  DCHECK(primary_user_);

  // Add `this` as a SessionActivationObserver to see when the screen is locked.
  auto* session_controller = SessionController::Get();
  if (session_controller) {
    session_controller->AddSessionActivationObserverForAccountId(
        primary_user_->GetAccountId(), this);
  }
}

InSessionPasswordChangeManager::~InSessionPasswordChangeManager() {
  // Remove `this` as a SessionActivationObserver.
  auto* session_controller = SessionController::Get();
  if (session_controller) {
    session_controller->RemoveSessionActivationObserverForAccountId(
        primary_user_->GetAccountId(), this);
  }
}

// static
void InSessionPasswordChangeManager::SetForTesting(
    InSessionPasswordChangeManager* instance) {
  CHECK(!g_test_instance);
  g_test_instance = instance;
}

// static
void InSessionPasswordChangeManager::ResetForTesting() {
  g_test_instance = nullptr;
}

void InSessionPasswordChangeManager::MaybeShowExpiryNotification() {
  // We are checking password expiry now, and this function will decide if we
  // want to check again in the future, so for now, make sure there are no other
  // pending tasks to check aggain.
  recheck_task_.CancelPendingRecheck();

  PrefService* prefs = primary_profile_->GetPrefs();
  if (!prefs->GetBoolean(prefs::kSamlInSessionPasswordChangeEnabled)) {
    DismissExpiryNotification();
    return;
  }

  SamlPasswordAttributes attrs = SamlPasswordAttributes::LoadFromPrefs(prefs);
  if (!attrs.has_expiration_time()) {
    DismissExpiryNotification();
    return;
  }

  // Calculate how many days until the password will expire.
  const base::TimeDelta time_until_expiry =
      attrs.expiration_time() - base::Time::Now();
  const int days_until_expiry = RoundToDays(time_until_expiry);
  const int advance_warning_days =
      prefs->GetInteger(prefs::kSamlPasswordExpirationAdvanceWarningDays);

  bool is_expired = time_until_expiry <= kZeroTime;
  // Show notification if a) expired, or b) advance_warning_days is set > 0 and
  // we are now within advance_warning_days of the expiry time.
  const bool show_notification =
      is_expired ||
      (advance_warning_days > 0 && days_until_expiry <= advance_warning_days);

  if (show_notification) {
    // Show as urgent if urgent_warning_days is set > 0 and we are now within
    // urgent_warning_days of the expiry time.
    const bool show_as_urgent =
        (urgent_warning_days_ > 0 && days_until_expiry <= urgent_warning_days_);
    if (show_as_urgent) {
      ShowUrgentExpiryNotification();
    } else {
      ShowStandardExpiryNotification(time_until_expiry);
    }
    RecordNotificationShown(is_expired, show_as_urgent);

    // We check again whether to reshow / update the notification after one day:
    recheck_task_.RecheckAfter(kOneDay);

  } else {
    // We have not yet reached the advance warning threshold. Check again
    // once we have arrived at expiry_time minus advance_warning_days...
    base::TimeDelta recheck_delay =
        time_until_expiry - base::Days(advance_warning_days);
    // But, wait an extra hour so that when this code is next run, it is clear
    // we are now inside advance_warning_days (and not right on the boundary).
    recheck_delay += kOneHour;
    recheck_task_.RecheckAfter(recheck_delay);
  }
}

void InSessionPasswordChangeManager::ShowStandardExpiryNotification(
    base::TimeDelta time_until_expiry) {
  // Show a notification, and reshow it each time the screen is unlocked.
  renotify_on_unlock_ = true;
  PasswordExpiryNotification::Show(primary_profile_, time_until_expiry);
  UrgentPasswordExpiryNotificationDialog::Dismiss();
}

void InSessionPasswordChangeManager::ShowUrgentExpiryNotification() {
  // Show a notification, and reshow it each time the screen is unlocked.
  renotify_on_unlock_ = true;
  UrgentPasswordExpiryNotificationDialog::Show();
  PasswordExpiryNotification::Dismiss(primary_profile_);
}

void InSessionPasswordChangeManager::DismissExpiryNotification() {
  UrgentPasswordExpiryNotificationDialog::Dismiss();
  PasswordExpiryNotification::Dismiss(primary_profile_);
}

void InSessionPasswordChangeManager::OnExpiryNotificationDismissedByUser() {
  // When a notification is dismissed, we then don't pop it up again each time
  // the user unlocks the screen.
  renotify_on_unlock_ = false;
}

void InSessionPasswordChangeManager::OnScreenUnlocked() {
  if (renotify_on_unlock_) {
    MaybeShowExpiryNotification();
  }
}

void InSessionPasswordChangeManager::StartInSessionPasswordChange() {
  RecordEvent(InSessionPasswordChangeEvent::kStartPasswordChange);
  NotifyObservers(Event::START_SAML_IDP_PASSWORD_CHANGE);
  DismissExpiryNotification();
  PasswordChangeDialog::Show();
  ConfirmPasswordChangeDialog::Dismiss();
}

void InSessionPasswordChangeManager::OnSamlPasswordChanged(
    const std::string& scraped_old_password,
    const std::string& scraped_new_password) {
  RecordEvent(InSessionPasswordChangeEvent::kSamlPasswordChanged);
  NotifyObservers(Event::SAML_IDP_PASSWORD_CHANGED);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      primary_user_->GetAccountId(), true);
  DismissExpiryNotification();
  PasswordChangeDialog::Dismiss();

  RecordScrapingResult(!scraped_old_password.empty(),
                       !scraped_new_password.empty());

  const bool both_passwords_scraped =
      !scraped_old_password.empty() && !scraped_new_password.empty();
  if (both_passwords_scraped) {
    // Both passwords scraped so we try to change cryptohome password now.
    // Show the confirm dialog but initially showing a spinner. If the change
    // fails, then the dialog will hide the spinner and show a prompt.
    // If the change succeeds, the dialog and spinner will just disappear.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/true);
    ChangePassword(scraped_old_password, scraped_new_password,
                   PasswordSource::PASSWORDS_SCRAPED);
  } else {
    // Failed to scrape passwords - prompt for passwords immediately.
    ConfirmPasswordChangeDialog::Show(scraped_old_password,
                                      scraped_new_password,
                                      /*show_spinner_initially=*/false);
  }
}

void InSessionPasswordChangeManager::ChangePassword(
    const std::string& old_password,
    const std::string& new_password,
    PasswordSource password_source) {
  CHECK(!old_password.empty() && !new_password.empty());

  password_source_ = password_source;
  NotifyObservers(Event::START_CRYPTOHOME_PASSWORD_CHANGE);
  auto user_context = std::make_unique<UserContext>(*primary_user_);
  // TODO(b/258638651): Consider getting rid of `Key` usage here, and passing
  // the new password to `password_update_flow_` directly.
  user_context->SetKey(Key(new_password));
  password_update_flow_.Start(
      std::move(user_context), old_password,
      base::BindOnce(&InSessionPasswordChangeManager::OnPasswordUpdateSuccess,
                     weak_ptr_factory_.GetWeakPtr()),
      base::BindOnce(&InSessionPasswordChangeManager::OnPasswordUpdateFailure,
                     weak_ptr_factory_.GetWeakPtr()));
}

void InSessionPasswordChangeManager::AddObserver(Observer* observer) {
  observer_list_.AddObserver(observer);
}

void InSessionPasswordChangeManager::RemoveObserver(Observer* observer) {
  observer_list_.RemoveObserver(observer);
}

void InSessionPasswordChangeManager::OnPasswordUpdateFailure(
    std::unique_ptr<UserContext> /*user_context*/,
    AuthenticationError error) {
  VLOG(1) << "Failed to change cryptohome password: "
          << error.get_cryptohome_error();
  RecordCryptohomePasswordChangeFailure(password_source_);
  NotifyObservers(Event::CRYPTOHOME_PASSWORD_CHANGE_FAILURE);
}

void InSessionPasswordChangeManager::OnPasswordUpdateSuccess(
    std::unique_ptr<UserContext> user_context) {
  DCHECK(user_context);
  VLOG(3) << "Cryptohome password is changed.";
  RecordCryptohomePasswordChangeSuccess(password_source_);
  NotifyObservers(Event::CRYPTOHOME_PASSWORD_CHANGED);

  user_manager::UserManager::Get()->SaveForceOnlineSignin(
      user_context->GetAccountId(), false);

  // Clear expiration time from prefs so that we don't keep nagging the user to
  // change password (until the SAML provider tells us a new expiration time).
  SamlPasswordAttributes loaded =
      SamlPasswordAttributes::LoadFromPrefs(primary_profile_->GetPrefs());
  SamlPasswordAttributes(
      /*modified_time=*/base::Time::Now(), /*expiration_time=*/base::Time(),
      loaded.password_change_url())
      .SaveToPrefs(primary_profile_->GetPrefs());

  DismissExpiryNotification();
  PasswordChangeDialog::Dismiss();
  ConfirmPasswordChangeDialog::Dismiss();
  if (features::IsSamlNotificationOnPasswordChangeSuccessEnabled()) {
    PasswordChangeSuccessNotification::Show(primary_profile_);
  }
  // We request a new sync token. It will be updated locally and signal the fact
  // of password change to other devices owned by the user.
  CreateTokenAsync();
  RecordEvent(InSessionPasswordChangeEvent::kFinishPasswordChange);
}

void InSessionPasswordChangeManager::OnSessionActivated(bool activated) {
  // Not needed.
}

void InSessionPasswordChangeManager::OnLockStateChanged(bool locked) {
  if (!locked) {
    OnScreenUnlocked();
  }
}

void InSessionPasswordChangeManager::OnTokenCreated(
    const std::string& sync_token) {
  // Set token value in local state.
  user_manager::KnownUser known_user(g_browser_process->local_state());
  known_user.SetPasswordSyncToken(primary_user_->GetAccountId(), sync_token);
}

void InSessionPasswordChangeManager::OnTokenFetched(
    const std::string& sync_token) {
  // Ignored.
}

void InSessionPasswordChangeManager::OnTokenVerified(bool is_valid) {
  // Ignored.
}

void InSessionPasswordChangeManager::OnApiCallFailed(
    PasswordSyncTokenFetcher::ErrorType error_type) {
  // TODO(crbug.com/40143230): Error types will be tracked by UMA histograms.
  // Going forward we should also consider re-trying token creation depending on
  // the error_type.
}

void InSessionPasswordChangeManager::CreateTokenAsync() {
  password_sync_token_fetcher_ = std::make_unique<PasswordSyncTokenFetcher>(
      primary_profile_->GetURLLoaderFactory(), primary_profile_, this);
  password_sync_token_fetcher_->StartTokenCreate();
}

// static
InSessionPasswordChangeManager* InSessionPasswordChangeManager::GetNullable() {
  return g_test_instance ? g_test_instance
                         : g_browser_process->platform_part()
                               ->in_session_password_change_manager();
}

void InSessionPasswordChangeManager::NotifyObservers(Event event) {
  for (auto& observer : observer_list_) {
    observer.OnEvent(event);
  }
}

}  // namespace ash
