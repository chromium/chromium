// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_executor_impl.h"
#include <cstdint>

#include "base/check.h"
#include "base/logging.h"
#include "base/memory/ptr_util.h"
#include "chrome/browser/ash/policy/scheduled_task_handler/scheduled_task_util.h"
#include "chromeos/ash/components/settings/timezone_settings.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace policy {

ScheduledTaskExecutorImpl::ScheduledTaskExecutorImpl(const char* timer_tag)
    : timer_tag_(timer_tag) {}

ScheduledTaskExecutorImpl::~ScheduledTaskExecutorImpl() = default;

void ScheduledTaskExecutorImpl::Start(
    ScheduledTaskData* scheduled_task_data,
    chromeos::OnStartNativeTimerCallback result_cb,
    TimerCallback timer_expired_cb,
    base::TimeDelta external_delay) {
  Reset();

  DCHECK(scheduled_task_data);

  // For accuracy of the next scheduled task, capture current time as close to
  // the start of this function as possible.
  const base::TimeTicks cur_ticks = GetTicksSinceBoot();
  const base::Time cur_time = GetCurrentTime();

  // Calculate the next scheduled task time. In case there is an error while
  // calculating, due to concurrent DST or Time Zone changes, then reschedule
  // this function and try to schedule the task again. There should
  // only be one outstanding task to start the timer. If there is a failure
  // the wake lock is released and acquired again when this task runs.
  std::optional<base::TimeDelta> delay_to_next_schedule =
      scheduled_task_util::CalculateNextScheduledTaskTimerDelay(
          *scheduled_task_data, cur_time, GetTimeZone());
  if (!delay_to_next_schedule.has_value()) {
    LOG(ERROR) << "Failed to calculate next scheduled task time";
    std::move(result_cb).Run(false);
    return;
  }

  base::TimeDelta total_delay = delay_to_next_schedule.value() + external_delay;
  scheduled_task_time_ = cur_time + total_delay;

  scheduled_task_timer_ = std::make_unique<chromeos::NativeTimer>(timer_tag_);
  scheduled_task_timer_->Start(cur_ticks + total_delay,
                               std::move(timer_expired_cb),
                               std::move(result_cb));
}

void ScheduledTaskExecutorImpl::Reset() {
  scheduled_task_timer_.reset();
  scheduled_task_time_ = base::Time();
}

const base::Time ScheduledTaskExecutorImpl::GetScheduledTaskTime() const {
  return scheduled_task_time_;
}

base::Time ScheduledTaskExecutorImpl::GetCurrentTime() {
  return base::Time::Now();
}

base::TimeTicks ScheduledTaskExecutorImpl::GetTicksSinceBoot() {
  struct timespec ts = {};
  int ret = clock_gettime(CLOCK_BOOTTIME, &ts);
  DCHECK_EQ(ret, 0);
  return base::TimeTicks() + base::TimeDelta::FromTimeSpec(ts);
}

const icu::TimeZone& ScheduledTaskExecutorImpl::GetTimeZone() {
  return ash::system::TimezoneSettings::GetInstance()->GetTimezone();
}

}  // namespace policy
