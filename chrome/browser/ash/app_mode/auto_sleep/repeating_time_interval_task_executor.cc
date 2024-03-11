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
    base::RepeatingClosure on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback,
    const std::string& tag) {
  return std::make_unique<RepeatingTimeIntervalTaskExecutor>(
      time_interval, on_interval_start_callback, on_interval_end_callback, tag);
}

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
  base::Time current_time = clock_->Now();

  if (TimeFallsInInterval(current_time, time_interval_)) {
    IntervalStartsNow();
  } else {
    IntervalStartsLater();
  }
}

void RepeatingTimeIntervalTaskExecutor::IntervalStartsNow() {
  TimerResultCallback timer_start_result_callback = base::BindOnce(
      &RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerStartResult,
      weak_ptr_factory_.GetWeakPtr());

  base::OnceClosure timer_end_callback = base::BindOnce(
      &RepeatingTimeIntervalTaskExecutor::HandleIntervalEndTimerFinish,
      weak_ptr_factory_.GetWeakPtr());
  StartTimer(time_interval_.end(), std::move(timer_start_result_callback),
             std::move(timer_end_callback));
}

void RepeatingTimeIntervalTaskExecutor::IntervalStartsLater() {
  TimerResultCallback timer_start_result_callback = base::BindOnce(
      &RepeatingTimeIntervalTaskExecutor::HandleIntervalStartTimerStartResult,
      weak_ptr_factory_.GetWeakPtr());
  // Call `Start` when the timer to the start of the interval finishes,
  // as that would retrigger the logic to the run the timer to the end of the
  // interval and call the callbacks respectively.
  base::OnceClosure timer_end_callback =
      base::BindOnce(&RepeatingTimeIntervalTaskExecutor::Start,
                     weak_ptr_factory_.GetWeakPtr());
  StartTimer(time_interval_.start(), std::move(timer_start_result_callback),
             std::move(timer_end_callback));
}

void RepeatingTimeIntervalTaskExecutor::StartTimer(
    policy::WeeklyTime expiration_time,
    TimerResultCallback timer_result_callback,
    base::OnceClosure timer_expiration_callback) {
  // Acquire a wake lock so that the device doesn't suspend during time tick
  // calculation, otherwise the time tick calculation will be incorrect.
  base::OnceCallback<void(bool)> timer_start_result_callback = base::BindOnce(
      [](policy::ScopedWakeLock wakelock, TimerResultCallback callback,
         bool result) -> void {
        std::move(callback).Run(std::move(wakelock), result);
      },
      policy::ScopedWakeLock(device::mojom::WakeLockType::kPreventAppSuspension,
                             kWakeLockReason),
      std::move(timer_result_callback));

  timer_->Start(
      GetTimeTicksSinceBoot() + GetDuration(clock_->Now(), expiration_time),
      std::move(timer_expiration_callback),
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
  Start();
}

void RepeatingTimeIntervalTaskExecutor::HandleIntervalStartTimerStartResult(
    policy::ScopedWakeLock wakelock,
    bool result) {
  // TODO(b/324878921) Consider retrying or scheduling the timer for the next
  // week when `NativeTimer` fails to start.
  if (!result) {
    LOG(ERROR) << "Failed to start RepeatingTimeIntervalTaskExecutor timer to "
                  "the start of the interval";
    return;
  }
}

base::TimeTicks RepeatingTimeIntervalTaskExecutor::GetTimeTicksSinceBoot() {
  struct timespec spec;
  int result = clock_gettime(CLOCK_BOOTTIME, &spec);
  CHECK_EQ(result, 0);

  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(spec);
}

}  // namespace ash
