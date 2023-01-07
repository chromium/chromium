// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_FEED_ANDROID_REFRESH_TASK_SCHEDULER_IMPL_H_
#define CHROME_BROWSER_FEED_ANDROID_REFRESH_TASK_SCHEDULER_IMPL_H_

#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"

#include "components/feed/core/v2/public/refresh_task_scheduler.h"

namespace background_task {
class BackgroundTaskScheduler;
}  // namespace background_task

namespace feed {
class FeedService;

class RefreshTaskSchedulerImpl : public RefreshTaskScheduler {
 public:
  explicit RefreshTaskSchedulerImpl(
      background_task::BackgroundTaskScheduler* scheduler);
  ~RefreshTaskSchedulerImpl() override;
  RefreshTaskSchedulerImpl(const RefreshTaskSchedulerImpl&) = delete;
  RefreshTaskSchedulerImpl& operator=(const RefreshTaskSchedulerImpl&) = delete;

  void Run(RefreshTaskId task_id,
           FeedService* service,
           base::OnceClosure task_complete);

  // RefreshTaskScheduler.
  void EnsureScheduled(RefreshTaskId task_id, base::TimeDelta period) override;
  void Cancel(RefreshTaskId task_id) override;
  void RefreshTaskComplete(RefreshTaskId task_id) override;

 private:
  base::OnceClosure& TaskCallback(RefreshTaskId task_id);

  raw_ptr<background_task::BackgroundTaskScheduler> scheduler_;
  base::OnceClosure for_you_task_complete_callback_;
  base::OnceClosure web_feeds_task_complete_callback_;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_REFRESH_TASK_SCHEDULER_IMPL_H_
