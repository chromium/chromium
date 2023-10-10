// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include "build/build_config.h"

#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/profiles/profile_test_util.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "chrome/browser/unified_consent/unified_consent_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
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

metrics::structured::StructuredMetricsService* GetSMService() {
  return g_browser_process->GetMetricsServicesManager()
      ->GetStructuredMetricsService();
}

MetricsServiceClient* GetMetricsServiceClient() {
  return GetSMService()->GetMetricsServiceClient();
}

// A helper object for overriding metrics enabled state.
class MetricsConsentOverride {
 public:
  explicit MetricsConsentOverride(bool initial_state) : state_(initial_state) {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &state_);
    Update(initial_state);
  }

  ~MetricsConsentOverride() {
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        nullptr);
  }

  void Update(bool state) {
    state_ = state;
    // Trigger rechecking of metrics state.
    g_browser_process->GetMetricsServicesManager()->UpdateUploadPermissions(
        /*may_upload=*/true);
  }

 private:
  bool state_;
};

class StructuredMetricsServiceTestBase : public InProcessBrowserTest {
 public:
  StructuredMetricsServiceTestBase() = default;

  bool HasUnsentLogs() {
    return GetSMService()->reporting_service_->log_store()->has_unsent_logs();
  }

  bool HasStagedLog() {
    return GetSMService()->reporting_service_->log_store()->has_staged_log();
  }

  void Wait() { base::RunLoop().RunUntilIdle(); }

  std::unique_ptr<ChromeUserMetricsExtension> GetStagedLog() {
    if (!HasUnsentLogs()) {
      return nullptr;
    }

    auto* log_store = GetSMService()->reporting_service_->log_store();
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
};

class TestStructuredMetricsService : public StructuredMetricsServiceTestBase {
 public:
  TestStructuredMetricsService() {
    feature_list_.InitAndEnableFeature(
        metrics::structured::kEnabledStructuredMetricsService);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

class TestStructuredMetricsServiceDisabled
    : public StructuredMetricsServiceTestBase {
 public:
  TestStructuredMetricsServiceDisabled() {
    feature_list_.InitAndDisableFeature(
        metrics::structured::kEnabledStructuredMetricsService);
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, EnabledWithConsent) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  MetricsConsentOverride metrics_consent(true);

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, DisabledWhenRevoked) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  MetricsConsentOverride metrics_consent(true);

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Revoke consent.
  metrics_consent.Update(false);

  // Verify that recording and reporting are disabled.
  EXPECT_FALSE(sm_service->recording_enabled());
  EXPECT_FALSE(sm_service->reporting_active());
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService,
                       // TODO(crbug.com/1482522): Re-enable this test
                       DISABLED_InMemoryPurgeOnConsentRevoke) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  MetricsConsentOverride metrics_consent(true);

  // Wait for the consent to propagate.
  Wait();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Record a couple of events and verify that they are recorded.
  structured::events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("metric one")
      .SetTestMetricTwo(10)
      .Record();

  structured::events::v2::test_project_five::TestEventSix()
      .SetTestMetricSix("metric six")
      .Record();

  // There should be at least the 2 events recorded above. There could be others
  // such as login event.
  EXPECT_THAT(sm_service->recorder()->events()->non_uma_events_size(),
              testing::Ge(2));

  // Change the consent to force a purge.
  metrics_consent.Update(false);

  // There shouldn't be any staged or un-staged logs and no in-memory events.
  EXPECT_FALSE(HasUnsentLogs());
  EXPECT_FALSE(HasStagedLog());
  EXPECT_EQ(sm_service->recorder()->events()->non_uma_events_size(), 0);
  EXPECT_EQ(sm_service->recorder()->events()->uma_events_size(), 0);
}

// TODO(crbug.com/1482059): Re-enable this test
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
  MetricsConsentOverride metrics_consent(true);

  // Wait for the consent to propagate.
  Wait();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  // Record a couple of events and verify that they are recorded.
  structured::events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("metric one")
      .SetTestMetricTwo(10)
      .Record();

  structured::events::v2::test_project_five::TestEventSix()
      .SetTestMetricSix("metric six")
      .Record();

  // There should be at least the 2 events recorded above. There could be others
  // such as login event.
  EXPECT_THAT(sm_service->recorder()->events()->non_uma_events_size(),
              testing::Ge(2));

  // Flush the in-memory events to a staged log.
  sm_service->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  // Change the consent to force a purge.
  metrics_consent.Update(false);

  // There shouldn't be any staged or un-staged logs and no in-memory events.
  EXPECT_FALSE(HasUnsentLogs());
  EXPECT_FALSE(HasStagedLog());
  EXPECT_EQ(sm_service->recorder()->events()->non_uma_events_size(), 0);
  EXPECT_EQ(sm_service->recorder()->events()->uma_events_size(), 0);
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsService, SystemProfilePopulated) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  MetricsConsentOverride metrics_consent(true);

  // Wait for the consent to propagate.
  Wait();

  // Verify that recording and reporting are enabled.
  EXPECT_TRUE(sm_service->recording_enabled());
  EXPECT_TRUE(sm_service->reporting_active());

  Wait();

  // Record an event inorder to build a log.
  structured::events::v2::test_project_one::TestEventOne()
      .SetTestMetricOne("metric one")
      .SetTestMetricTwo(10)
      .Record();

  Wait();

  // Flush the in-memory events to a staged log.
  sm_service->Flush(metrics::MetricsLogsEventManager::CreateReason::kUnknown);

  std::unique_ptr<ChromeUserMetricsExtension> uma_proto = GetStagedLog();
  ASSERT_NE(uma_proto.get(), nullptr);

  // Verify that the SystemProfile has been set appropriately.
  const SystemProfileProto& system_profile = uma_proto->system_profile();
  EXPECT_EQ(system_profile.app_version(),
            GetMetricsServiceClient()->GetVersionString());
}

IN_PROC_BROWSER_TEST_F(TestStructuredMetricsServiceDisabled,
                       ValidStateWhenDisabled) {
  auto* sm_service = GetSMService();

  // Enable consent for profile.
  MetricsConsentOverride metrics_consent(true);

  // Everything should be null expect the recorder. The recorder is used by
  // StructuredMetricsProvider when the service is disabled; therefore, it
  // cannot be null.
  EXPECT_THAT(sm_service->recorder(), testing::NotNull());
  EXPECT_THAT(sm_service->reporting_service_.get(), testing::IsNull());
  EXPECT_THAT(sm_service->scheduler_.get(), testing::IsNull());
}

}  // namespace metrics
