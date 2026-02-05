// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/metrics/android/background_upload_task.h"

#include <memory>
#include <optional>
#include <string_view>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/run_loop.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "chrome/browser/android/background_task_scheduler/chrome_background_task_factory.h"
#include "chrome/test/base/platform_browser_test.h"
#include "components/background_task_scheduler/background_task_scheduler.h"
#include "components/background_task_scheduler/background_task_scheduler_factory.h"
#include "components/background_task_scheduler/task_ids.h"
#include "components/background_task_scheduler/task_info.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/log_store.h"
#include "components/metrics/metrics_features.h"
#include "components/metrics/metrics_log.h"
#include "components/metrics/metrics_log_uploader.h"
#include "components/metrics/metrics_logs_event_manager.h"
#include "components/metrics/metrics_reporting_service.h"
#include "components/metrics/private_metrics/private_metrics_reporting_service.h"
#include "components/metrics/reporting_service.h"
#include "components/metrics/structured/reporting/structured_metrics_reporting_service.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/metrics/test/test_metrics_service_client.h"
#include "components/metrics/unsent_log_store.h"
#include "components/prefs/testing_pref_service.h"
#include "components/ukm/ukm_reporting_service.h"
#include "content/public/test/browser_test.h"

namespace metrics {
namespace {

const char kLogData1[] = "data1";
const char kLogData2[] = "data2";

class TestBackgroundUploadMetricsServiceClient
    : public TestMetricsServiceClient {
 public:
  bool ShouldStartUpFast() const override { return true; }
  bool IsJobSchedulerSupported() const override {
    // TestMetricsServiceClient overrides this to always return false. Override
    // it again to reflect prod logic.
    return MetricsServiceClient::IsJobSchedulerSupported();
  }
};

}  // namespace

using MetricServiceType = MetricsLogUploader::MetricServiceType;

class BackgroundUploadTaskBrowserTest
    : public PlatformBrowserTest,
      public testing::WithParamInterface<MetricServiceType> {
 public:
  BackgroundUploadTaskBrowserTest() {
    scoped_feature_list_.InitAndEnableFeature(
        features::kMetricsLogJobSchedulerUpload);
  }

  ~BackgroundUploadTaskBrowserTest() override = default;

  void SetUpOnMainThread() override {
    // Need to manually call this in tests to initialize BackgroundTaskScheduler
    // on the Java side.
    ChromeBackgroundTaskFactory::SetAsDefault();

    UnsentLogStore::UnsentLogStoreLimits storage_limits =
        UnsentLogStore::UnsentLogStoreLimits{
            // Numbers here are not relevant to the test.
            .min_log_count = 10,
            .min_queue_size_bytes = 300 * 1024,  // 300 KiB
            .max_log_size_bytes = 1024 * 1024,   // 1 MiB
        };
    // Create the corresponding ReportingService.
    base::RepeatingCallback<void(const std::string&)> store_log_lambda;
    switch (service_type()) {
      case MetricServiceType::UMA: {
        MetricsReportingService::RegisterPrefs(local_state()->registry());
        auto reporting_service = std::make_unique<MetricsReportingService>(
            client(), local_state(), /*logs_event_manager=*/nullptr);
        log_store_ = reporting_service->metrics_log_store();
        store_log_lambda = base::BindRepeating(
            [](MetricsReportingService* service, const std::string& data) {
              service->metrics_log_store()->StoreLog(
                  data, MetricsLog::LogType::ONGOING_LOG, LogMetadata(),
                  MetricsLogsEventManager::CreateReason::kUnknown);
            },
            reporting_service.get());
        expected_histogram_name_ = "UMA.LogBackgroundUploadTaskPendingTime";
        reporting_service_ = std::move(reporting_service);
        break;
      }
      case MetricServiceType::UKM: {
        ukm::UkmReportingService::RegisterPrefs(local_state()->registry());
        auto reporting_service =
            std::make_unique<ukm::UkmReportingService>(client(), local_state());
        log_store_ = reporting_service->ukm_log_store();
        store_log_lambda = base::BindRepeating(
            [](ukm::UkmReportingService* service, const std::string& data) {
              service->ukm_log_store()->StoreLog(
                  data, LogMetadata(),
                  MetricsLogsEventManager::CreateReason::kUnknown);
            },
            reporting_service.get());
        expected_histogram_name_ = "UKM.LogBackgroundUploadTaskPendingTime";
        reporting_service_ = std::move(reporting_service);
        break;
      }
      case MetricServiceType::DWA: {
        private_metrics::PrivateMetricsReportingService::RegisterPrefs(
            local_state()->registry());
        auto reporting_service =
            std::make_unique<private_metrics::PrivateMetricsReportingService>(
                client(), local_state(), storage_limits,
                /*dwa_compatibility=*/true);
        log_store_ = reporting_service->unsent_log_store();
        store_log_lambda = base::BindRepeating(
            [](private_metrics::PrivateMetricsReportingService* service,
               const std::string& data) {
              service->unsent_log_store()->StoreLog(
                  data, LogMetadata(),
                  MetricsLogsEventManager::CreateReason::kUnknown);
            },
            reporting_service.get());
        expected_histogram_name_ = "DWA.LogBackgroundUploadTaskPendingTime";
        reporting_service_ = std::move(reporting_service);
        break;
      }
      case MetricServiceType::PRIVATE_METRICS: {
        private_metrics::PrivateMetricsReportingService::RegisterPrefs(
            local_state()->registry());
        auto reporting_service =
            std::make_unique<private_metrics::PrivateMetricsReportingService>(
                client(), local_state(), storage_limits,
                /*dwa_compatibility=*/false);
        log_store_ = reporting_service->unsent_log_store();
        store_log_lambda = base::BindRepeating(
            [](private_metrics::PrivateMetricsReportingService* service,
               const std::string& data) {
              service->unsent_log_store()->StoreLog(
                  data, LogMetadata(),
                  MetricsLogsEventManager::CreateReason::kUnknown);
            },
            reporting_service.get());
        expected_histogram_name_ =
            "PrivateMetrics.LogBackgroundUploadTaskPendingTime";
        reporting_service_ = std::move(reporting_service);
        break;
      }
      case MetricServiceType::STRUCTURED_METRICS: {
        structured::reporting::StructuredMetricsReportingService::RegisterPrefs(
            local_state()->registry());
        auto reporting_service = std::make_unique<
            structured::reporting::StructuredMetricsReportingService>(
            client(), local_state(), storage_limits);
        log_store_ = reporting_service->log_store();
        store_log_lambda = base::BindRepeating(
            [](structured::reporting::StructuredMetricsReportingService*
                   service,
               const std::string& data) {
              service->StoreLog(
                  data, MetricsLogsEventManager::CreateReason::kUnknown);
            },
            reporting_service.get());
        expected_histogram_name_ =
            "StructuredMetrics.LogBackgroundUploadTaskPendingTime";
        reporting_service_ = std::move(reporting_service);
        break;
      }
    }
    ASSERT_TRUE(reporting_service_);
    ASSERT_TRUE(log_store_);

    // Store two logs.
    reporting_service_->Initialize();
    store_log_lambda.Run(kLogData1);
    store_log_lambda.Run(kLogData2);

    // Set an override so that scheduled `BackgroundUploadTask`s do uploads
    // through the one we just instantiated rather than the browser-level one.
    BackgroundUploadTask::SetReportingServiceForTesting(
        service_type(), reporting_service_.get());

    PlatformBrowserTest::SetUpOnMainThread();
  }

  void TearDownOnMainThread() override {
    // Remove override for good measure.
    BackgroundUploadTask::UnsetReportingServiceForTesting(service_type());
    PlatformBrowserTest::TearDownOnMainThread();
  }

  TestingPrefServiceSimple* local_state() { return &local_state_; }
  TestBackgroundUploadMetricsServiceClient* client() { return &client_; }
  std::string_view expected_histogram_name() {
    return expected_histogram_name_;
  }
  ReportingService* reporting_service() { return reporting_service_.get(); }
  LogStore* log_store() { return log_store_; }
  MetricServiceType service_type() const { return GetParam(); }

 private:
  TestingPrefServiceSimple local_state_;
  TestBackgroundUploadMetricsServiceClient client_;
  std::string_view expected_histogram_name_;
  std::unique_ptr<ReportingService> reporting_service_;
  raw_ptr<LogStore> log_store_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// Verifies that on Android, the various metrics systems schedule upload tasks
// through JobScheduler, and that those tasks actually get run.
IN_PROC_BROWSER_TEST_P(BackgroundUploadTaskBrowserTest, BackgroundUploadTask) {
  base::HistogramTester histogram_tester;
  // 2 logs were created in SetUp(), and fast startup is enabled in `client()`.
  // Hence, enabling reporting should immediately post an upload task to the
  // main thread (i.e. `ReportingService::SendNextLogWhenPossible()`). This
  // task, in turn, should schedule a BackgroundUploadTask with JobScheduler.
  ASSERT_FALSE(client()->uploader());
  ASSERT_FALSE(log_store()->has_staged_log());
  ASSERT_TRUE(log_store()->has_unsent_logs());
  histogram_tester.ExpectTotalCount(expected_histogram_name(), 0);
  reporting_service()->EnableReporting();

  // Wait until we have started uploading. This should mean that the
  // BackgroundUploadTask that was scheduled with JobScheduler has run.
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return client()->uploader() && client()->uploader()->is_uploading();
  }));
  ASSERT_TRUE(log_store()->has_staged_log());
  std::string decoded_log_data;
  DecodeLogData(log_store()->staged_log(), &decoded_log_data);
  EXPECT_EQ(decoded_log_data, kLogData2);
  // A histogram measuring the time the BackgroundUploadTask was pending should
  // have been emitted.
  histogram_tester.ExpectTotalCount(expected_histogram_name(), 1);
  // Complete the upload.
  client()->uploader()->CompleteUpload(200);

  // BackgroundUploadTask requires a TaskFinishedCallback to be called when the
  // background task has finished in order for further background tasks to be
  // scheduled with JobScheduler. Verify that it was properly called by making
  // sure that the scheduling loop works and that uploading the second log goes
  // as expected.
  ASSERT_FALSE(log_store()->has_staged_log());
  ASSERT_TRUE(log_store()->has_unsent_logs());
  ASSERT_FALSE(client()->uploader()->is_uploading());
  // Wait until we have started uploading. This should mean that the second
  // BackgroundUploadTask that was scheduled with JobScheduler has run.
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return client()->uploader()->is_uploading(); }));
  ASSERT_TRUE(log_store()->has_staged_log());
  DecodeLogData(log_store()->staged_log(), &decoded_log_data);
  EXPECT_EQ(decoded_log_data, kLogData1);
  // The timing histogram should have been emitted again (2 total samples).
  histogram_tester.ExpectTotalCount(expected_histogram_name(), 2);
  // Complete the upload.
  client()->uploader()->CompleteUpload(200);

  // We should be done.
  ASSERT_FALSE(log_store()->has_staged_log());
  ASSERT_FALSE(log_store()->has_unsent_logs());
  ASSERT_FALSE(client()->uploader()->is_uploading());
}

