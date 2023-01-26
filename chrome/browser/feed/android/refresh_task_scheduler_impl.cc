// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/feed/android/refresh_task_scheduler_impl.h"

#include <utility>

#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/background_task_scheduler/task_info.h"
#include "components/feed/core/v2/config.h"
#include "components/feed/core/v2/public/feed_api.h"
#include "components/feed/core/v2/public/feed_service.h"

namespace feed {

namespace {
background_task::TaskIds ToBackgroundTaskId(RefreshTaskId id) {
  switch (id) {
    case RefreshTaskId::kRefreshForYouFeed:
      return background_task::TaskIds::FEEDV2_REFRESH_JOB_ID;
    case RefreshTaskId::kRefreshWebFeed:
      return background_task::TaskIds::WEBFEEDS_REFRESH_JOB_ID;
  }
}
}  // namespace

RefreshTaskSchedulerImpl::RefreshTaskSchedulerImpl(
    background_task::BackgroundTaskScheduler* scheduler)
    : scheduler_(scheduler) {
  DCHECK(scheduler_);
}
RefreshTaskSchedulerImpl::~RefreshTaskSchedulerImpl() = default;

void RefreshTaskSchedulerImpl::Run(RefreshTaskId task_id,
                                   FeedService* service,
                                   base::OnceClosure task_complete) {
  TaskCallback(task_id) = std::move(task_complete);
  service->GetStream()->ExecuteRefreshTask(task_id);
}

void RefreshTaskSchedulerImpl::EnsureScheduled(RefreshTaskId task_id,
                                               base::TimeDelta delay) {
  background_task::OneOffInfo one_off;
  one_off.window_start_time_ms = delay.InMilliseconds();
  one_off.window_end_time_ms =
      delay.InMilliseconds() +
      GetFeedConfig().background_refresh_window_length.InMilliseconds();
  one_off.expires_after_window_end_time = true;
  background_task::TaskInfo task_info(
      static_cast<int>(ToBackgroundTaskId(task_id)), one_off);
  task_info.is_persisted = true;
  task_info.update_current = true;
  task_info.network_type = background_task::TaskInfo::ANY;
  scheduler_->Schedule(task_info);
}

void RefreshTaskSchedulerImpl::Cancel(RefreshTaskId task_id) {
  scheduler_->Cancel(static_cast<int>(ToBackgroundTaskId(task_id)));
}

void RefreshTaskSchedulerImpl::RefreshTaskComplete(RefreshTaskId task_id) {
  base::OnceClosure& callback = TaskCallback(task_id);
  if (callback) {
    std::move(callback).Run();
  }
}

base::OnceClosure& RefreshTaskSchedulerImpl::TaskCallback(
    RefreshTaskId task_id) {
  switch (task_id) {
    case RefreshTaskId::kRefreshForYouFeed:
      return for_you_task_complete_callback_;
    case RefreshTaskId::kRefreshWebFeed:
      return web_feeds_task_complete_callback_;
  }
}

}  // namespace feed
