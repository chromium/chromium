// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_
#define CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_

#include "components/background_task_scheduler/background_task.h"
#include "components/background_task_scheduler/task_ids.h"

namespace metrics {

class BackgroundUploadTask : public background_task::BackgroundTask {
 public:
  explicit BackgroundUploadTask(background_task::TaskIds task_id);
  ~BackgroundUploadTask() override;

  BackgroundUploadTask(const BackgroundUploadTask& other) = delete;
  BackgroundUploadTask& operator=(const BackgroundUploadTask& other) = delete;

 private:
  // background_task::BackgroundTask:
  void OnStartTaskInReducedMode(
      const background_task::TaskParameters& task_params,
      background_task::TaskFinishedCallback callback,
      SimpleFactoryKey* key) override;
  void OnStartTaskWithFullBrowser(
      const background_task::TaskParameters& task_params,
      background_task::TaskFinishedCallback callback,
      content::BrowserContext* browser_context) override;
  void OnFullBrowserLoaded(content::BrowserContext* browser_context) override;
  bool OnStopTask(const background_task::TaskParameters& task_params) override;

  [[maybe_unused]] background_task::TaskIds task_id_;
};

}  // namespace metrics
#endif  // CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_
