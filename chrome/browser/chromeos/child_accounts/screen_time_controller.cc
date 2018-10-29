// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/child_accounts/screen_time_controller.h"

#include "ash/public/cpp/vector_icons/vector_icons.h"
#include "base/optional.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service.h"
#include "chrome/browser/chromeos/child_accounts/consumer_status_reporting_service_factory.h"
#include "chrome/browser/chromeos/profiles/profile_helper.h"
#include "chrome/browser/notifications/notification_display_service.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/login_screen_client.h"
#include "chrome/common/pref_names.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/dbus/dbus_thread_manager.h"
#include "chromeos/dbus/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/base/l10n/time_format.h"
#include "ui/message_center/public/cpp/notification.h"

namespace chromeos {

namespace {

constexpr base::TimeDelta kWarningNotificationTimeout =
    base::TimeDelta::FromMinutes(5);
constexpr base::TimeDelta kExitNotificationTimeout =
    base::TimeDelta::FromMinutes(1);

// The notification id. All the time limit notifications share the same id so
// that a subsequent notification can replace the previous one.
constexpr char kTimeLimitNotificationId[] = "time-limit-notification";

// The notifier id representing the app.
constexpr char kTimeLimitNotifierId[] = "family-link";

// Dictionary keys for prefs::kScreenTimeLastState.
constexpr char kScreenStateLocked[] = "locked";
constexpr char kScreenStateCurrentPolicyType[] = "active_policy";
constexpr char kScreenStateTimeUsageLimitEnabled[] = "time_usage_limit_enabled";
constexpr char kScreenStateRemainingUsage[] = "remaining_usage";
constexpr char kScreenStateUsageLimitStarted[] = "usage_limit_started";
constexpr char kScreenStateNextStateChangeTime[] = "next_state_change_time";
constexpr char kScreenStateNextPolicyType[] = "next_active_policy";
constexpr char kScreenStateNextUnlockTime[] = "next_unlock_time";
constexpr char kScreenStateLastStateChanged[] = "last_state_changed";

}  // namespace

// static
void ScreenTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kUsageTimeLimit);
  registry->RegisterDictionaryPref(prefs::kScreenTimeLastState);
}

ScreenTimeController::ScreenTimeController(content::BrowserContext* context)
    : context_(context),
      pref_service_(Profile::FromBrowserContext(context)->GetPrefs()) {
  session_manager::SessionManager::Get()->AddObserver(this);
  system::TimezoneSettings::GetInstance()->AddObserver(this);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUsageTimeLimit,
      base::BindRepeating(&ScreenTimeController::OnPolicyChanged,
                          base::Unretained(this)));
}

ScreenTimeController::~ScreenTimeController() {
  session_manager::SessionManager::Get()->RemoveObserver(this);
  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

base::TimeDelta ScreenTimeController::GetScreenTimeDuration() {
  return ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->GetChildScreenTime();
}

void ScreenTimeController::CheckTimeLimit(const std::string& source) {
  VLOG(1) << "Checking time limits (source=" << source << ")";

  // Stop all timers. They will be rescheduled below.
  ResetStateTimers();
  ResetInSessionTimers();

  base::Time now = base::Time::Now();
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  base::Optional<usage_time_limit::State> last_state = GetLastStateFromPref();
  const base::DictionaryValue* time_limit =
      pref_service_->GetDictionary(prefs::kUsageTimeLimit);

  usage_time_limit::State state = usage_time_limit::GetState(
      time_limit->CreateDeepCopy(), GetScreenTimeDuration(), now, now,
      &time_zone, last_state);
  SaveCurrentStateToPref(state);

  // Show/hide time limits message based on the policy enforcement.
  UpdateTimeLimitsMessage(
      state.is_locked, state.is_locked ? state.next_unlock_time : base::Time());
  VLOG(1) << "Screen should be locked is set to " << state.is_locked;

  if (state.is_locked) {
    DCHECK(!state.next_unlock_time.is_null());
    if (!session_manager::SessionManager::Get()->IsScreenLocked()) {
      VLOG(1) << "Request status report before locking screen.";
      ConsumerStatusReportingServiceFactory::GetForBrowserContext(context_)
          ->RequestImmediateStatusReport();
      ForceScreenLockByPolicy(state.next_unlock_time);
    }
  } else {
    base::Optional<TimeLimitNotificationType> notification_type;
    switch (state.next_state_active_policy) {
      case usage_time_limit::ActivePolicies::kFixedLimit:
        notification_type = kBedTime;
        break;
      case usage_time_limit::ActivePolicies::kUsageLimit:
        notification_type = kScreenTime;
        break;
      case usage_time_limit::ActivePolicies::kNoActivePolicy:
      case usage_time_limit::ActivePolicies::kOverride:
        break;
      default:
        NOTREACHED();
    }

    if (notification_type.has_value()) {
      // Schedule notification based on the remaining screen time until lock.
      const base::TimeDelta remaining_time = state.next_state_change_time - now;
      if (remaining_time >= kWarningNotificationTimeout) {
        warning_notification_timer_.Start(
            FROM_HERE, remaining_time - kWarningNotificationTimeout,
            base::BindRepeating(
                &ScreenTimeController::ShowNotification, base::Unretained(this),
                notification_type.value(), kWarningNotificationTimeout));
      }
      if (remaining_time >= kExitNotificationTimeout) {
        exit_notification_timer_.Start(
            FROM_HERE, remaining_time - kExitNotificationTimeout,
            base::BindRepeating(
                &ScreenTimeController::ShowNotification, base::Unretained(this),
                notification_type.value(), kExitNotificationTimeout));
      }
    }
  }

  base::Time next_get_state_time =
      std::min(state.next_state_change_time,
               usage_time_limit::GetExpectedResetTime(
                   time_limit->CreateDeepCopy(), now, &time_zone));
  if (!next_get_state_time.is_null()) {
    VLOG(1) << "Scheduling state change timer in "
            << state.next_state_change_time - now;
    next_state_timer_.Start(
        FROM_HERE, next_get_state_time - now,
        base::BindRepeating(&ScreenTimeController::CheckTimeLimit,
                            base::Unretained(this), "next_state_timer_"));
  }
}

void ScreenTimeController::ForceScreenLockByPolicy(
    base::Time next_unlock_time) {
  DCHECK(!session_manager::SessionManager::Get()->IsScreenLocked());
  chromeos::DBusThreadManager::Get()
      ->GetSessionManagerClient()
      ->RequestLockScreen();

  // Update the time limits message when the lock screen UI is ready.
  next_unlock_time_ = next_unlock_time;
}

void ScreenTimeController::UpdateTimeLimitsMessage(
    bool visible,
    base::Time next_unlock_time) {
  DCHECK(visible || next_unlock_time.is_null());
  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  AccountId account_id =
      chromeos::ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromBrowserContext(context_))
          ->GetAccountId();
  LoginScreenClient::Get()->login_screen()->SetAuthEnabledForUser(
      account_id, !visible,
      visible ? next_unlock_time : base::Optional<base::Time>());
}

