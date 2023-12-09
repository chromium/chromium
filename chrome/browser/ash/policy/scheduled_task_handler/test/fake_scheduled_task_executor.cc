// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/test/fake_scheduled_task_executor.h"

#include <memory>
#include <string>
#include <utility>

#include "base/functional/bind.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "base/task/sequenced_task_runner.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"

namespace policy {
namespace {
// Time zones that will be used in tests.
constexpr char kESTTimeZoneID[] = "America/New_York";
}  // namespace

FakeScheduledTaskExecutor::FakeScheduledTaskExecutor(const base::Clock* clock)
    : clock_(clock) {
  // Set time zone so that tests are deterministic across different
  // environments.
  time_zone_ = base::WrapUnique(icu::TimeZone::createTimeZone(
      icu::UnicodeString::fromUTF8(kESTTimeZoneID)));
}

FakeScheduledTaskExecutor::~FakeScheduledTaskExecutor() = default;

void FakeScheduledTaskExecutor::Start(
    ScheduledTaskData* scheduled_task_data,
    chromeos::OnStartNativeTimerCallback result_cb,
    TimerCallback timer_expired_cb,
    base::TimeDelta external_delay) {
  Reset();

  // Calculate the next scheduled task time. In case there is an error while
  // calculating, due to concurrent DST or Time Zone changes, then reschedule
  // this function and try to schedule the task again. There should
  // only be one outstanding task to start the timer. If there is a failure
  // the wake lock is released and acquired again when this task runs.
  base::Time cur_time = GetCurrentTime();
  std::optional<base::TimeDelta> delay_to_next_schedule =
      scheduled_task_util::CalculateNextScheduledTaskTimerDelay(
          *scheduled_task_data, cur_time, GetTimeZone());
  if (simulate_calculate_next_update_check_failure_ ||
      !delay_to_next_schedule.has_value()) {
    LOG(ERROR) << "Failed to calculate next scheduled task time";
    std::move(result_cb).Run(false);
    return;
  }
  scheduled_task_time_ =
      cur_time + delay_to_next_schedule.value() + external_delay;

  base::SequencedTaskRunner::GetCurrentDefault()->PostTask(
      FROM_HERE, base::BindOnce(std::move(result_cb), true));
  timer_.Start(FROM_HERE, delay_to_next_schedule.value() + external_delay,
               std::move(timer_expired_cb));
}

void FakeScheduledTaskExecutor::Reset() {
  timer_.Stop();
}

const base::Time FakeScheduledTaskExecutor::GetScheduledTaskTime() const {
  return scheduled_task_time_;
}

void FakeScheduledTaskExecutor::SetTimeZone(
    std::unique_ptr<icu::TimeZone> time_zone) {
  time_zone_ = std::move(time_zone);
}

base::Time FakeScheduledTaskExecutor::GetCurrentTime() const {
  return clock_->Now();
}

const icu::TimeZone& FakeScheduledTaskExecutor::GetTimeZone() const {
  return *time_zone_;
}

void FakeScheduledTaskExecutor::SimulateCalculateNextScheduledTaskFailure(
    bool simulate) {
  simulate_calculate_next_update_check_failure_ = simulate;
}

}  // namespace policy
