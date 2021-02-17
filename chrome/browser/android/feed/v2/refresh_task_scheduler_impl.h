// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ANDROID_FEED_V2_REFRESH_TASK_SCHEDULER_IMPL_H_
#define CHROME_BROWSER_ANDROID_FEED_V2_REFRESH_TASK_SCHEDULER_IMPL_H_

#include "base/callback.h"

#include "components/feed/core/v2/refresh_task_scheduler.h"

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

  void Run(FeedService* service, base::OnceClosure task_complete);

  // RefreshTaskScheduler.
  void EnsureScheduled(base::TimeDelta period) override;
  void Cancel() override;
  void RefreshTaskComplete() override;

 private:
  background_task::BackgroundTaskScheduler* scheduler_;
  base::OnceClosure task_complete_callback_;
};

}  // namespace feed

#endif  // CHROME_BROWSER_ANDROID_FEED_V2_REFRESH_TASK_SCHEDULER_IMPL_H_
