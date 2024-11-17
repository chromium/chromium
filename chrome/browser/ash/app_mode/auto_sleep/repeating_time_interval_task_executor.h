// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/memory/raw_ref.h"
#include "base/memory/weak_ptr.h"
#include "base/scoped_observation.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "base/timer/wall_clock_timer.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace ash {

// When the device enters the specified weekly time interval, this
// class invokes the provided `on_interval_start_callback`. This class
// schedules the time interval using the system timezone. Changes to the system
// timezone will make it reprogram the time interval.
class RepeatingTimeIntervalTaskExecutor
    : public system::TimezoneSettings::Observer {
 public:
  class Factory {
   public:
    Factory();
    Factory(const base::Clock* clock, const base::TickClock* tick_clock);
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    ~Factory();

    std::unique_ptr<RepeatingTimeIntervalTaskExecutor> Create(
        const policy::WeeklyTimeInterval& time_interval,
        base::RepeatingCallback<void(base::TimeDelta)>
            on_interval_start_callback);

   private:
    raw_ref<const base::Clock> clock_;
    raw_ref<const base::TickClock> tick_clock_;
  };

  RepeatingTimeIntervalTaskExecutor(const RepeatingTimeIntervalTaskExecutor&) =
      delete;
  RepeatingTimeIntervalTaskExecutor& operator=(
      const RepeatingTimeIntervalTaskExecutor&) = delete;

  ~RepeatingTimeIntervalTaskExecutor() override;

  // Starts the executor and schedules the `timer_` to the start of the next
  // `interval_`. Additionally invokes `on_interval_start_callback_` if
  // currently inside the interval.
  void ScheduleTimer();

  // `system::TimezoneSettings::Observer`
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  const policy::WeeklyTimeInterval& time_interval() const {
    return time_interval_;
  }

 private:
  // TODO(crbug.com/328421429): Inline `ScheduleTimer()` method.
  RepeatingTimeIntervalTaskExecutor(
      const policy::WeeklyTimeInterval& time_interval,
      base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
      const base::Clock* clock,
      const base::TickClock* tick_clock);

  void InvokeOnStartCallback();
  void ScheduleTimerAtNextIntervalStart();

  // Clock to get the current system time.
  raw_ref<const base::Clock> clock_;

  std::unique_ptr<base::WallClockTimer> timer_until_start_of_interval_;

  const policy::WeeklyTimeInterval time_interval_;

  const base::RepeatingCallback<void(base::TimeDelta)>
      on_interval_start_callback_;

  bool timer_scheduled_ = false;

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
