// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android/background_upload_task.h"

#include "base/containers/flat_map.h"
#include "base/containers/map_util.h"
#include "base/no_destructor.h"
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

static auto& GetReportingServiceOverridesForTesting() {
  static base::NoDestructor<
      base::flat_map<MetricsLogUploader::MetricServiceType, ReportingService*>>
      reporting_service_overrides_for_testing;
  return *reporting_service_overrides_for_testing;
}

static auto& GetTaskDoneCallbackForTesting() {
  static base::NoDestructor<base::OnceClosure> task_done_callback_for_testing;
  return *task_done_callback_for_testing;
}

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

ReportingService* GetReportingService(background_task::TaskIds task_id) {
  MetricsLogUploader::MetricServiceType service_type =
      TaskIdToMetricServiceType(task_id);

  if (auto* value = base::FindOrNull(GetReportingServiceOverridesForTesting(),
                                     service_type)) {
    return *value;
  }

  CHECK(g_browser_process);
  auto* manager = g_browser_process->GetMetricsServicesManager();
  CHECK(manager);
  return manager->GetReportingService(service_type);
}

}  // namespace

BackgroundUploadTask::BackgroundUploadTask(background_task::TaskIds task_id)
    : task_id_(task_id) {}
BackgroundUploadTask::~BackgroundUploadTask() = default;

// static
void BackgroundUploadTask::SetReportingServiceForTesting(
    MetricsLogUploader::MetricServiceType service_type,
    ReportingService* service) {
  GetReportingServiceOverridesForTesting().insert_or_assign(service_type,
                                                            service);
}

// static
bool BackgroundUploadTask::UnsetReportingServiceForTesting(
    MetricsLogUploader::MetricServiceType service_type) {
  return GetReportingServiceOverridesForTesting().erase(service_type);
}

// static
void BackgroundUploadTask::SetTaskDoneCallbackForTesting(
    base::OnceClosure callback) {
  CHECK(GetTaskDoneCallbackForTesting().is_null());
  GetTaskDoneCallbackForTesting() = std::move(callback);
}

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
  if (!GetTaskDoneCallbackForTesting().is_null()) {
    done_callback = std::move(done_callback)
                        .Then(std::move(GetTaskDoneCallbackForTesting()));
  }

  ReportingService* reporting_service = GetReportingService(task_id_);

  // Tasks posted to the JobScheduler will actually persist even when the app is
  // killed/closed. For example, if a task is pending, but the app is closed,
  // the OS will re-start the application whenever it deems the task ready to
  // run, and the task will run in this completely new process. This means that
  // we may be trying to run a task here that was posted by a previous session,
  // and the ReportingService may not be ready for it (e.g. not fully
  // initialized yet). In fact, the target ReportingService may not even exist
  // if, say, it is gated behind a feature flag (e.g. task was posted in a
  // previous session when the feature was enabled, but ran in a new session
  // where the feature was disabled). As a result, only actually start the
  // upload if the corresponding ReportingService is actually expecting an
  // upload. This keeps the behaviour more consistent across platforms, and
  // ensures the ReportingService is actually ready to upload (though it is
  // technically possible to adapt ReportingService to gracefully handle these
  // "unplanned" uploads).
  if (!reporting_service ||
      !reporting_service->background_upload_task_scheduled()) {
    std::move(done_callback).Run();
    return;
  }

  reporting_service->SendNextLogNow(base::PassKey<BackgroundUploadTask>(),
                                    std::move(done_callback));
}

}  // namespace metrics
