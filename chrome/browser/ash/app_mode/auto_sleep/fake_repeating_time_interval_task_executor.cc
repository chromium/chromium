// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/fake_repeating_time_interval_task_executor.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/tick_clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

namespace ash {

FakeRepeatingTimeIntervalTaskExecutor::Factory::Factory(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : clock_(clock), tick_clock_(tick_clock) {
  CHECK(clock);
  CHECK(tick_clock);
}

FakeRepeatingTimeIntervalTaskExecutor::Factory::~Factory() = default;

std::unique_ptr<RepeatingTimeIntervalTaskExecutor>
FakeRepeatingTimeIntervalTaskExecutor::Factory::Create(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback) {
  return std::make_unique<FakeRepeatingTimeIntervalTaskExecutor>(
      time_interval, on_interval_start_callback, on_interval_end_callback,
      clock_, tick_clock_);
}

FakeRepeatingTimeIntervalTaskExecutor::FakeRepeatingTimeIntervalTaskExecutor(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingCallback<void(base::TimeDelta)> on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback,
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : RepeatingTimeIntervalTaskExecutor(time_interval,
                                        on_interval_start_callback,
                                        on_interval_end_callback) {
  clock_ = clock;
  timer_ = std::make_unique<base::WallClockTimer>(clock, tick_clock);
}

FakeRepeatingTimeIntervalTaskExecutor::
    ~FakeRepeatingTimeIntervalTaskExecutor() = default;

}  // namespace ash
