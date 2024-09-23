// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/child_accounts/screen_time_controller.h"

#include <algorithm>
#include <string>
#include <utility>

#include "ash/public/cpp/child_accounts/parent_access_controller.h"
#include "ash/public/cpp/login_screen.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/clock.h"
#include "base/time/default_clock.h"
#include "base/timer/timer.h"
#include "base/trace_event/trace_event.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service.h"
#include "chrome/browser/ash/child_accounts/child_status_reporting_service_factory.h"
#include "chrome/browser/ash/child_accounts/time_limit_override.h"
#include "chrome/browser/ash/login/lock/screen_locker.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/common/pref_names.h"
#include "chromeos/ash/components/dbus/session_manager/session_manager_client.h"
#include "components/prefs/pref_registry_simple.h"
#include "components/prefs/pref_service.h"
#include "components/session_manager/core/session_manager.h"
#include "content/public/browser/browser_context.h"

namespace ash {

namespace {

constexpr base::TimeDelta kUsageTimeLimitWarningTime = base::Minutes(15);

// Dictionary keys for prefs::kScreenTimeLastState.
constexpr char kScreenStateLocked[] = "locked";
constexpr char kScreenStateCurrentPolicyType[] = "active_policy";
constexpr char kScreenStateTimeUsageLimitEnabled[] = "time_usage_limit_enabled";
constexpr char kScreenStateRemainingUsage[] = "remaining_usage";
constexpr char kScreenStateUsageLimitStarted[] = "usage_limit_started";
constexpr char kScreenStateNextStateChangeTime[] = "next_state_change_time";
constexpr char kScreenStateNextPolicyType[] = "next_active_policy";
constexpr char kScreenStateNextUnlockTime[] = "next_unlock_time";

AuthDisabledReason ConvertLockReason(
    usage_time_limit::PolicyType active_policy) {
  switch (active_policy) {
    case usage_time_limit::PolicyType::kFixedLimit:
      return AuthDisabledReason::kTimeWindowLimit;
    case usage_time_limit::PolicyType::kUsageLimit:
      return AuthDisabledReason::kTimeUsageLimit;
    case usage_time_limit::PolicyType::kOverride:
      return AuthDisabledReason::kTimeLimitOverride;
    case usage_time_limit::PolicyType::kNoPolicy:
      break;
  }
  NOTREACHED_IN_MIGRATION();
  return AuthDisabledReason();
}

}  // namespace

// static
void ScreenTimeController::RegisterProfilePrefs(PrefRegistrySimple* registry) {
  registry->RegisterDictionaryPref(prefs::kScreenTimeLastState);
  registry->RegisterDictionaryPref(prefs::kTimeLimitLocalOverride);
  registry->RegisterDictionaryPref(prefs::kUsageTimeLimit);
}

ScreenTimeController::ScreenTimeController(content::BrowserContext* context)
    : context_(context),
      pref_service_(Profile::FromBrowserContext(context)->GetPrefs()),
      clock_(base::DefaultClock::GetInstance()),
      next_state_timer_(std::make_unique<base::OneShotTimer>()),
      usage_time_limit_warning_timer_(std::make_unique<base::OneShotTimer>()),
      last_policy_(pref_service_->GetDict(prefs::kUsageTimeLimit).Clone()),
      time_limit_notifier_(context) {
  session_manager::SessionManager::Get()->AddObserver(this);
  UsageTimeStateNotifier::GetInstance()->AddObserver(this);

  system::TimezoneSettings::GetInstance()->AddObserver(this);
  SystemClockClient::Get()->AddObserver(this);
  pref_change_registrar_.Init(pref_service_);
  pref_change_registrar_.Add(
      prefs::kUsageTimeLimit,
      base::BindRepeating(&ScreenTimeController::OnPolicyChanged,
                          base::Unretained(this)));

  parent_access::ParentAccessService::Get().AddObserver(this);
}

ScreenTimeController::~ScreenTimeController() {
  parent_access::ParentAccessService::Get().RemoveObserver(this);

  session_manager::SessionManager::Get()->RemoveObserver(this);
  UsageTimeStateNotifier::GetInstance()->RemoveObserver(this);

  system::TimezoneSettings::GetInstance()->RemoveObserver(this);
  SystemClockClient::Get()->RemoveObserver(this);
}

void ScreenTimeController::AddObserver(Observer* observer) {
  DCHECK(observer);
  observers_.AddObserver(observer);
}

void ScreenTimeController::RemoveObserver(Observer* observer) {
  DCHECK(observer);
  observers_.RemoveObserver(observer);
}

base::TimeDelta ScreenTimeController::GetScreenTimeDuration() {
  return ChildStatusReportingServiceFactory::GetForBrowserContext(context_)
      ->GetChildScreenTime();
}

void ScreenTimeController::SetClocksForTesting(
    const base::Clock* clock,
    const base::TickClock* tick_clock,
    scoped_refptr<base::SequencedTaskRunner> task_runner) {
  clock_ = clock;
  next_state_timer_ = std::make_unique<base::OneShotTimer>(tick_clock);
  next_state_timer_->SetTaskRunner(task_runner);
  usage_time_limit_warning_timer_ =
      std::make_unique<base::OneShotTimer>(tick_clock);
  usage_time_limit_warning_timer_->SetTaskRunner(task_runner);
}

void ScreenTimeController::NotifyUsageTimeLimitWarningForTesting() {
  for (Observer& observer : observers_)
    observer.UsageTimeLimitWarning();
}

void ScreenTimeController::CheckTimeLimit(const std::string& source) {
  VLOG(1) << "Checking time limits (source=" << source << ")";

  // Stop all timers. They will be rescheduled below.
  ResetStateTimers();
  ResetInSessionTimers();
  ResetWarningTimers();

  base::Time now = clock_->Now();
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  std::optional<usage_time_limit::State> last_state = GetLastStateFromPref();
  const base::Value::Dict& time_limit =
      pref_service_->GetDict(prefs::kUsageTimeLimit);
  const base::Value::Dict& local_override =
      pref_service_->GetDict(prefs::kTimeLimitLocalOverride);

  // TODO(agawronska): Usage timestamp should be passed instead of second |now|.
  usage_time_limit::State state = usage_time_limit::GetState(
      time_limit, &local_override, GetScreenTimeDuration(), now, now,
      &time_zone, last_state);
  SaveCurrentStateToPref(state);

  VLOG(1) << "Screen should be locked is set to " << state.is_locked;

  if (state.is_locked) {
    OnScreenLockByPolicy(state.active_policy, state.next_unlock_time);
    DCHECK(!state.next_unlock_time.is_null());
    if (!session_manager::SessionManager::Get()->IsScreenLocked())
      ForceScreenLockByPolicy();
  } else {
    OnScreenLockByPolicyEnd();
    std::optional<TimeLimitNotifier::LimitType> notification_type =
        ConvertPolicyType(state.next_state_active_policy);
    if (notification_type.has_value()) {
      // Schedule notification based on the remaining screen time until lock.
      // TODO(crbug.com/41422189): Dismiss a shown notification when it no
      // longer applies.
      const base::TimeDelta remaining_time = state.next_state_change_time - now;
      time_limit_notifier_.MaybeScheduleLockNotifications(
          notification_type.value(), remaining_time);
    }

    ScheduleUsageTimeLimitWarning(state);
  }

  // Trigger policy update notifications.
  auto updated_policy_types =
      usage_time_limit::UpdatedPolicyTypes(last_policy_, time_limit);
  for (const auto& policy_type : updated_policy_types) {
    std::optional<base::Time> lock_time;
    if (policy_type == usage_time_limit::PolicyType::kOverride)
      lock_time = state.next_state_change_time;

    std::optional<TimeLimitNotifier::LimitType> notification_type =
        ConvertPolicyType(policy_type);
    DCHECK(notification_type);
    time_limit_notifier_.ShowPolicyUpdateNotification(notification_type.value(),
                                                      lock_time);
  }
  last_policy_ = time_limit.Clone();

  // TODO(agawronska): We are creating UsageTimeLimitProcessor second time in
  // this method. Could expected reset time be returned as a part of the state?
  base::Time next_get_state_time =
      std::min(state.next_state_change_time,
               usage_time_limit::GetExpectedResetTime(
                   time_limit, &local_override, now, &time_zone));
  if (!next_get_state_time.is_null()) {
    VLOG(1) << "Scheduling state change timer in " << next_get_state_time - now;
    next_state_timer_->Start(
        FROM_HERE, next_get_state_time - now,
        base::BindOnce(&ScreenTimeController::CheckTimeLimit,
                       base::Unretained(this), "next_state_timer_"));
  }
}

void ScreenTimeController::ForceScreenLockByPolicy() {
  DCHECK(!session_manager::SessionManager::Get()->IsScreenLocked());

  // Avoid abrupt session restart that looks like a crash and happens when lock
  // screen is requested before sign in completion. It is safe, because time
  // limits will be reevaluated when session state changes to active.
  // TODO(agawronska): Remove the flag when it is confirmed that this does not
  // cause a bug (https://crbug.com/924844).
  if (base::FeatureList::IsEnabled(features::kDMServerOAuthForChildUser) &&
      session_manager::SessionManager::Get()->session_state() !=
          session_manager::SessionState::ACTIVE) {
    return;
  }

  SessionManagerClient::Get()->RequestLockScreen();
}

void ScreenTimeController::OnAccessCodeValidation(
    ParentCodeValidationResult result,
    std::optional<AccountId> account_id) {
  AccountId current_user_id =
      ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromBrowserContext(context_))
          ->GetAccountId();
  if (result != ParentCodeValidationResult::kValid || !account_id ||
      account_id.value() != current_user_id)
    return;

  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  usage_time_limit::TimeLimitOverride local_override(
      usage_time_limit::TimeLimitOverride::Action::kUnlock, clock_->Now(),
      std::nullopt);
  // Replace previous local override stored in pref, because PAC can only be
  // entered if previous override is not active anymore.
  pref_service_->SetDict(prefs::kTimeLimitLocalOverride,
                         local_override.ToDictionary());
  pref_service_->CommitPendingWrite();

