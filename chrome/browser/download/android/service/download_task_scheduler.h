// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_
#define CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_

#include <jni.h>
#include <stdint.h>

#include "components/download/public/task/task_scheduler.h"

namespace download {
namespace android {

// DownloadTaskScheduler is the utility class to schedule various types of
// background tasks with the OS as of when required by the download service.
class DownloadTaskScheduler : public TaskScheduler {
 public:
  DownloadTaskScheduler();

  DownloadTaskScheduler(const DownloadTaskScheduler&) = delete;
  DownloadTaskScheduler& operator=(const DownloadTaskScheduler&) = delete;

  ~DownloadTaskScheduler() override;

  // TaskScheduler implementation.
  void ScheduleTask(DownloadTaskType task_type,
                    bool require_unmetered_network,
                    bool require_charging,
                    int optimal_battery_percentage,
                    int64_t window_start_time_seconds,
                    int64_t window_end_time_seconds) override;
  void CancelTask(DownloadTaskType task_type) override;
};

}  // namespace android
}  // namespace download

#endif  // CHROME_BROWSER_DOWNLOAD_ANDROID_SERVICE_DOWNLOAD_TASK_SCHEDULER_H_
