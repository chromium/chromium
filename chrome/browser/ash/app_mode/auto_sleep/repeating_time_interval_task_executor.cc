// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"

#include <memory>

#include "base/check.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/time/default_clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/dbus/power/native_timer.h"
#include "chromeos/dbus/power/power_manager_client.h"

namespace ash {

namespace {

base::TimeDelta GetDuration(const base::Time& start,
                            const policy::WeeklyTime& end) {
  policy::WeeklyTime start_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(start);
  return start_weekly_time.GetDurationTo(end);
}

// Returns true when the provided `time` is contained within the `interval`.
bool TimeFallsInInterval(const base::Time& time,
                         const policy::WeeklyTimeInterval& interval) {
  policy::WeeklyTime current_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(time);

  return interval.Contains(current_weekly_time);
}

}  // namespace

RepeatingTimeIntervalTaskExecutor::Factory::Factory() = default;

RepeatingTimeIntervalTaskExecutor::Factory::~Factory() = default;

std::unique_ptr<RepeatingTimeIntervalTaskExecutor>
RepeatingTimeIntervalTaskExecutor::Factory::Create(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback) {
  return std::make_unique<RepeatingTimeIntervalTaskExecutor>(
      time_interval, on_interval_start_callback, on_interval_end_callback);
}

RepeatingTimeIntervalTaskExecutor::RepeatingTimeIntervalTaskExecutor(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback)
    : clock_(base::DefaultClock::GetInstance()),
      timer_(std::make_unique<base::WallClockTimer>()),
      time_interval_(time_interval),
      on_interval_start_callback_(on_interval_start_callback),
      on_interval_end_callback_(on_interval_end_callback) {
  CHECK(on_interval_start_callback_);
  CHECK(on_interval_end_callback_);
  CHECK(system::TimezoneSettings::GetInstance());
  timezone_observer_.Observe(system::TimezoneSettings::GetInstance());
  last_known_time_zone_id_ =
      system::TimezoneSettings::GetInstance()->GetCurrentTimezoneID();
}

RepeatingTimeIntervalTaskExecutor::~RepeatingTimeIntervalTaskExecutor() =
    default;

void RepeatingTimeIntervalTaskExecutor::ScheduleTimer() {
  timer_scheduled_ = true;
  base::Time current_time = clock_->Now();

  if (TimeFallsInInterval(current_time, time_interval_)) {
    IntervalStartsNow();
  } else {
    IntervalStartsLater();
  }
}

void RepeatingTimeIntervalTaskExecutor::TimezoneChanged(
    const icu::TimeZone& timezone) {
  std::u16string updated_timezone_id =
      system::TimezoneSettings::GetInstance()->GetTimezoneID(timezone);
  if (!timer_scheduled_ || updated_timezone_id == last_known_time_zone_id_) {
    return;
  }

  last_known_time_zone_id_ = updated_timezone_id;

  // Notify the power manager of user activity to make sure any
  // requests to suspend the device are cancelled so that the invariant of the
  // timer waking up the device when the timer ends is maintained.
  chromeos::PowerManagerClient::Get()->NotifyUserActivity(
      power_manager::USER_ACTIVITY_OTHER);

  timer_->Stop();
  if (has_interval_end_timer_started_) {
    this->on_interval_end_callback_.Run();
    has_interval_end_timer_started_ = false;
  }

  ScheduleTimer();
}

void RepeatingTimeIntervalTaskExecutor::IntervalStartsNow() {
  has_interval_end_timer_started_ = true;
  on_interval_start_callback_.Run(
      GetDuration(clock_->Now(), time_interval_.end()));

  // Also start a wall clock timer to the end of the interval so that we can
  // also schedule the timer for next week.
  StartTimer(
      time_interval_.end(),
      base::BindOnce(
          &RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerFinish,
          weak_ptr_factory_.GetWeakPtr()));
}

void RepeatingTimeIntervalTaskExecutor::IntervalStartsLater() {
  StartTimer(time_interval_.start(),
             base::BindOnce(&RepeatingTimeIntervalTaskExecutor::ScheduleTimer,
                            weak_ptr_factory_.GetWeakPtr()));
}

void RepeatingTimeIntervalTaskExecutor::StartTimer(
    policy::WeeklyTime expiration_time,
    base::OnceClosure timer_expiration_callback) {
  auto next_scheduled_time =
      clock_->Now() + GetDuration(clock_->Now(), expiration_time);
  timer_->Start(FROM_HERE, next_scheduled_time,
                std::move(timer_expiration_callback));
}

void RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerFinish() {
  has_interval_end_timer_started_ = false;
  on_interval_end_callback_.Run();
  ScheduleTimer();
}

}  // namespace ash