  CheckTimeLimit("OnAccessCodeValidation");
}

void ScreenTimeController::OnScreenLockByPolicy(
    usage_time_limit::PolicyType active_policy,
    base::Time next_unlock_time) {
  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  // Show lock message.
  AccountId account_id =
      ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromBrowserContext(context_))
          ->GetAccountId();
  ScreenLocker::default_screen_locker()->TemporarilyDisableAuthForUser(
      account_id, AuthDisabledData(ConvertLockReason(active_policy),
                                   next_unlock_time, GetScreenTimeDuration(),
                                   true /*disable_lock_screen_media*/));

  // Add parent access code button.
  // TODO(agawronska): Move showing shelf button to ash.
  LoginScreen::Get()->ShowParentAccessButton(true);
}

void ScreenTimeController::OnScreenLockByPolicyEnd() {
  if (!session_manager::SessionManager::Get()->IsScreenLocked())
    return;

  AccountId account_id =
      ProfileHelper::Get()
          ->GetUserByProfile(Profile::FromBrowserContext(context_))
          ->GetAccountId();
  ScreenLocker::default_screen_locker()->ReenableAuthForUser(account_id);

  // TODO(agawronska): Move showing shelf button to ash.
  LoginScreen::Get()->ShowParentAccessButton(false);
}

