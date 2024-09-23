// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_FAKE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
#define CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_FAKE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_

#include "chrome/browser/ash/app_mode/auto_sleep/repeating_time_interval_task_executor.h"

#include <memory>

#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/default_tick_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

namespace ash {

class FakeRepeatingTimeIntervalTaskExecutor
    : public RepeatingTimeIntervalTaskExecutor {
 public:
  class Factory : public RepeatingTimeIntervalTaskExecutor::Factory {
   public:
    Factory(const base::Clock* clock,
            const base::TickClock* tick_clock =
                base::DefaultTickClock::GetInstance());
    Factory() = delete;
    Factory(const Factory&) = delete;
    const Factory& operator=(const Factory&) = delete;
    ~Factory() override;

    std::unique_ptr<RepeatingTimeIntervalTaskExecutor> Create(
        const policy::WeeklyTimeInterval& time_interval,
        base::RepeatingCallback<void(base::TimeDelta)>
            on_interval_start_callback,
        base::RepeatingClosure on_interval_end_callback) override;

   private:
    raw_ptr<const base::Clock> clock_ = nullptr;
    raw_ptr<const base::TickClock> tick_clock_ = nullptr;
  };

  FakeRepeatingTimeIntervalTaskExecutor() = delete;

  FakeRepeatingTimeIntervalTaskExecutor(
      const policy::WeeklyTimeInterval& time_interval,
      base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
      base::RepeatingClosure on_interval_end_callback,
      const base::Clock* clock,
      const base::TickClock* tick_clock);

  FakeRepeatingTimeIntervalTaskExecutor(
      const FakeRepeatingTimeIntervalTaskExecutor&) = delete;
  FakeRepeatingTimeIntervalTaskExecutor& operator=(
      const FakeRepeatingTimeIntervalTaskExecutor&) = delete;

  ~FakeRepeatingTimeIntervalTaskExecutor() override;
};

}  // namespace ash

#endif  // CHROME_BROWSER_ASH_APP_MODE_AUTO_SLEEP_FAKE_REPEATING_TIME_INTERVAL_TASK_EXECUTOR_H_
