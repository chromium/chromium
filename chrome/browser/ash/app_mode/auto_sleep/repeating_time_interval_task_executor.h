// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_

#include <memory>

#include "base/functional/callback.h"
#include "base/functional/callback_forward.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "base/timer/wall_clock_timer.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scoped_wake_lock.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "chromeos/dbus/power/native_timer.h"

namespace ash {

// When the device enters and exits the specified weekly time interval, this
// class invokes the provided `on_interval_start_callback` callback and
// `on_interval_end_callback` callback respectively every week. This class
// schedules the time interval using the system timezone. Changes to the system
// timezone will make it reprogram the time interval. When the timer fails to
// start the callbacks will not be executed.
class RepeatingTimeIntervalTaskExecutor
    : public system::TimezoneSettings::Observer {
 public:
  using TimerResultCallback =
      base::OnceCallback<void(policy::ScopedWakeLock, bool)>;
  class Factory {
   public:
    Factory();
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    virtual ~Factory();

    virtual std::unique_ptr<RepeatingTimeIntervalTaskExecutor> Create(
        const policy::WeeklyTimeInterval& time_interval,
        base::RepeatingCallback<void(base::TimeDelta)>
            on_interval_start_callback,
        base::RepeatingClosure on_interval_end_callback);
  };

  RepeatingTimeIntervalTaskExecutor() = delete;

  // TODO(b/328421429): Make constructor private and inline `ScheduleTimer()`
  // method.
  RepeatingTimeIntervalTaskExecutor(
      const policy::WeeklyTimeInterval& time_interval,
      base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
      base::RepeatingClosure on_interval_end_callback);

  RepeatingTimeIntervalTaskExecutor(const RepeatingTimeIntervalTaskExecutor&) =
      delete;
  RepeatingTimeIntervalTaskExecutor& operator=(
      const RepeatingTimeIntervalTaskExecutor&) = delete;

  ~RepeatingTimeIntervalTaskExecutor() override;

  // Starts the executor and schedules the `timer_` to the start and end of the
  // `interval_` respectively. Runs `on_interval_start_callback_` at the start
  // of the interval and `on_interval_end_callback_` at the end.
  void ScheduleTimer();

  // system::TimezoneSettings::Observer
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  const policy::WeeklyTimeInterval& time_interval() const {
    return time_interval_;
  }

 protected:
  // Clock to get the current system time.
  raw_ptr<const base::Clock> clock_;

  // `timer_` is used for two reasons:
  // 1) When we are waiting until the time interval starts to call
  // `on_interval_start_callback_`.
  //
  // 2) When we are waiting until the time interval ends to call
  // `on_interval_end_callback_`.
  std::unique_ptr<base::WallClockTimer> timer_;

 private:
  // Called by the `Start` function when the current time falls inside the
  // `time_interval_`.
  void IntervalStartsNow();

  // Called by the `Start` function when the start of the interval is in the
  // future.
  void IntervalStartsLater();

  // Starts a timer to expire at `expiration_time`. Calls the
  // `timer_expiration_callback` on timer expiration.
  void StartTimer(policy::WeeklyTime expiration_time,
                  base::OnceClosure timer_expiration_callback);

  // Timer until the end of the interval is finished.
  // TODO(b/330836068): Remove interval end timer.
  void HandleIntervalEndTimerFinish();

  const policy::WeeklyTimeInterval time_interval_;

  const base::RepeatingCallback<void(base::TimeDelta)>
      on_interval_start_callback_;
  // TODO(b/330836068): Remove interval end callback.
  const base::RepeatingClosure on_interval_end_callback_;

  bool timer_scheduled_ = false;

  // Flag to track if a timer to the end of the interval has started. Used to
  // run the `on_interval_start_callback_` when the timezone changes.
  bool has_interval_end_timer_started_ = false;

  // Last known timezone used to prevent reacting to multiple `TimezoneChanged`
  // observer calls of the same timezone.
  std::u16string last_known_time_zone_id_;

  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      timezone_observer_{this};

  base::WeakPtrFactory<RepeatingTimeIntervalTaskExecutor> weak_ptr_factory_{
      this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