void ScreenTimeController::OnPolicyChanged() {
  CheckTimeLimit("OnPolicyChanged");
}

void ScreenTimeController::ResetStateTimers() {
  VLOG(1) << "Stopping state timers";
  next_state_timer_->Stop();
}

void ScreenTimeController::ResetWarningTimers() {
  VLOG(1) << "Stopping warning timers";
  usage_time_limit_warning_timer_->Stop();
}

void ScreenTimeController::ResetInSessionTimers() {
  VLOG(1) << "Stopping in-session timers";
  time_limit_notifier_.UnscheduleNotifications();
}

void ScreenTimeController::ScheduleUsageTimeLimitWarning(
    const usage_time_limit::State& state) {
  if (state.next_state_active_policy ==
          usage_time_limit::PolicyType::kUsageLimit &&
      UsageTimeStateNotifier::GetInstance()->GetState() ==
          UsageTimeStateNotifier::UsageTimeState::ACTIVE) {
    base::Time now = clock_->Now();
    base::TimeDelta time_until_next_state = state.next_state_change_time - now;
    base::TimeDelta delay =
        time_until_next_state < kUsageTimeLimitWarningTime
            ? base::TimeDelta()
            : time_until_next_state - kUsageTimeLimitWarningTime;

    VLOG(1) << "Scheduling usage time limit warning in " << delay.InMinutes()
            << " minutes.";
    usage_time_limit_warning_timer_->Start(
        FROM_HERE, delay,
        base::BindOnce(&ScreenTimeController::UsageTimeLimitWarning,
                       base::Unretained(this)));
  }
}

