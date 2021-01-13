// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/feed/v2/refresh_task_scheduler_impl.h"

#include <utility>

#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/background_task_scheduler/task_info.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_service.h"
#include "components/feed/core/v2/public/feed_stream_api.h"

namespace feed {

RefreshTaskSchedulerImpl::RefreshTaskSchedulerImpl(
    background_task::BackgroundTaskScheduler* scheduler)
    : scheduler_(scheduler) {
  DCHECK(scheduler_);
}
RefreshTaskSchedulerImpl::~RefreshTaskSchedulerImpl() = default;

void RefreshTaskSchedulerImpl::Run(FeedService* service,
                                   base::OnceClosure task_complete) {
  task_complete_callback_ = std::move(task_complete);
  service->GetStream()->ExecuteRefreshTask();
}

void RefreshTaskSchedulerImpl::EnsureScheduled(base::TimeDelta delay) {
  background_task::OneOffInfo one_off;
  one_off.window_start_time_ms = delay.InMilliseconds();
  one_off.window_end_time_ms =
      delay.InMilliseconds() +
      GetFeedConfig().background_refresh_window_length.InMilliseconds();
  one_off.expires_after_window_end_time = true;
  background_task::TaskInfo task_info(
      static_cast<int>(background_task::TaskIds::FEEDV2_REFRESH_JOB_ID),
      one_off);
  task_info.update_current = true;
  task_info.network_type = background_task::TaskInfo::ANY;
  scheduler_->Schedule(task_info);
}

void RefreshTaskSchedulerImpl::Cancel() {
  scheduler_->Cancel(
      static_cast<int>(background_task::TaskIds::FEEDV2_REFRESH_JOB_ID));
}

void RefreshTaskSchedulerImpl::RefreshTaskComplete() {
  if (task_complete_callback_) {
    std::move(task_complete_callback_).Run();
  }
}

}  // namespace feed