// Verifies that if the ReportingService is not expecting an upload, the
// background upload task is a no-op. This can happen an upload task for a
// service was scheduled in a previous session/process, and then run in a newly
// run session/process where the ReportingService is not expecting any uploads
// (i.e. hasn't itself triggered the task).
IN_PROC_BROWSER_TEST_P(BackgroundUploadTaskBrowserTest,
                       ReportingServiceNotReady) {
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  BackgroundUploadTask::SetTaskDoneCallbackForTesting(run_loop.QuitClosure());

  // Manually schedule an upload task here, outside of the typical uploading
  // loop handled by MetricsUploadScheduler of ReportingService. This simulates
  // an "external" task being run (e.g. one scheduled by a previous session).
  background_task::OneOffInfo one_off;
  background_task::TaskInfo task_info(
      reporting_service()->background_upload_task_id(), one_off);
  ASSERT_TRUE(
      background_task::BackgroundTaskSchedulerFactory::GetScheduler()->Schedule(
          task_info));
  // Wait until the upload task runs and terminates.
  run_loop.Run();

  // Verify that we still have the two logs present (none were uploaded), and no
  // timing histograms were emitted.
  ASSERT_FALSE(client()->uploader());
  ASSERT_FALSE(log_store()->has_staged_log());
  ASSERT_TRUE(log_store()->has_unsent_logs());
  log_store()->StageNextLog();
  log_store()->DiscardStagedLog();
  ASSERT_FALSE(log_store()->has_staged_log());
  ASSERT_TRUE(log_store()->has_unsent_logs());
  log_store()->StageNextLog();
  log_store()->DiscardStagedLog();
  EXPECT_FALSE(log_store()->has_unsent_logs());
  histogram_tester.ExpectTotalCount(expected_histogram_name(), 0);
}

