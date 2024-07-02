// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <utility>

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/structured/test/structured_metrics_mixin.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/common/chrome_features.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/metrics/log_decoder.h"
#include "components/metrics/metrics_service_client.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/structured_metrics_service.h"
#include "components/metrics/unsent_log_store.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/sync/test/fake_server_network_resources.h"
#include "components/unified_consent/unified_consent_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"

namespace metrics {
namespace {

// The name hash of "TestProjectOne".
constexpr uint64_t kProjectOneHash = UINT64_C(16881314472396226433);

// The name hash of "TestProjectFive".
constexpr uint64_t kProjectFiveHash = UINT64_C(3960582687892677139);

// The name hash of "TestProjectSeven".
constexpr uint64_t kProjectSevenHash = UINT64_C(10319251808101486833);

// The name hash of "chrome::TestProjectOne::TestEventOne".
constexpr uint64_t kEventOneHash = UINT64_C(13593049295042080097);

// The name hash of "chrome::TestProjectFive::TestEventSix".
constexpr uint64_t kEventSixHash = UINT64_C(2873337042686447043);

// The name hash of "chrome::TestProjectSeven::TestEventEight".
constexpr uint64_t kEventEightHash = UINT64_C(16290206418240617738);

structured::StructuredMetricsService* GetSMService() {
  return g_browser_process->GetMetricsServicesManager()
      ->GetStructuredMetricsService();
}

}  // namespace

class StructuredMetricsServiceTestBase : public MixinBasedInProcessBrowserTest {
 public:
  StructuredMetricsServiceTestBase() = default;

  void SetUpOnMainThread() override {
    structured_metrics_mixin_.SetUpOnMainThread();
  }

  bool HasUnsentLogs() {
    return GetSMService()->log_store()->has_unsent_logs();
  }

  bool HasStagedLog() { return GetSMService()->log_store()->has_staged_log(); }

  void WaitForConsentChanges() { base::RunLoop().RunUntilIdle(); }
  void WaitUntilKeysReady() { structured_metrics_mixin_.WaitUntilKeysReady(); }

  std::unique_ptr<ChromeUserMetricsExtension> GetStagedLog() {
    if (!HasUnsentLogs()) {
      return nullptr;
    }

    auto* log_store = GetSMService()->log_store();
    if (log_store->has_staged_log()) {
      // For testing purposes, we examine the content of a staged log without
      // ever sending the log, so discard any previously staged log.
      log_store->DiscardStagedLog();
    }

    log_store->StageNextLog();
    if (!log_store->has_staged_log()) {
      return nullptr;
    }

    std::unique_ptr<ChromeUserMetricsExtension> uma_proto =
        std::make_unique<ChromeUserMetricsExtension>();
    if (!metrics::DecodeLogDataToProto(log_store->staged_log(),
                                       uma_proto.get())) {
      return nullptr;
    }
    return uma_proto;
  }

 protected:
  structured::StructuredMetricsMixin structured_metrics_mixin_{&mixin_host_};
};

class TestStructuredMetricsService : public StructuredMetricsServiceTestBase {
 public:
  TestStructuredMetricsService() {
    feature_list_.InitWithFeatures(
        {metrics::structured::kEnabledStructuredMetricsService,
         ::features::kChromeStructuredMetrics},
        {});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, EnabledWithConsent) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, DisabledWhenRevoked) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Revoke consent.
  structured_metrics_mixin_.UpdateRecordingState(false);

  // Verify that recording and reporting are disabled.
  EXPECT_FALSE(sm_service->recording_enabled());
  EXPECT_FALSE(sm_service->reporting_active());
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService,
                       // TODO(crbug.com/40931527): Re-enable this test
                       DISABLED_InMemoryPurgeOnConsentRevoke) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  WaitForConsentChanges();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Keys must be ready for events to be recorded actively. Otherwise, they will
  // be queued to be recorded and the test may fail.
  WaitUntilKeysReady();

  // Record a couple of events and verify that they are recorded.
  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_one::TestEventOne()
          .SetTestMetricOne("metric one")
          .SetTestMetricTwo(10));

  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_five::TestEventSix()
          .SetTestMetricSix("metric six"));

  // This will timeout and fail the test if events have not been recorded
  // successfully.
  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectOneHash,
                                                   kEventOneHash);
  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectFiveHash,
                                                   kEventSixHash);

  // Change the consent to force a purge.
  structured_metrics_mixin_.UpdateRecordingState(false);

  // There shouldn't be any staged or un-staged logs and no in-memory events.
  EXPECT_FALSE(HasUnsentLogs());
  EXPECT_FALSE(HasStagedLog());
  EXPECT_EQ(sm_service->recorder()->event_storage()->RecordedEventsCount(), 0);
}

// TODO(crbug.com/40931189): Re-enable this test
// Only flaky on chromeos-rel.
#if BUILDFLAG(IS_CHROMEOS) && defined(NDEBUG) && !defined(ADDRESS_SANITIZER)
#define MAYBE_StagedLogPurgeOnConsentRevoke \
  DISABLED_StagedLogPurgeOnConsentRevoke
