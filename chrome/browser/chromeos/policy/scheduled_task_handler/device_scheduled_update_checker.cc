// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_task_handler/device_scheduled_update_checker.h"

#include <time.h>

#include <algorithm>
#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/scheduled_task_handler/scheduled_task_executor.h"
#include "chrome/browser/chromeos/policy/scheduled_task_handler/task_executor_with_retries.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace policy {

namespace {

// Reason associated to acquire |ScopedWakeLock|.
constexpr char kWakeLockReason[] = "DeviceScheduledUpdateChecker";

ScheduledTaskExecutor::Frequency GetFrequency(const std::string& frequency) {
  if (frequency == "DAILY")
    return ScheduledTaskExecutor::Frequency::kDaily;

  if (frequency == "WEEKLY")
    return ScheduledTaskExecutor::Frequency::kWeekly;

  DCHECK_EQ(frequency, "MONTHLY");
  return ScheduledTaskExecutor::Frequency::kMonthly;
}

// Convert the string day of week to UCalendarDaysOfWeek.
UCalendarDaysOfWeek StringDayOfWeekToIcuDayOfWeek(
    const std::string& day_of_week) {
  if (day_of_week == "SUNDAY")
    return UCAL_SUNDAY;
  if (day_of_week == "MONDAY")
    return UCAL_MONDAY;
  if (day_of_week == "TUESDAY")
    return UCAL_TUESDAY;
  if (day_of_week == "WEDNESDAY")
    return UCAL_WEDNESDAY;
  if (day_of_week == "THURSDAY")
    return UCAL_THURSDAY;
  if (day_of_week == "FRIDAY")
    return UCAL_FRIDAY;
  DCHECK_EQ(day_of_week, "SATURDAY");
  return UCAL_SATURDAY;
}

}  // namespace

