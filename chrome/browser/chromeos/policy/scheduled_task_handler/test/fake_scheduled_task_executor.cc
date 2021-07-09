// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/memory/ptr_util.h"

namespace policy {
namespace {
// Time zones that will be used in tests.
constexpr char kESTTimeZoneID[] = "America/New_York";

// The tag associated to register scheduled task timer.
constexpr char kScheduledTaskTimerTagForTest[] =
    "DeviceScheduledTaskTimerForTest";
}  // namespace

FakeScheduledTaskExecutor::FakeScheduledTaskExecutor(
    const base::Clock* clock,
    const base::TickClock* tick_clock)
    : ScheduledTaskExecutorImpl(kScheduledTaskTimerTagForTest),
      clock_(clock),
      tick_clock_(tick_clock) {
  // Set time zone so that tests are deterministic across different
  // environments.
  time_zone_ = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(kESTTimeZoneID)));
}

FakeScheduledTaskExecutor::~FakeScheduledTaskExecutor() {}

void FakeScheduledTaskExecutor::SetTimeZone(
    std::unique_ptr<icu::TimeZone> time_zone) {
  time_zone_ = std::move(time_zone);
}

base::Time FakeScheduledTaskExecutor::GetCurrentTime() {
  return clock_->Now();
}

const icu::TimeZone& FakeScheduledTaskExecutor::GetTimeZone() {
  return *time_zone_;
}

void FakeScheduledTaskExecutor::SimulateCalculateNextScheduledTaskFailure(
    bool simulate) {
  simulate_calculate_next_update_check_failure_ = simulate;
}

base::TimeDelta FakeScheduledTaskExecutor::CalculateNextScheduledTaskTimerDelay(
    base::Time cur_time,
    ScheduledTaskData* scheduled_task_data) {
  if (simulate_calculate_next_update_check_failure_)
    return scheduled_task_internal::kInvalidDelay;
  return ScheduledTaskExecutorImpl::CalculateNextScheduledTaskTimerDelay(
      cur_time, scheduled_task_data);
}

base::TimeTicks FakeScheduledTaskExecutor::GetTicksSinceBoot() {
  return tick_clock_->NowTicks();
}
}  // namespace policy
