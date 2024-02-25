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

namespace ash {

namespace {

constexpr char kWakeLockReason[] = "RepeatingTimeIntervalTaskExecutor";

base::TimeDelta GetDuration(const base::Time& start,
                            const policy::WeeklyTime& end) {
  policy::WeeklyTime start_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(start);
  return start_weekly_time.GetDurationTo(end);
}

// Returns true when the provided `time` is contained within the `interval`.
// TODO(b/319086751): Add check to early return if duration between end interval
// and current time is less than a fixed amount of minutes.
bool TimeFallsInInterval(const base::Time& time,
                         const policy::WeeklyTimeInterval& interval) {
  policy::WeeklyTime current_weekly_time =
      policy::WeeklyTime::GetLocalWeeklyTime(time);

  return interval.Contains(current_weekly_time);
}

}  // namespace

RepeatingTimeIntervalTaskExecutor::RepeatingTimeIntervalTaskExecutor(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingClosure on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback,
    const std::string& tag)
    : clock_(base::DefaultClock::GetInstance()),
      time_interval_(time_interval),
      on_interval_start_callback_(on_interval_start_callback),
      on_interval_end_callback_(on_interval_end_callback),
      timer_tag_(tag),
      timer_(std::make_unique<chromeos::NativeTimer>(tag)) {
  CHECK(on_interval_start_callback_);
  CHECK(on_interval_end_callback_);
}

RepeatingTimeIntervalTaskExecutor::~RepeatingTimeIntervalTaskExecutor() =
    default;

void RepeatingTimeIntervalTaskExecutor::Start() {
  if (TimeFallsInInterval(clock_->Now(), time_interval_)) {
    IntervalStartsNow();
  } else {
    // TODO(b/319086751) Schedule a timer to the start of interval.
  }
}

void RepeatingTimeIntervalTaskExecutor::IntervalStartsNow() {
  // Acquire a wake lock so that the device doesn't suspend during time tick
  // calculation, otherwise the time tick calculation will be incorrect.
  chromeos::OnStartNativeTimerCallback timer_start_result_callback =
      base::BindOnce(
          &RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerStartResult,
          weak_ptr_factory_.GetWeakPtr(),
          policy::ScopedWakeLock(
              device::mojom::WakeLockType::kPreventAppSuspension,
              kWakeLockReason));

  base::OnceClosure timer_end_callback = base::BindOnce(
      &RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerFinish,
      weak_ptr_factory_.GetWeakPtr());
  timer_->Start(GetTimeTicksSinceBoot() +
                    GetDuration(clock_->Now(), time_interval_.end()),
                std::move(timer_end_callback),
                std::move(timer_start_result_callback));
}

void RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerStartResult(
    policy::ScopedWakeLock wakelock,
    bool result) {
  // TODO(b/324878921) Consider retrying or scheduling the timer for the next
  // week when `NativeTimer` fails to start.
  if (!result) {
    LOG(ERROR) << "Failed to start RepeatingTimeIntervalTaskExecutor timer";
    return;
  }
  on_interval_start_callback_.Run();
}

void RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerFinish() {
  on_interval_end_callback_.Run();
  // TODO(b/319086751) Call `Start()` here when next interval in the future case
  // is implemented.
}

base::TimeTicks RepeatingTimeIntervalTaskExecutor::GetTimeTicksSinceBoot() {
  struct timespec spec;
  int result = clock_gettime(CLOCK_BOOTTIME, &spec);
  CHECK_EQ(result, 0);

  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(spec);
}

}  // namespace ash