void ScreenTimeController::ShowNotification(
    ScreenTimeController::TimeLimitNotificationType type,
    const base::TimeDelta& time_remaining) {
  const base::string16 title = l10n_util::GetStringUTF16(
      type == kScreenTime ? IDS_SCREEN_TIME_NOTIFICATION_TITLE
                          : IDS_BED_TIME_NOTIFICATION_TITLE);
  std::unique_ptr<message_center::Notification> notification =
      message_center::Notification::CreateSystemNotification(
          message_center::NOTIFICATION_TYPE_SIMPLE, kTimeLimitNotificationId,
          title,
          ui::TimeFormat::Simple(ui::TimeFormat::FORMAT_DURATION,
                                 ui::TimeFormat::LENGTH_LONG, time_remaining),
          l10n_util::GetStringUTF16(IDS_TIME_LIMIT_NOTIFICATION_DISPLAY_SOURCE),
          GURL(),
          message_center::NotifierId(
              message_center::NotifierId::SYSTEM_COMPONENT,
              kTimeLimitNotifierId),
          message_center::RichNotificationData(),
          new message_center::NotificationDelegate(),
          ash::kNotificationSupervisedUserIcon,
          message_center::SystemNotificationWarningLevel::NORMAL);
  NotificationDisplayService::GetForProfile(
      Profile::FromBrowserContext(context_))
      ->Display(NotificationHandler::Type::TRANSIENT, *notification);
}

void ScreenTimeController::OnPolicyChanged() {
  CheckTimeLimit("OnPolicyChanged");
}

void ScreenTimeController::ResetStateTimers() {
  VLOG(1) << "Stopping state timers";
  next_state_timer_.Stop();
}

void ScreenTimeController::ResetInSessionTimers() {
  VLOG(1) << "Stopping in-session timers";
  warning_notification_timer_.Stop();
  exit_notification_timer_.Stop();
}

void ScreenTimeController::SaveCurrentStateToPref(
    const usage_time_limit::State& state) {
  auto state_dict =
      std::make_unique<base::Value>(base::Value::Type::DICTIONARY);

  state_dict->SetKey(kScreenStateLocked, base::Value(state.is_locked));
  state_dict->SetKey(kScreenStateCurrentPolicyType,
                     base::Value(static_cast<int>(state.active_policy)));
  state_dict->SetKey(kScreenStateTimeUsageLimitEnabled,
                     base::Value(state.is_time_usage_limit_enabled));
  state_dict->SetKey(kScreenStateRemainingUsage,
                     base::Value(base::checked_cast<int>(
                         state.remaining_usage.InMilliseconds())));
  state_dict->SetKey(kScreenStateUsageLimitStarted,
                     base::Value(state.time_usage_limit_started.ToDoubleT()));
  state_dict->SetKey(kScreenStateNextStateChangeTime,
                     base::Value(state.next_state_change_time.ToDoubleT()));
  state_dict->SetKey(
      kScreenStateNextPolicyType,
      base::Value(static_cast<int>(state.next_state_active_policy)));
  state_dict->SetKey(kScreenStateNextUnlockTime,
                     base::Value(state.next_unlock_time.ToDoubleT()));
  state_dict->SetKey(kScreenStateLastStateChanged,
                     base::Value(state.last_state_changed.ToDoubleT()));

  pref_service_->Set(prefs::kScreenTimeLastState, *state_dict);
  pref_service_->CommitPendingWrite();
}

