// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/policy/scheduled_task_handler/scheduled_task_executor.h"

#include "base/check.h"

namespace policy {

ScheduledTaskExecutor::ScheduledTaskData::ScheduledTaskData() = default;
ScheduledTaskExecutor::ScheduledTaskData::ScheduledTaskData(
    const ScheduledTaskData&) = default;
ScheduledTaskExecutor::ScheduledTaskData::~ScheduledTaskData() = default;

ScheduledTaskExecutor::ScheduledTaskExecutor(const char* timer_tag)
    : timer_tag_(timer_tag) {}

ScheduledTaskExecutor::~ScheduledTaskExecutor() = default;

void ScheduledTaskExecutor::Start(
    ScheduledTaskData* scheduled_task_data,
    chromeos::OnStartNativeTimerCallback result_cb,
    TimerCallback timer_expired_cb) {
  // Only one |ScheduledTaskTimer| can be outstanding
  scheduled_task_timer_.reset();

  DCHECK(scheduled_task_data);

  // TODO(https://crbug.com/1224891): Add logic for calculating
  // next_scheduled_task_time_ticks.

  scheduled_task_timer_ = std::make_unique<chromeos::NativeTimer>(timer_tag_);
  scheduled_task_timer_->Start(
      scheduled_task_data->next_scheduled_task_time_ticks,
      std::move(timer_expired_cb), std::move(result_cb));
}

void ScheduledTaskExecutor::Reset() {
  scheduled_task_timer_.reset();
}

}  // namespace policy