namespace update_checker_internal {

absl::optional<ScheduledTaskExecutor::ScheduledTaskData> ParseScheduledUpdate(
    const base::Value& value) {
  ScheduledTaskExecutor::ScheduledTaskData result;
  // Parse mandatory values first i.e. hour, minute and frequency of update
  // check. These should always be present due to schema validation at higher
  // layers.
  const base::Value* hour_value = value.FindPathOfType(
      {"update_check_time", "hour"}, base::Value::Type::INTEGER);
  DCHECK(hour_value);
  int hour = hour_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(hour >= 0 && hour <= 23);
  result.hour = hour;

  const base::Value* minute_value = value.FindPathOfType(
      {"update_check_time", "minute"}, base::Value::Type::INTEGER);
  DCHECK(minute_value);
  int minute = minute_value->GetInt();
  // Validated by schema validation at higher layers.
  DCHECK(minute >= 0 && minute <= 59);
  result.minute = minute;

  // Validated by schema validation at higher layers.
  const std::string* frequency = value.FindStringKey({"frequency"});
  DCHECK(frequency);
  result.frequency = GetFrequency(*frequency);

  // Parse extra fields for weekly and monthly frequencies.
  switch (result.frequency) {
    case ScheduledTaskExecutor::Frequency::kDaily:
      break;

    case ScheduledTaskExecutor::Frequency::kWeekly: {
      const std::string* day_of_week = value.FindStringKey({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return absl::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToIcuDayOfWeek(*day_of_week);
      break;
    }

    case ScheduledTaskExecutor::Frequency::kMonthly: {
      absl::optional<int> day_of_month = value.FindIntKey({"day_of_month"});
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return absl::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_month = day_of_month.value();
      break;
    }
  }

  return result;
}

// Converts an icu::Calendar to base::Time. Assumes |time| is a valid time.
base::Time IcuToBaseTime(const icu::Calendar& time) {
  UErrorCode status = U_ZERO_ERROR;
  UDate seconds_from_epoch = time.getTime(status) / 1000;
  DCHECK(U_SUCCESS(status));
  base::Time result = base::Time::FromTimeT(seconds_from_epoch);
  if (result.is_null())
    result = base::Time::UnixEpoch();
  return result;
}

}  // namespace update_checker_internal

// |cros_settings_subscription_| will be destroyed as part of this object
// guaranteeing to not run |OnScheduledUpdateCheckDataChanged| after its
// destruction. Therefore, it's safe to use "this" while adding this observer.
// Similarly, |start_update_check_timer_task_executor_| and
// |os_and_policies_update_checker_| will be destroyed as part of this object,
// so it's safe to use "this" with any callbacks.
DeviceScheduledUpdateChecker::DeviceScheduledUpdateChecker(
    ash::CrosSettings* cros_settings,
    chromeos::NetworkStateHandler* network_state_handler,
    std::unique_ptr<ScheduledTaskExecutor> update_check_executor)
    : cros_settings_(cros_settings),
      cros_settings_subscription_(cros_settings_->AddSettingsObserver(
          chromeos::kDeviceScheduledUpdateCheck,
          base::BindRepeating(
              &DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged,
              base::Unretained(this)))),
      start_update_check_timer_task_executor_(
          update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations,
          update_checker_internal::kStartUpdateCheckTimerRetryTime),
      os_and_policies_update_checker_(network_state_handler),
      update_check_executor_(std::move(update_check_executor)) {
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  // Check if policy already exists.
  OnScheduledUpdateCheckDataChanged();
}

DeviceScheduledUpdateChecker::~DeviceScheduledUpdateChecker() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

void DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired() {
  // Following things needs to be done on every update check event. These will
  // be done serially to make state management easier -
  // - Download updates and refresh policies.
  // - Calculate and start the next update check timer.
  // For both these operations a wake lock should be held to ensure that the
  // update check is completed successfully and the device doesn't suspend while
  // calculating time ticks and setting the timer for the next update check. The
  // wake lock is held even when the above two operations are retried on
  // failure. This decision was made to simplify the design.
  //
  // The wake lock will be released in |OnUpdateCheckTimerStartResult| either
  // due to successful completion of step 2 above or failure and letting the
  // device suspend and reacquire the wake lock again in
  // |StartUpdateCheckTimer|.

  // If no policy exists, state should have been reset and this callback
  // shouldn't have fired.
  DCHECK(scheduled_update_check_data_);

  // |os_and_policies_update_checker_| will be destroyed as part of this object,
  // so it's safe to use "this" with any callbacks. This overrides any previous
  // update check calls. Since, the minimum update frequency is daily there is
  // very little chance of stepping on an existing update check that hasn't
  // finished for a day. Timeouts will ensure that an update check completes,
  // successfully or unsuccessfully, way before a day.
  os_and_policies_update_checker_.Start(
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnUpdateCheckCompletion,
          base::Unretained(this),
          ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                         kWakeLockReason)),
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
}

void DeviceScheduledUpdateChecker::TimezoneChanged(
    const icu::TimeZone& time_zone) {
  // Anytime the time zone changes, the update check timer delay should be
  // recalculated and the timer should be started with updated values according
  // to the new time zone.
  // |scheduled_update_check_data_->next_scheduled_task_time_ticks| also needs
  // to be reset, as it would be incorrect in the context of a new time zone.
  // For this purpose, treat it as a new policy and call
  // |OnScheduledUpdateCheckDataChanged| instead of |MaybeStartUpdateCheckTimer|
  // directly.
  OnScheduledUpdateCheckDataChanged();
}

void DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged() {
  // If the policy is removed then reset all state including any existing update
  // checks.
  const base::Value* value =
      cros_settings_->GetPref(chromeos::kDeviceScheduledUpdateCheck);
  if (!value) {
    ResetState();
    return;
  }

  // Keep any old policy timers running if a new policy is ill-formed and can't
  // be used to set a new timer.
  absl::optional<ScheduledTaskExecutor::ScheduledTaskData>
      scheduled_update_check_data =
          update_checker_internal::ParseScheduledUpdate(*value);
  if (!scheduled_update_check_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set |update_check_timer_| again.
  scheduled_update_check_data_ = std::move(scheduled_update_check_data);
  MaybeStartUpdateCheckTimer(
      ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                     kWakeLockReason),
      false /* is_retry */);
}

void DeviceScheduledUpdateChecker::StartUpdateCheckTimer(
    ScopedWakeLock scoped_wake_lock) {
  // The device shouldn't suspend while calculating time ticks and setting the
  // timer for the next update check. Otherwise the next update check timer will
  // be inaccurately scheduled. Hence, a wake lock must always be held for this
  // entire task. The wake lock could already be held at this
  // point due to |OnUpdateCheckTimerExpired|.

  // |update_check_executor_| will be destroyed as part of this object and is
  // guaranteed to not run callbacks after its destruction. Therefore, it's
  // safe to use base::Unretained(this) when starting the executor.
  update_check_executor_->Start(
      &scheduled_update_check_data_.value(),
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnUpdateCheckTimerStartResult,
          base::Unretained(this), std::move(scoped_wake_lock)),
      base::BindOnce(&DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired,
                     base::Unretained(this)));
}