base::Optional<usage_time_limit::State>
ScreenTimeController::GetLastStateFromPref() {
  const base::DictionaryValue* last_state =
      pref_service_->GetDictionary(prefs::kScreenTimeLastState);
  usage_time_limit::State result;
  if (last_state->empty())
    return base::nullopt;

  // Verify is_locked from the pref is a boolean value.
  const base::Value* is_locked = last_state->FindKey(kScreenStateLocked);
  if (!is_locked || !is_locked->is_bool())
    return base::nullopt;
  result.is_locked = is_locked->GetBool();

  // Verify active policy type is a value of usage_time_limit::ActivePolicies.
  const base::Value* active_policy =
      last_state->FindKey(kScreenStateCurrentPolicyType);
  // TODO(crbug.com/823536): Add kCount in usage_time_limit::ActivePolicies
  // instead of checking kUsageLimit here.
  if (!active_policy || !active_policy->is_int() ||
      active_policy->GetInt() < 0 ||
      active_policy->GetInt() >
          static_cast<int>(usage_time_limit::ActivePolicies::kUsageLimit)) {
    return base::nullopt;
  }
  result.active_policy =
      static_cast<usage_time_limit::ActivePolicies>(active_policy->GetInt());

  // Verify time_usage_limit_enabled from the pref is a boolean value.
  const base::Value* time_usage_limit_enabled =
      last_state->FindKey(kScreenStateTimeUsageLimitEnabled);
  if (!time_usage_limit_enabled || !time_usage_limit_enabled->is_bool())
    return base::nullopt;
  result.is_time_usage_limit_enabled = time_usage_limit_enabled->GetBool();

  // Verify remaining_usage from the pref is a int value.
  const base::Value* remaining_usage =
      last_state->FindKey(kScreenStateRemainingUsage);
  if (!remaining_usage || !remaining_usage->is_int())
    return base::nullopt;
  result.remaining_usage =
      base::TimeDelta::FromMilliseconds(remaining_usage->GetInt());

  // Verify time_usage_limit_started from the pref is a double value.
  const base::Value* time_usage_limit_started =
      last_state->FindKey(kScreenStateUsageLimitStarted);
  if (!time_usage_limit_started || !time_usage_limit_started->is_double())
    return base::nullopt;
  result.time_usage_limit_started =
      base::Time::FromDoubleT(time_usage_limit_started->GetDouble());

  // Verify next_state_change_time from the pref is a double value.
  const base::Value* next_state_change_time =
      last_state->FindKey(kScreenStateNextStateChangeTime);
  if (!next_state_change_time || !next_state_change_time->is_double())
    return base::nullopt;
  result.next_state_change_time =
      base::Time::FromDoubleT(next_state_change_time->GetDouble());

  // Verify next policy type is a value of usage_time_limit::ActivePolicies.
  const base::Value* next_active_policy =
      last_state->FindKey(kScreenStateNextPolicyType);
  if (!next_active_policy || !next_active_policy->is_int() ||
      next_active_policy->GetInt() < 0 ||
      next_active_policy->GetInt() >
          static_cast<int>(usage_time_limit::ActivePolicies::kUsageLimit)) {
    return base::nullopt;
  }
  result.next_state_active_policy =
      static_cast<usage_time_limit::ActivePolicies>(
          next_active_policy->GetInt());

  // Verify next_unlock_time from the pref is a double value.
  const base::Value* next_unlock_time =
      last_state->FindKey(kScreenStateNextUnlockTime);
  if (!next_unlock_time || !next_unlock_time->is_double())
    return base::nullopt;
  result.next_unlock_time =
      base::Time::FromDoubleT(next_unlock_time->GetDouble());

  // Verify last_state_changed from the pref is a double value.
  const base::Value* last_state_changed =
      last_state->FindKey(kScreenStateLastStateChanged);
  if (!last_state_changed || !last_state_changed->is_double())
    return base::nullopt;
  result.last_state_changed =
      base::Time::FromDoubleT(last_state_changed->GetDouble());
  return result;
}

void ScreenTimeController::OnSessionStateChanged() {
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  if (session_state == session_manager::SessionState::LOCKED) {
    if (next_unlock_time_) {
      UpdateTimeLimitsMessage(true /*visible*/, next_unlock_time_.value());
      next_unlock_time_.reset();
    }
    ResetInSessionTimers();
  } else if (session_state == session_manager::SessionState::ACTIVE) {
    CheckTimeLimit("OnSessionStateChanged");
  }
}

void ScreenTimeController::TimezoneChanged(const icu::TimeZone& timezone) {
  CheckTimeLimit("TimezoneChanged");
}

}  // namespace chromeos
