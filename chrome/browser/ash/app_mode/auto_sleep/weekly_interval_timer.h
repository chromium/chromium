// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_WEEKLY_INTERVAL_TIMER_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_WEEKLY_INTERVAL_TIMER_H_

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
class WeeklyIntervalTimer : public system::TimezoneSettings::Observer {
 public:
  class Factory {
   public:
    Factory();
    Factory(const base::Clock* clock, const base::TickClock* tick_clock);
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    ~Factory();

    std::unique_ptr<WeeklyIntervalTimer> Create(
        const policy::WeeklyTimeInterval& time_interval,
        base::RepeatingCallback<void(base::TimeDelta)>
            on_interval_start_callback);

   private:
    raw_ref<const base::Clock> clock_;
    raw_ref<const base::TickClock> tick_clock_;
  };

  WeeklyIntervalTimer(const WeeklyIntervalTimer&) = delete;
  WeeklyIntervalTimer& operator=(const WeeklyIntervalTimer&) = delete;

  ~WeeklyIntervalTimer() override;

  const policy::WeeklyTimeInterval& time_interval() const {
    return time_interval_;
  }

 private:
  WeeklyIntervalTimer(
      const policy::WeeklyTimeInterval& time_interval,
      base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
      const base::Clock* clock,
      const base::TickClock* tick_clock);

  // `system::TimezoneSettings::Observer`
  void TimezoneChanged(const icu::TimeZone& timezone) override;

  // Invokes callback if currently inside the interval.
  // Starts a timer to the next start of the interval to repeat this check.
  void CheckIntervalAndStartTimer();

  void InvokeOnStartCallback();
  void ScheduleTimerAtNextIntervalStart();

  // Clock to get the current system time.
  raw_ref<const base::Clock> clock_;

  std::unique_ptr<base::WallClockTimer> timer_until_start_of_interval_;

  const policy::WeeklyTimeInterval time_interval_;

  const base::RepeatingCallback<void(base::TimeDelta)>
      on_interval_start_callback_;

  // Last known timezone used to prevent reacting to multiple `TimezoneChanged`
  // observer calls of the same timezone.
  std::u16string last_known_time_zone_id_;

  base::ScopedObservation<system::TimezoneSettings,
                          system::TimezoneSettings::Observer>
      timezone_observer_{this};

  base::WeakPtrFactory<WeeklyIntervalTimer> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_WEEKLY_INTERVAL_TIMER_H_
