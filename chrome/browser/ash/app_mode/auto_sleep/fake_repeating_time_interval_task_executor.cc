// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/auto_sleep/fake_repeating_time_interval_task_executor.h"

#include <memory>
#include <string>

#include "base/functional/callback.h"
#include "base/time/clock.h"
#include "base/time/time.h"
#include "chromeos/ash/components/policy/weekly_time/weekly_time_interval.h"

namespace ash {

FakeRepeatingTimeIntervalTaskExecutor::Factory::Factory(
    const base::Clock* clock)
    : clock_(clock) {
  CHECK(clock);
}

FakeRepeatingTimeIntervalTaskExecutor::Factory::~Factory() = default;

std::unique_ptr<RepeatingTimeIntervalTaskExecutor>
FakeRepeatingTimeIntervalTaskExecutor::Factory::Create(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingClosure on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback,
    const std::string& tag) {
  return std::make_unique<FakeRepeatingTimeIntervalTaskExecutor>(
      time_interval, on_interval_start_callback, on_interval_end_callback, tag,
      clock_);
}

FakeRepeatingTimeIntervalTaskExecutor::FakeRepeatingTimeIntervalTaskExecutor(
    const policy::WeeklyTimeInterval& time_interval,
    base::RepeatingClosure on_interval_start_callback,
    base::RepeatingClosure on_interval_end_callback,
    const std::string& tag,
    const base::Clock* clock)
    : RepeatingTimeIntervalTaskExecutor(time_interval,
                                        on_interval_start_callback,
                                        on_interval_end_callback,
                                        tag) {
  clock_ = clock;
}

FakeRepeatingTimeIntervalTaskExecutor::
    ~FakeRepeatingTimeIntervalTaskExecutor() = default;

base::TimeTicks FakeRepeatingTimeIntervalTaskExecutor::GetTimeTicksSinceBoot() {
  // Only use `base::TimeTicks::Now` for testing as it is overridden by mock
  // time and can be safely used in tests, the reason we do not use this in
  // non-test code is due to b/40296804, `base::TimeTicks::Now` does not handle
  // suspend properly.
  return base::TimeTicks::Now();
}

}  // namespace ash
