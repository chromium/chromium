// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/download/android/service/download_task_scheduler.h"

#include "base/android/jni_android.h"
#include "base/trace_event/trace_event.h"
#include "chrome/android/chrome_jni_headers/DownloadTaskScheduler_jni.h"

namespace download {
namespace android {

DownloadTaskScheduler::DownloadTaskScheduler() = default;

DownloadTaskScheduler::~DownloadTaskScheduler() = default;

void DownloadTaskScheduler::ScheduleTask(DownloadTaskType task_type,
                                         bool require_unmetered_network,
                                         bool require_charging,
                                         int optimal_battery_percentage,
                                         int64_t window_start_time_seconds,
                                         int64_t window_end_time_seconds) {
  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadTaskScheduler_scheduleTask(
      env, static_cast<jint>(task_type), require_unmetered_network,
      require_charging, optimal_battery_percentage, window_start_time_seconds,
      window_end_time_seconds);
}

void DownloadTaskScheduler::CancelTask(DownloadTaskType task_type) {
  TRACE_EVENT0("download_service", "DownloadTaskScheduler.CancelTask");

  JNIEnv* env = base::android::AttachCurrentThread();
  Java_DownloadTaskScheduler_cancelTask(env, static_cast<jint>(task_type));
}

}  // namespace android
}  // namespace download
