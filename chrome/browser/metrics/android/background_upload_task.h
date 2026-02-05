// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_
#define CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_

#include "components/background_task_scheduler/background_task.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/reporting_service.h"

namespace metrics {

class BackgroundUploadTask : public background_task::BackgroundTask {
 public:
  explicit BackgroundUploadTask(background_task::TaskIds task_id);
  ~BackgroundUploadTask() override;

  BackgroundUploadTask(const BackgroundUploadTask& other) = delete;
  BackgroundUploadTask& operator=(const BackgroundUploadTask& other) = delete;

  // Because this class is not instantiated directly, there is no good way to
  // pass in which ReportingService instance scheduled this task. StartUpload()
  // below will simply start the upload of whichever ReportingService is
  // registered with the browser, but in tests, it may actually have been
  // scheduled through a custom ReportingService that we manually instantiated.
  // This helper allows overriding which ReportingService will have its upload
  // started.
  static void SetReportingServiceForTesting(
      MetricsLogUploader::MetricServiceType service_type,
      ReportingService* service);

  // Removes an override set by SetReportingServiceForTesting().
  static bool UnsetReportingServiceForTesting(
      MetricsLogUploader::MetricServiceType service_type);

  // Sets a callback to run when the next upload task has finished running.
  static void SetTaskDoneCallbackForTesting(base::OnceClosure callback);

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

  // Starts the upload of the currently staged log.
  void StartUpload(background_task::TaskFinishedCallback callback);

  background_task::TaskIds task_id_;
  background_task::TaskFinishedCallback callback_;
};

}  // namespace metrics
#endif  // CHROME_BROWSER_METRICS_ANDROID_BACKGROUND_UPLOAD_TASK_H_
