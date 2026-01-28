// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android/background_upload_task.h"

#include "base/notreached.h"
#include "base/types/pass_key.h"
#include "chrome/browser/browser_process.h"
#include "components/background_task_scheduler/background_task.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/background_task_scheduler/task_parameters.h"
#include "components/keyed_service/core/simple_factory_key.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/reporting_service.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "content/public/browser/browser_context.h"

namespace metrics {
namespace {

MetricsLogUploader::MetricServiceType TaskIdToMetricServiceType(
    background_task::TaskIds task_id) {
  switch (task_id) {
    case background_task::TaskIds::UMA_UPLOAD_JOB_ID:
      return MetricsLogUploader::MetricServiceType::UMA;
    case background_task::TaskIds::UKM_UPLOAD_JOB_ID:
      return MetricsLogUploader::MetricServiceType::UKM;
    case background_task::TaskIds::DWA_UPLOAD_JOB_ID:
      return MetricsLogUploader::MetricServiceType::DWA;
    case background_task::TaskIds::PUMA_UPLOAD_JOB_ID:
      return MetricsLogUploader::MetricServiceType::PRIVATE_METRICS;
    case background_task::TaskIds::STRUCTURED_METRICS_UPLOAD_JOB_ID:
      return MetricsLogUploader::MetricServiceType::STRUCTURED_METRICS;
    default:
      NOTREACHED();
  }
}

}  // namespace

BackgroundUploadTask::BackgroundUploadTask(background_task::TaskIds task_id)
    : task_id_(task_id) {}
BackgroundUploadTask::~BackgroundUploadTask() = default;

void BackgroundUploadTask::OnStartTaskInReducedMode(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    SimpleFactoryKey* key) {
  callback_ = std::move(callback);
}

void BackgroundUploadTask::OnStartTaskWithFullBrowser(
    const background_task::TaskParameters& task_params,
    background_task::TaskFinishedCallback callback,
    content::BrowserContext* browser_context) {
  StartUpload(std::move(callback));
}

void BackgroundUploadTask::OnFullBrowserLoaded(
    content::BrowserContext* browser_context) {
  StartUpload(std::move(callback_));
}

bool BackgroundUploadTask::OnStopTask(
    const background_task::TaskParameters& task_params) {
  // This is called when the OS wants to urgently stop this background task.
  // Return false to indicate that there is no need to re-schedule the task.
  // The various metrics services already have their own rescheduling mechanism.
  // (E.g., if as a result of this call, the network request to upload a log
  // will fail, the ReportingService will re-schedule the upload).
  return false;
}

void BackgroundUploadTask::StartUpload(
    background_task::TaskFinishedCallback callback) {
  // `callback` must be called when the background task is done (i.e. the upload
  // of a metrics log). The callback accepts a boolean, indicating whether the
  // task should be re-scheduled. Always return false -- the various metrics
  // services already have their own rescheduling mechanisms.
  base::OnceClosure done_callback =
      base::BindOnce(std::move(callback), /*reschedule=*/false);
  CHECK(g_browser_process);
  auto* manager = g_browser_process->GetMetricsServicesManager();
  CHECK(manager);
  manager->GetReportingService(TaskIdToMetricServiceType(task_id_))
      ->SendNextLogNow(base::PassKey<BackgroundUploadTask>(),
                       std::move(done_callback));
}

}  // namespace metrics