#else
#define MAYBE_StagedLogPurgeOnConsentRevoke StagedLogPurgeOnConsentRevoke
#endif
IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService,
                       MAYBE_StagedLogPurgeOnConsentRevoke) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  WaitForConsentChanges();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Keys must be ready for events to be recorded actively. Otherwise, they will
  // be queued to be recorded and the test may fail.
  WaitUntilKeysReady();

  // Record a couple of events and verify that they are recorded.
  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_one::TestEventOne()
          .SetTestMetricOne("metric one")
          .SetTestMetricTwo(10));

  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_five::TestEventSix()
          .SetTestMetricSix("metric six"));

  // This will timeout and fail the test if events have not been recorded
  // successfully.
  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectOneHash,
                                                   kEventOneHash);
  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectFiveHash,
                                                   kEventSixHash);

  // Flush the in-memory events to a staged log.
  sm_service->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Change the consent to force a purge.
  structured_metrics_mixin_.UpdateRecordingState(false);

  // There shouldn't be any staged or un-staged logs and no in-memory events.
  EXPECT_FALSE(HasUnsentLogs());
  EXPECT_FALSE(HasStagedLog());
  EXPECT_EQ(sm_service->recorder()->event_storage()->RecordedEventsCount(), 0);
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, SystemProfilePopulated) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  // Wait for the consent to propagate.
  WaitForConsentChanges();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  WaitUntilKeysReady();

  // Record an event inorder to build a log.
  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_one::TestEventOne()
          .SetTestMetricOne("metric one")
          .SetTestMetricTwo(10));

  // This will timeout and fail the test if events have not been recorded
  // successfully.
  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectOneHash,
                                                   kEventOneHash);

  // Flush the in-memory events to a staged log.
  sm_service->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  std::unique_ptr<ChromeUserMetricsExtension> uma_proto = GetStagedLog();
  ASSERT_NE(uma_proto.get(), nullptr);

  // Verify that the SystemProfile has been set appropriately.
  const SystemProfileProto& system_profile = uma_proto->system_profile();
  EXPECT_EQ(system_profile.app_version(),
            GetSMService()->GetMetricsServiceClient()->GetVersionString());
}
#endif  //  BUILDFLAG(IS_CHROMEOS_ASH)

class TestStructuredMetricsServiceDisabled
    : public StructuredMetricsServiceTestBase {
 public:
  TestStructuredMetricsServiceDisabled() {
    feature_list_.InitWithFeatures(
        {::features::kChromeStructuredMetrics},
        {metrics::structured::kEnabledStructuredMetricsService});
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsServiceDisabled,
                       ValidStateWhenDisabled) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  structured_metrics_mixin_.UpdateRecordingState(true);

  // Everything should be null expect the recorder. The recorder is used by
  // StructuredMetricsProvider when the service is disabled; therefore, it
  // cannot be null.
  EXPECT_THAT(sm_service->recorder(), testing::NotNull());
  EXPECT_THAT(sm_service->reporting_service_.get(), testing::IsNull());
  EXPECT_THAT(sm_service->scheduler_.get(), testing::IsNull());
}

// TODO(crbug.com/41485716): Flaky on linux-chromeos-rel.
IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService,
                       DISABLED_PurgeForceRecordedEvents) {
  // This feature is intended only for events that are recorded before user
  // consent.
  auto* sm_service = GetSMService();
  // Simulates an initial state of disabled metrics.
  structured_metrics_mixin_.UpdateRecordingState(false);
  WaitForConsentChanges();

  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_seven::TestEventEight());

  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectSevenHash,
                                                   kEventEightHash);

  // Confirms the disabled metrics when consent is not given.
  structured_metrics_mixin_.UpdateRecordingState(false);
  WaitForConsentChanges();

  EXPECT_FALSE(HasUnsentLogs());
  EXPECT_FALSE(HasStagedLog());
  EXPECT_EQ(sm_service->recorder()->event_storage()->RecordedEventsCount(), 0);
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, CreateLogs) {
  auto* sm_service = GetSMService();
  structured_metrics_mixin_.UpdateRecordingState(true);
  WaitForConsentChanges();

  structured::StructuredMetricsClient::Record(
      structured::events::v2::test_project_seven::TestEventEight());

  structured_metrics_mixin_.WaitUntilEventRecorded(kProjectSevenHash,
                                                   kEventEightHash);

  // Makes sure that the logs are created without issues.
  // Disable upload, CreateLogs: creates the logs and starts the upload process.
  base::RunLoop run_loop;
  sm_service->SetCreateLogsCallbackInTests(run_loop.QuitClosure());
  sm_service->CreateLogs(
      metrics::MetricsLogsEventManager::CreateReason::kUnknown,
      /*notify_scheduler=*/false);
  run_loop.Run();

  EXPECT_TRUE(HasUnsentLogs());

  std::unique_ptr<ChromeUserMetricsExtension> uma_proto = GetStagedLog();
  EXPECT_NE(uma_proto.get(), nullptr);

  EXPECT_EQ(uma_proto->structured_data().events_size(), 1);
}

}  // namespace metrics