void DeviceScheduledUpdateChecker::OnUpdateCheckTimerStartResult(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // Schedule a retry if |update_check_timer_| failed to start.
  if (!result) {
    LOG(ERROR) << "Failed to start update check timer";
    MaybeStartUpdateCheckTimer(std::move(scoped_wake_lock),
                               true /* is_retry */);
  }
}

void DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure() {
  // Retrying has a limit. In the unlikely scenario this is met, the next update
  // check can only happen when a new policy comes in or Chrome is restarted.
  // The wake lock acquired in |MaybeStartUpdateCheckTimer| will be released
  // because |task| was destroyed in |start_update_check_timer_executor_|.
  LOG(ERROR) << "Failed to start update check timer after all retries";
  ResetState();
}

void DeviceScheduledUpdateChecker::MaybeStartUpdateCheckTimer(
    ScopedWakeLock scoped_wake_lock,
    bool is_retry) {
  // No need to start the next update check timer if the policy has been
  // removed. This can happen if an update check was ongoing, a new policy
  // came in but failed to start the timer which reset all state in
  // |OnStartUpdateCheckTimerRetryFailure|.
  if (!scheduled_update_check_data_)
    return;

  // Only start the timer if an existing update check is not running because if
  // it is, it will start the timer in |OnUpdateCheckCompletion|.
  if (os_and_policies_update_checker_.IsRunning())
    return;

  // Safe to use |this| as |start_update_check_timer_task_executor_| is a
  // member of |this|. Wake lock needs to be held to calculate next update check
  // timer accurately without the device suspending mid-calculation.
  if (is_retry) {
    start_update_check_timer_task_executor_.ScheduleRetry(
        base::BindOnce(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                       base::Unretained(this), std::move(scoped_wake_lock)));
  } else {
    start_update_check_timer_task_executor_.Start(
        base::BindOnce(&DeviceScheduledUpdateChecker::StartUpdateCheckTimer,
                       base::Unretained(this), std::move(scoped_wake_lock)),
        base::BindOnce(
            &DeviceScheduledUpdateChecker::OnStartUpdateCheckTimerRetryFailure,
            base::Unretained(this)));
  }
}

void DeviceScheduledUpdateChecker::OnUpdateCheckCompletion(
    ScopedWakeLock scoped_wake_lock,
    bool result) {
  // Start the next update check timer irrespective of the current update
  // check succeeding or not.
  LOG_IF(ERROR, !result) << "Update check failed";
  MaybeStartUpdateCheckTimer(std::move(scoped_wake_lock), false /* is_retry */);
}

void DeviceScheduledUpdateChecker::ResetState() {
  update_check_executor_->Reset();
  scheduled_update_check_data_ = absl::nullopt;
  os_and_policies_update_checker_.Stop();
  start_update_check_timer_task_executor_.Stop();
}

}  // namespace policy
