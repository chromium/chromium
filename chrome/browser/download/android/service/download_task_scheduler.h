// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_

#include <jni.h>
#include <stdint.h>
#include <memory>

#include "base/macros.h"
#include "components/download/public/task/task_scheduler.h"

namespace download {
namespace android {

// DownloadTaskScheduler is the utility class to schedule various types of
// background tasks with the OS as of when required by the download service.
class DownloadTaskScheduler : public TaskScheduler {
 public:
  DownloadTaskScheduler();
  ~DownloadTaskScheduler() override;

  // TaskScheduler implementation.
  void ScheduleTask(DownloadTaskType task_type,
                    bool require_unmetered_network,
                    bool require_charging,
                    int optimal_battery_percentage,
                    int64_t window_start_time_seconds,
                    int64_t window_end_time_seconds) override;
  void CancelTask(DownloadTaskType task_type) override;

 private:
  DISALLOW_COPY_AND_ASSIGN(DownloadTaskScheduler);
};

}  // namespace android
}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_
