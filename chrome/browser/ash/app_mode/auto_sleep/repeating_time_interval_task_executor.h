// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/dbus/power/native_timer.h"

namespace ash {

// When the device enters and exits the specified time interval, this class
// invokes the provided `on_interval_start_callback` callback and
// `on_interval_end_callback` callback respectively. This class schedules the
// time interval using the system timezone. Changes to the system timezone will
// make it reprogram the time interval. When the timer fails to start the
// callbacks will not be executed.
// TODO(b/319086751) Implement case when next interval is in the future.
// TODO(b/319083880) Observe time zone changes and cancel pending executors.
class RepeatingTimeIntervalTaskExecutor {
 public:
  RepeatingTimeIntervalTaskExecutor() = delete;

  RepeatingTimeIntervalTaskExecutor(
      const policy::WeeklyTimeInterval& time_interval,
      base::RepeatingClosure on_interval_start_callback,
      base::RepeatingClosure on_interval_end_callback,
      const std::string& tag);

  RepeatingTimeIntervalTaskExecutor(const RepeatingTimeIntervalTaskExecutor&) =
      delete;
  RepeatingTimeIntervalTaskExecutor& operator=(
      const RepeatingTimeIntervalTaskExecutor&) = delete;

  virtual ~RepeatingTimeIntervalTaskExecutor();

  const policy::WeeklyTimeInterval& time_interval() const {
    return time_interval_;
  }

  const std::string& timer_tag() const { return timer_tag_; }

  // Starts the executor and schedules the `timer_` to the start and end of the
  // `interval_` respectively. Runs `on_interval_start_callback_` at the start
  // of the interval and `on_interval_end_callback_` at the end.
  void Start();

 protected:
  // Clock to get the current system time.
  raw_ptr<const base::Clock> clock_;

 private:
  // Called by the `Start` function when the current time falls inside the
  // `time_interval_`.
  void IntervalStartsNow();

  // Timer until the end of the interval can fail to start. Handle the result to
  // inform about the failure, or proceed on the successful timer start.
  void HandleIntervalEndTimerStartResult(policy::ScopedWakeLock wakelock,
                                         bool result);

  // Timer until the end of the interval is finished.
  void HandleIntervalEndTimerFinish();

  virtual base::TimeTicks GetTimeTicksSinceBoot();

  const policy::WeeklyTimeInterval time_interval_;
  const base::RepeatingClosure on_interval_start_callback_;
  const base::RepeatingClosure on_interval_end_callback_;

  // Tag associated with the `NativeTimer`.
  const std::string timer_tag_;

  // `timer_` is used for two reasons:
  // 1) When we are waiting until the time interval starts to call
  // `on_interval_start_callback_`. TODO(b/319086751): Implement case when next
  // interval is in the future.
  //
  // 2) When we are waiting until the time interval ends to call
  // `on_interval_end_callback_`.
  std::unique_ptr<chromeos::NativeTimer> timer_;

  base::WeakPtrFactory<RepeatingTimeIntervalTaskExecutor> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