void ScreenTimeController::SaveCurrentStateToPref(
    const usage_time_limit::State& state) {
  base::Value::Dict state_dict;

  state_dict.Set(kScreenStateLocked, base::Value(state.is_locked));
  state_dict.Set(kScreenStateCurrentPolicyType,
                 static_cast<int>(state.active_policy));
  state_dict.Set(kScreenStateTimeUsageLimitEnabled,
                 state.is_time_usage_limit_enabled);
  state_dict.Set(
      kScreenStateRemainingUsage,
      base::checked_cast<int>(state.remaining_usage.InMilliseconds()));
  state_dict.Set(kScreenStateUsageLimitStarted,
                 state.time_usage_limit_started.InSecondsFSinceUnixEpoch());
  state_dict.Set(kScreenStateNextStateChangeTime,
                 state.next_state_change_time.InSecondsFSinceUnixEpoch());
  state_dict.Set(kScreenStateNextPolicyType,
                 static_cast<int>(state.next_state_active_policy));
  state_dict.Set(kScreenStateNextUnlockTime,
                 state.next_unlock_time.InSecondsFSinceUnixEpoch());

  pref_service_->SetDict(prefs::kScreenTimeLastState, std::move(state_dict));
  pref_service_->CommitPendingWrite();
}

std::optional<usage_time_limit::State>
ScreenTimeController::GetLastStateFromPref() {
  const base::Value::Dict& last_state =
      pref_service_->GetDict(prefs::kScreenTimeLastState);
  usage_time_limit::State result;
  if (last_state.empty())
    return std::nullopt;

  // Verify is_locked from the pref is a boolean value.
  const base::Value* is_locked = last_state.Find(kScreenStateLocked);
  if (!is_locked || !is_locked->is_bool())
    return std::nullopt;
  result.is_locked = is_locked->GetBool();

  // Verify active policy type is a value of usage_time_limit::PolicyType.
  const base::Value* active_policy =
      last_state.Find(kScreenStateCurrentPolicyType);
  // TODO(crbug.com/40567736): Add kCount in usage_time_limit::PolicyType
  // instead of checking kUsageLimit here.
  if (!active_policy || !active_policy->is_int() ||
      active_policy->GetInt() < 0 ||
      active_policy->GetInt() >
          static_cast<int>(usage_time_limit::PolicyType::kUsageLimit)) {
    return std::nullopt;
  }
  result.active_policy =
      static_cast<usage_time_limit::PolicyType>(active_policy->GetInt());

  // Verify time_usage_limit_enabled from the pref is a boolean value.
  const base::Value* time_usage_limit_enabled =
      last_state.Find(kScreenStateTimeUsageLimitEnabled);
  if (!time_usage_limit_enabled || !time_usage_limit_enabled->is_bool())
    return std::nullopt;
  result.is_time_usage_limit_enabled = time_usage_limit_enabled->GetBool();

  // Verify remaining_usage from the pref is a int value.
  const base::Value* remaining_usage =
      last_state.Find(kScreenStateRemainingUsage);
  if (!remaining_usage || !remaining_usage->is_int())
    return std::nullopt;
  result.remaining_usage = base::Milliseconds(remaining_usage->GetInt());

  // Verify time_usage_limit_started from the pref is a double value.
  const base::Value* time_usage_limit_started =
      last_state.Find(kScreenStateUsageLimitStarted);
  if (!time_usage_limit_started || !time_usage_limit_started->is_double())
    return std::nullopt;
  result.time_usage_limit_started = base::Time::FromSecondsSinceUnixEpoch(
      time_usage_limit_started->GetDouble());

  // Verify next_state_change_time from the pref is a double value.
  const base::Value* next_state_change_time =
      last_state.Find(kScreenStateNextStateChangeTime);
  if (!next_state_change_time || !next_state_change_time->is_double())
    return std::nullopt;
  result.next_state_change_time = base::Time::FromSecondsSinceUnixEpoch(
      next_state_change_time->GetDouble());

  // Verify next policy type is a value of usage_time_limit::PolicyType.
  const base::Value* next_active_policy =
      last_state.Find(kScreenStateNextPolicyType);
  if (!next_active_policy || !next_active_policy->is_int() ||
      next_active_policy->GetInt() < 0 ||
      next_active_policy->GetInt() >
          static_cast<int>(usage_time_limit::PolicyType::kUsageLimit)) {
    return std::nullopt;
  }
  result.next_state_active_policy =
      static_cast<usage_time_limit::PolicyType>(next_active_policy->GetInt());

  // Verify next_unlock_time from the pref is a double value.
  const base::Value* next_unlock_time =
      last_state.Find(kScreenStateNextUnlockTime);
  if (!next_unlock_time || !next_unlock_time->is_double())
    return std::nullopt;
  result.next_unlock_time =
      base::Time::FromSecondsSinceUnixEpoch(next_unlock_time->GetDouble());
  return result;
}

