// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_update_checker/device_scheduled_update_checker.h"

#include <time.h>

#include <algorithm>
#include <utility>

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/values.h"
#include "chrome/browser/chromeos/policy/scheduled_update_checker/task_executor_with_retries.h"
#include "chromeos/settings/cros_settings_names.h"
#include "chromeos/settings/timezone_settings.h"
#include "services/device/public/mojom/constants.mojom.h"
#include "services/service_manager/public/cpp/connector.h"
#include "third_party/icu/source/i18n/unicode/gregocal.h"
#include "third_party/icu/source/i18n/unicode/ucal.h"

namespace policy {

namespace {

// The tag associated to register |update_check_timer_|.
constexpr char kUpdateCheckTimerTag[] = "DeviceScheduledUpdateChecker";

// Reason associated to acquire |ScopedWakeLock|.
constexpr char kWakeLockReason[] = "DeviceScheduledUpdateChecker";

DeviceScheduledUpdateChecker::Frequency GetFrequency(
    const std::string& frequency) {
  if (frequency == "DAILY")
    return DeviceScheduledUpdateChecker::Frequency::kDaily;

  if (frequency == "WEEKLY")
    return DeviceScheduledUpdateChecker::Frequency::kWeekly;

  DCHECK_EQ(frequency, "MONTHLY");
  return DeviceScheduledUpdateChecker::Frequency::kMonthly;
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

// Returns true iff a >= b.
bool IsCalGreaterThanEqual(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  if (a.after(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  if (a.equals(b, status)) {
    DCHECK(U_SUCCESS(status));
    return true;
  }

  return false;
}

// Advances |time| based on the policy represented by
// |scheduled_update_check_data|.
//
// For daily policy - Advances |time| by 1 day.
// For weekly policy - Advances |time| by 1 week.
// For monthly policy - Advances |time| by 1 month.
//
// Returns true on success and false if it failed to set a valid time.
bool AdvanceTimeBasedOnPolicy(
    const DeviceScheduledUpdateChecker::ScheduledUpdateCheckData&
        scheduled_update_check_data,
    icu::Calendar* time) {
  UCalendarDateFields field = UCAL_MONTH;
  switch (scheduled_update_check_data.frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily:
      field = UCAL_DAY_OF_MONTH;
      break;
    case DeviceScheduledUpdateChecker::Frequency::kWeekly:
      field = UCAL_WEEK_OF_YEAR;
      break;
    case DeviceScheduledUpdateChecker::Frequency::kMonthly:
      break;
  }
  UErrorCode status = U_ZERO_ERROR;
  time->add(field, 1, status);
  return U_SUCCESS(status);
}

// Sets |time| based on the policy represented by |scheduled_update_check_data|.
// Returns true on success and false if it failed to set a valid time.
bool SetTimeBasedOnPolicy(
    const DeviceScheduledUpdateChecker::ScheduledUpdateCheckData&
        scheduled_update_check_data,
    icu::Calendar* time) {
  // Set the daily fields first as they will be common across different policy
  // types.
  time->set(UCAL_HOUR_OF_DAY, scheduled_update_check_data.hour);
  time->set(UCAL_MINUTE, scheduled_update_check_data.minute);
  time->set(UCAL_SECOND, 0);
  time->set(UCAL_MILLISECOND, 0);

  switch (scheduled_update_check_data.frequency) {
    case DeviceScheduledUpdateChecker::Frequency::kDaily:
      return true;

    case DeviceScheduledUpdateChecker::Frequency::kWeekly:
      DCHECK(scheduled_update_check_data.day_of_week);
      time->set(UCAL_DAY_OF_WEEK,
                scheduled_update_check_data.day_of_week.value());
      return true;

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      DCHECK(scheduled_update_check_data.day_of_month);
      UErrorCode status = U_ZERO_ERROR;
      // If policy's |day_of_month| is greater than the maximum days in |time|'s
      // current month then it's set to the last day in the month.
      int cur_max_days_in_month =
          time->getActualMaximum(UCAL_DAY_OF_MONTH, status);
      if (U_FAILURE(status)) {
        LOG(ERROR) << "Failed to get max days in month";
        return false;
      }

      time->set(UCAL_DAY_OF_MONTH,
                std::min(scheduled_update_check_data.day_of_month.value(),
                         cur_max_days_in_month));
      return true;
    }
  }
}

}  // namespace

namespace update_checker_internal {

base::Optional<DeviceScheduledUpdateChecker::ScheduledUpdateCheckData>
ParseScheduledUpdate(const base::Value& value) {
  DeviceScheduledUpdateChecker::ScheduledUpdateCheckData result;
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
    case DeviceScheduledUpdateChecker::Frequency::kDaily:
      break;

    case DeviceScheduledUpdateChecker::Frequency::kWeekly: {
      const std::string* day_of_week = value.FindStringKey({"day_of_week"});
      if (!day_of_week) {
        LOG(ERROR) << "Day of week missing";
        return base::nullopt;
      }

      // Validated by schema validation at higher layers.
      result.day_of_week = StringDayOfWeekToIcuDayOfWeek(*day_of_week);
      break;
    }

    case DeviceScheduledUpdateChecker::Frequency::kMonthly: {
      base::Optional<int> day_of_month = value.FindIntKey({"day_of_month"});
      if (!day_of_month) {
        LOG(ERROR) << "Day of month missing";
        return base::nullopt;
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

base::TimeDelta GetDiff(const icu::Calendar& a, const icu::Calendar& b) {
  UErrorCode status = U_ZERO_ERROR;
  UDate a_ms = a.getTime(status);
  DCHECK(U_SUCCESS(status));
  UDate b_ms = b.getTime(status);
  DCHECK(U_SUCCESS(status));
  DCHECK(a_ms >= b_ms);
  return base::TimeDelta::FromMilliseconds(a_ms - b_ms);
}

std::unique_ptr<icu::Calendar> ConvertUtcToTzIcuTime(base::Time cur_time,
                                                     const icu::TimeZone& tz) {
  // Get ms from epoch for |cur_time| and use it to get the new time in |tz|.
  UErrorCode status = U_ZERO_ERROR;
  std::unique_ptr<icu::Calendar> cal_tz =
      std::make_unique<icu::GregorianCalendar>(tz, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }
  // Erase current time from the calendar.
  cal_tz->clear();
  time_t ms_from_epoch = cur_time.ToTimeT() * 1000;
  cal_tz->setTime(ms_from_epoch, status);
  if (U_FAILURE(status)) {
    LOG(ERROR) << "Couldn't create calendar";
    return nullptr;
  }

  return cal_tz;
}

}  // namespace update_checker_internal

// |cros_settings_observer_| will be destroyed as part of this object
// guaranteeing to not run |OnScheduledUpdateCheckDataChanged| after its
// destruction. Therefore, it's safe to use "this" while adding this observer.
// Similarly, |start_update_check_timer_task_executor_| and
// |os_and_policies_update_checker_| will be destroyed as part of this object,
// so it's safe to use "this" with any callbacks.
DeviceScheduledUpdateChecker::DeviceScheduledUpdateChecker(
    chromeos::CrosSettings* cros_settings,
    chromeos::NetworkStateHandler* network_state_handler,
    service_manager::Connector* connector)
    : cros_settings_(cros_settings),
      connector_(connector),
      cros_settings_observer_(cros_settings_->AddSettingsObserver(
          chromeos::kDeviceScheduledUpdateCheck,
          base::BindRepeating(
              &DeviceScheduledUpdateChecker::OnScheduledUpdateCheckDataChanged,
              base::Unretained(this)))),
      start_update_check_timer_task_executor_(
          update_checker_internal::kMaxStartUpdateCheckTimerRetryIterations,
          update_checker_internal::kStartUpdateCheckTimerRetryTime),
      os_and_policies_update_checker_(network_state_handler) {
  chromeos::system::TimezoneSettings::GetInstance()->AddObserver(this);
  // Check if policy already exists.
  OnScheduledUpdateCheckDataChanged();
}

DeviceScheduledUpdateChecker::~DeviceScheduledUpdateChecker() {
  chromeos::system::TimezoneSettings::GetInstance()->RemoveObserver(this);
}

DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ScheduledUpdateCheckData() = default;
DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ScheduledUpdateCheckData(const ScheduledUpdateCheckData&) = default;
DeviceScheduledUpdateChecker::ScheduledUpdateCheckData::
    ~ScheduledUpdateCheckData() = default;

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
          ScopedWakeLock(connector_,
                         device::mojom::WakeLockType::kPreventAppSuspension,
                         kWakeLockReason)),
      update_checker_internal::kOsAndPoliciesUpdateCheckHardTimeout);
}

void DeviceScheduledUpdateChecker::TimezoneChanged(
    const icu::TimeZone& time_zone) {
  // Anytime the time zone changes, the update check timer delay should be
  // recalculated and the timer should be started with updated values according
  // to the new time zone.
  // |scheduled_update_check_data_->next_update_check_time_ticks| also needs to
  // be reset, as it would be incorrect in the context of a new time zone. For
  // this purpose, treat it as a new policy and call
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
  base::Optional<ScheduledUpdateCheckData> scheduled_update_check_data =
      update_checker_internal::ParseScheduledUpdate(*value);
  if (!scheduled_update_check_data) {
    LOG(ERROR) << "Failed to parse policy";
    return;
  }

  // Policy has been updated, calculate and set |update_check_timer_| again.
  scheduled_update_check_data_ = std::move(scheduled_update_check_data);
  MaybeStartUpdateCheckTimer(
      ScopedWakeLock(connector_,
                     device::mojom::WakeLockType::kPreventAppSuspension,
                     kWakeLockReason),
      false /* is_retry */);
}

base::TimeDelta
DeviceScheduledUpdateChecker::CalculateNextUpdateCheckTimerDelay(
    base::Time cur_time) {
  DCHECK(scheduled_update_check_data_);

  const auto cur_cal =
      update_checker_internal::ConvertUtcToTzIcuTime(cur_time, GetTimeZone());
  if (!cur_cal) {
    LOG(ERROR) << "Failed to get current ICU time";
    return update_checker_internal::kInvalidDelay;
  }

  auto update_check_time = base::WrapUnique(cur_cal->clone());
  DCHECK(update_check_time);

  // Set update check time based on the policy in
  // |scheduled_update_check_data_|.
  if (!SetTimeBasedOnPolicy(scheduled_update_check_data_.value(),
                            update_check_time.get())) {
    LOG(ERROR) << "Failed to set time based on policy";
    return update_checker_internal::kInvalidDelay;
  }

  // If the time has already passed it means that the update check needs to be
  // advanced based on the policy i.e. by a day, week or month. The equal to
  // case happens when the |OnUpdateCheckTimerExpired| runs and sets the next
  // |update_check_timer_|. In this case |update_check_time| definitely needs to
  // advance as per the policy. The |SetTimeBasedOnPolicy| is needed for the
  // monthly frequency, it won't change the time after advancing for daily or
  // weekly frequencies. For monthly, if the current time is Feb 28, 1970, 8PM
  // and an update check needs to happen on 7PM every 31st, then setting time
  // above and advancing time below gets us a time of Mar 28, 1970, 7PM. An
  // extra call to |SetTimeBasedOnPolicy| is required to finally get Mar 31,
  // 1970 7PM.
  if (IsCalGreaterThanEqual(*cur_cal, *update_check_time)) {
    if (!AdvanceTimeBasedOnPolicy(scheduled_update_check_data_.value(),
                                  update_check_time.get())) {
      LOG(ERROR) << "Failed to advance time";
      return update_checker_internal::kInvalidDelay;
    }

    if (!SetTimeBasedOnPolicy(scheduled_update_check_data_.value(),
                              update_check_time.get())) {
      LOG(ERROR) << "Failed to set time based on policy";
      return update_checker_internal::kInvalidDelay;
    }
  }
  DCHECK(!IsCalGreaterThanEqual(*cur_cal, *update_check_time));

  return update_checker_internal::GetDiff(*update_check_time, *cur_cal);
}

void DeviceScheduledUpdateChecker::StartUpdateCheckTimer(
    ScopedWakeLock scoped_wake_lock) {
  // The device shouldn't suspend while calculating time ticks and setting the
  // timer for the next update check. Otherwise the next update check timer will
  // be inaccurately scheduled. Hence, a wake lock must always be held for this
  // entire task. The wake lock could already be held at this
  // point due to |OnUpdateCheckTimerExpired|.

  // Only one |StartUpdateCheckTimer| can be outstanding.
  update_check_timer_.reset();

  DCHECK(scheduled_update_check_data_);

  // For accuracy of the next update check, capture current time as close to
  // the start of this function as possible.
  const base::TimeTicks cur_ticks = GetTicksSinceBoot();
  const base::Time cur_time = GetCurrentTime();

  // If this is a retry then |cur_ticks| could be >=
  // |next_update_check_time_ticks| i.e. the next timer schedule has already
  // passed, recalculate it. Else respect the calculated time.
  if (cur_ticks >= scheduled_update_check_data_->next_update_check_time_ticks) {
    // Calculate the next update check time. In case there is an error while
    // calculating, due to concurrent DST or Time Zone changes, then reschedule
    // this function and try to schedule the update check again. There should
    // only be one outstanding task to start the timer. If there is a failure
    // the wake lock is released and acquired again when this task runs.
    base::TimeDelta delay = CalculateNextUpdateCheckTimerDelay(cur_time);
    if (delay <= update_checker_internal::kInvalidDelay) {
      LOG(ERROR) << "Failed to calculate next update check time";
      MaybeStartUpdateCheckTimer(std::move(scoped_wake_lock),
                                 true /* is_retry */);
      return;
    }
    scheduled_update_check_data_->next_update_check_time_ticks =
        cur_ticks + delay;
  }

  // |update_check_timer_| will be destroyed as part of this object and is
  // guaranteed to not run callbacks after its destruction. Therefore, it's
  // safe to use "this" while starting the timer.
  update_check_timer_ =
      std::make_unique<chromeos::NativeTimer>(kUpdateCheckTimerTag);
  update_check_timer_->Start(
      scheduled_update_check_data_->next_update_check_time_ticks,
      base::BindOnce(&DeviceScheduledUpdateChecker::OnUpdateCheckTimerExpired,
                     base::Unretained(this)),
      base::BindOnce(
          &DeviceScheduledUpdateChecker::OnUpdateCheckTimerStartResult,
          base::Unretained(this), std::move(scoped_wake_lock)));
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
  update_check_timer_.reset();
  scheduled_update_check_data_ = base::nullopt;
  os_and_policies_update_checker_.Stop();
  start_update_check_timer_task_executor_.Stop();
}

base::Time DeviceScheduledUpdateChecker::GetCurrentTime() {
  return base::Time::Now();
}

base::TimeTicks DeviceScheduledUpdateChecker::GetTicksSinceBoot() {
  struct timespec ts = {};
  int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_NE(ret, 0);
  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(ts);
}

const icu::TimeZone& DeviceScheduledUpdateChecker::GetTimeZone() {
  return chromeos::system::TimezoneSettings::GetInstance()->GetTimezone();
}

}  // namespace policy
