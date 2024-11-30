// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/weekly_interval_timer.h"

#include <memory>
#include <string>

#include "base/check.h"
#include "base/check_deref.h"
#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/location.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "base/time/default_clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

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

WeeklyIntervalTimer::Factory::Factory()
    : Factory(base::DefaultClock::GetInstance(),
              base::DefaultTickClock::GetInstance()) {}

WeeklyIntervalTimer::Factory::Factory(const base::Clock* clock,
                                      const base::TickClock* tick_clock)
    : clock_(CHECK_DEREF(clock)), tick_clock_(CHECK_DEREF(tick_clock)) {}

WeeklyIntervalTimer::Factory::~Factory() = default;

std::unique_ptr<WeeklyIntervalTimer> WeeklyIntervalTimer::Factory::Create(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback) {
  // We can't use `make_unique` since the constructor is private.
  return base::WrapUnique(
      new WeeklyIntervalTimer(time_interval, on_interval_start_callback,
                              &clock_.get(), &tick_clock_.get()));
}

WeeklyIntervalTimer::WeeklyIntervalTimer(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : clock_(CHECK_DEREF(clock)),
      timer_until_start_of_interval_(
          std::make_unique<base::WallClockTimer>(clock, tick_clock)),
      time_interval_(time_interval),
      on_interval_start_callback_(on_interval_start_callback) {
  CHECK(tick_clock);
  CHECK(on_interval_start_callback_);
  auto& timezone_settings =
      CHECK_DEREF(system::TimezoneSettings::GetInstance());
  timezone_observer_.Observe(&timezone_settings);
  last_known_time_zone_id_ = timezone_settings.GetCurrentTimezoneID();

  // Start the timer but do it asynchronous to prevent invoking the callback
  // from inside the constructor/while the parent is still setting up.
  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE,
      base::BindOnce(&WeeklyIntervalTimer::CheckIntervalAndStartTimer,
                     weak_ptr_factory_.GetWeakPtr()));
}

WeeklyIntervalTimer::~WeeklyIntervalTimer() = default;

void WeeklyIntervalTimer::CheckIntervalAndStartTimer() {
  if (TimeFallsInInterval(clock_->Now(), time_interval_)) {
    InvokeOnStartCallback();
  }
  ScheduleTimerAtNextIntervalStart();
}

void WeeklyIntervalTimer::TimezoneChanged(const icu::TimeZone& timezone) {
  std::u16string updated_timezone_id =
      system::TimezoneSettings::GetTimezoneID(timezone);
  if (updated_timezone_id == last_known_time_zone_id_) {
    return;
  }

  last_known_time_zone_id_ = updated_timezone_id;

  CheckIntervalAndStartTimer();
}

void WeeklyIntervalTimer::InvokeOnStartCallback() {
  on_interval_start_callback_.Run(
      GetDuration(clock_->Now(), time_interval_.end()));
}

void WeeklyIntervalTimer::ScheduleTimerAtNextIntervalStart() {
  auto timer_duration = GetDuration(clock_->Now(), time_interval_.start());
  if (timer_duration.is_zero()) {
    // This will happen when the current time is exactly
    // `time_interval_.start()`, in which case we want to sleep for a full week.
    timer_duration = base::Days(7);
  }
  timer_until_start_of_interval_->Start(
      FROM_HERE, clock_->Now() + timer_duration,
      base::BindOnce(&WeeklyIntervalTimer::CheckIntervalAndStartTimer,
                     weak_ptr_factory_.GetWeakPtr()));
}

}  // namespace ash