void ScreenTimeController::UsageTimeLimitWarning() {
  base::Time now = clock_->Now();
  const icu::TimeZone& time_zone =
      system::TimezoneSettings::GetInstance()->GetTimezone();
  const base::Value::Dict& time_limit =
      pref_service_->GetDict(prefs::kUsageTimeLimit);
  const base::Value::Dict& local_override =
      pref_service_->GetDict(prefs::kTimeLimitLocalOverride);

  std::optional<base::TimeDelta> remaining_usage =
      usage_time_limit::GetRemainingTimeUsage(time_limit, &local_override, now,
                                              GetScreenTimeDuration(),
                                              &time_zone);

  // Remaining time usage can be bigger than |kUsageTimeLimitWarningTime|
  // because it is counted in another class so the timers might be called with
  // some delay. We must ensure that the observers will be called when the
  // usage time is less than |kUsageTimeLimitWarningTime|.
  if (remaining_usage && remaining_usage < kUsageTimeLimitWarningTime) {
    for (Observer& observer : observers_)
      observer.UsageTimeLimitWarning();
  } else {
    CheckTimeLimit("UsageTimeLimitWarning");
  }
}

std::optional<TimeLimitNotifier::LimitType>
ScreenTimeController::ConvertPolicyType(
    usage_time_limit::PolicyType policy_type) {
  switch (policy_type) {
    case usage_time_limit::PolicyType::kFixedLimit:
      return std::make_optional(TimeLimitNotifier::LimitType::kBedTime);
    case usage_time_limit::PolicyType::kUsageLimit:
      return std::make_optional(TimeLimitNotifier::LimitType::kScreenTime);
    case usage_time_limit::PolicyType::kOverride:
      return std::make_optional(TimeLimitNotifier::LimitType::kOverride);
    case usage_time_limit::PolicyType::kNoPolicy:
      return std::nullopt;
  }
}

void ScreenTimeController::OnSessionStateChanged() {
  TRACE_EVENT0("ui", "ScreenTimeController::OnSessionStateChanged");
  session_manager::SessionState session_state =
      session_manager::SessionManager::Get()->session_state();
  std::optional<usage_time_limit::State> last_state = GetLastStateFromPref();
  if (session_state == session_manager::SessionState::LOCKED && last_state &&
      last_state->is_locked) {
    OnScreenLockByPolicy(last_state->active_policy,
                         last_state->next_unlock_time);
  }
}

void ScreenTimeController::OnUsageTimeStateChange(
    const UsageTimeStateNotifier::UsageTimeState state) {
  if (state == UsageTimeStateNotifier::UsageTimeState::INACTIVE) {
    ResetWarningTimers();
    ResetInSessionTimers();
  } else {
    CheckTimeLimit("OnUsageTimeStateChange");
  }
}

void ScreenTimeController::TimezoneChanged(const icu::TimeZone& timezone) {
  CheckTimeLimit("TimezoneChanged");
}

void ScreenTimeController::SystemClockUpdated() {
  CheckTimeLimit("SystemClockUpdated");
}

}  // namespace ash
