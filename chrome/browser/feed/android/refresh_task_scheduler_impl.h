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

  void Run(FeedService* service, base::OnceClosure task_complete);

  // RefreshTaskScheduler.
  void EnsureScheduled(base::TimeDelta period) override;
  void Cancel() override;
  void RefreshTaskComplete() override;

 private:
  base::OnceClosure task_complete_callback_;
  raw_ptr<background_task::BackgroundTaskScheduler> scheduler_;
};

}  // namespace feed

#endif  // CHROME_BROWSER_FEED_ANDROID_REFRESH_TASK_SCHEDULER_IMPL_H_