// Verifies that if the ReportingService does not exist, the background upload
// task is a no-op. This can happen an upload task for a service was scheduled
// in a previous session/process, and then run in a newly run session/process
// where said service does not exist anymore (e.g. due to being disabled by a
// feature).
IN_PROC_BROWSER_TEST_P(BackgroundUploadTaskBrowserTest, ReportingServiceNull) {
  base::HistogramTester histogram_tester;

  base::RunLoop run_loop;
  BackgroundUploadTask::SetTaskDoneCallbackForTesting(run_loop.QuitClosure());
  // Set the ReportingService to null.
  BackgroundUploadTask::SetReportingServiceForTesting(service_type(), nullptr);

  // Manually schedule an upload task here, outside of the typical uploading
  // loop handled by MetricsUploadScheduler of ReportingService. This simulates
  // an "external" task being run (e.g. one scheduled by a previous session).
  background_task::OneOffInfo one_off;
  background_task::TaskInfo task_info(
      reporting_service()->background_upload_task_id(), one_off);
  ASSERT_TRUE(
      background_task::BackgroundTaskSchedulerFactory::GetScheduler()->Schedule(
          task_info));
  // Wait until the upload task runs and terminates.
  run_loop.Run();

  // We did not crash, which indicates that the upload task was properly
  // ignored. Verify further that no timing histograms were emitted.
  histogram_tester.ExpectTotalCount(expected_histogram_name(), 0);

  // The ReportingService override is unset in TearDownOnMainThread().
}

INSTANTIATE_TEST_SUITE_P(
    BackgroundUploadTaskBrowserTests,
    BackgroundUploadTaskBrowserTest,
    testing::Values(MetricServiceType::UMA,
                    MetricServiceType::UKM,
                    MetricServiceType::DWA,
                    MetricServiceType::PRIVATE_METRICS,
                    MetricServiceType::STRUCTURED_METRICS));

}  // namespace metrics
