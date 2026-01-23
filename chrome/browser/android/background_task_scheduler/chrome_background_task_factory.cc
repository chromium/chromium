// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"

#include <memory>
#include <utility>

#include "chrome/android/chrome_jni_headers/ChromeBackgroundTaskFactory_jni.h"
#include "chrome/browser/feed/android/background_refresh_task.h"
#include "chrome/browser/metrics/android/background_upload_task.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/feed/core/v2/public/types.h"

// static
void ChromeBackgroundTaskFactory::SetAsDefault() {
  JNIEnv* env = jni_zero::AttachCurrentThread();
  Java_ChromeBackgroundTaskFactory_setAsDefault(env);
}

std::unique_ptr<background_task::BackgroundTask>
ChromeBackgroundTaskFactory::GetNativeBackgroundTaskFromTaskId(int task_id) {
  // Add your tasks here with mappings to the given task_id.
  switch (task_id) {
    case static_cast<int>(background_task::TaskIds::FEEDV2_REFRESH_JOB_ID):
      return std::make_unique<feed::BackgroundRefreshTask>(
          feed::RefreshTaskId::kRefreshForYouFeed);
    case static_cast<int>(background_task::TaskIds::WEBFEEDS_REFRESH_JOB_ID):
      return std::make_unique<feed::BackgroundRefreshTask>(
          feed::RefreshTaskId::kRefreshWebFeed);
    case static_cast<int>(background_task::TaskIds::UMA_UPLOAD_JOB_ID):
    case static_cast<int>(background_task::TaskIds::UKM_UPLOAD_JOB_ID):
    case static_cast<int>(background_task::TaskIds::DWA_UPLOAD_JOB_ID):
    case static_cast<int>(background_task::TaskIds::PUMA_UPLOAD_JOB_ID):
    case static_cast<int>(
        background_task::TaskIds::STRUCTURED_METRICS_UPLOAD_JOB_ID):
      return std::make_unique<metrics::BackgroundUploadTask>(
          static_cast<background_task::TaskIds>(task_id));
    default:
      break;
  }
  return nullptr;
}

DEFINE_JNI(ChromeBackgroundTaskFactory)
