// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "components/metrics/metrics_service.h"

#include <memory>
#include <string>
#include <vector>

#include "base/command_line.h"
#include "base/feature_list.h"
#include "base/memory/weak_ptr.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromeos_buildflags.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/metrics/chrome_metrics_service_accessor.h"
#include "chrome/browser/metrics/chrome_metrics_services_manager_client.h"
#include "chrome/browser/metrics/testing/sync_metrics_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/test/integration/sync_service_impl_harness.h"
#include "chrome/browser/sync/test/integration/sync_test.h"
#include "components/metrics/delegating_provider.h"
#include "components/metrics/demographics/demographic_metrics_provider.h"
#include "components/metrics/demographics/demographic_metrics_test_utils.h"
#include "components/metrics/metrics_switches.h"
#include "components/metrics_services_manager/metrics_services_manager.h"
#include "components/sync/service/sync_user_settings.h"
#include "content/public/test/browser_test.h"
#include "testing/gmock/include/gmock/gmock-matchers.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "third_party/metrics_proto/chrome_user_metrics_extension.pb.h"
#include "third_party/metrics_proto/user_demographics.pb.h"

namespace metrics {
namespace {

class MetricsServiceUserDemographicsBrowserTest
    : public SyncTest,
      public testing::WithParamInterface<test::DemographicsTestParams> {
 public:
  MetricsServiceUserDemographicsBrowserTest() : SyncTest(SINGLE_CLIENT) {
    if (GetParam().enable_feature) {
      // Enable UMA and reporting of the synced user's birth year and gender.
      scoped_feature_list_.InitWithFeatures(
          // enabled_features =
          {internal::kMetricsReportingFeature, kDemographicMetricsReporting},
          // disabled_features =
          {});
    } else {
      scoped_feature_list_.InitWithFeatures(
          // enabled_features =
          {internal::kMetricsReportingFeature},
          // disabled_features =
          {kDemographicMetricsReporting});
    }
  }

  MetricsServiceUserDemographicsBrowserTest(
      const MetricsServiceUserDemographicsBrowserTest&) = delete;
  MetricsServiceUserDemographicsBrowserTest& operator=(
      const MetricsServiceUserDemographicsBrowserTest&) = delete;

  void SetUpCommandLine(base::CommandLine* command_line) override {
    SyncTest::SetUpCommandLine(command_line);
    // Enable the metrics service for testing (in recording-only mode).
    command_line->AppendSwitch(switches::kMetricsRecordingOnly);
  }

  void SetUp() override {
    // Consent for metrics and crash reporting for testing.
    ChromeMetricsServiceAccessor::SetMetricsAndCrashReportingForTesting(
        &metrics_consent_);
    SyncTest::SetUp();
  }

  PrefService* local_state() { return g_browser_process->local_state(); }

  // Forces a log record to be generated. Returns a copy of the record on
  // success; otherwise, returns nullptr.
  std::unique_ptr<ChromeUserMetricsExtension> GenerateLogRecord() {
    MetricsService* metrics_service =
        g_browser_process->GetMetricsServicesManager()->GetMetricsService();
    test::BuildAndStoreLog(metrics_service);

    if (!test::HasUnsentLogs(metrics_service))
      return nullptr;
    return test::GetLastUmaLog(metrics_service);
  }

 private:
  bool metrics_consent_ = true;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// TODO(crbug.com/40103988): Add the remaining test cases.
// Keep this test in sync with testUMADemographicsReportingWithFeatureEnabled
// and testUMADemographicsReportingWithFeatureDisabled in
// ios/chrome/browser/metrics/demographics_egtest.mm.
IN_PROC_BROWSER_TEST_P(MetricsServiceUserDemographicsBrowserTest,
                       AddSyncedUserBirthYearAndGenderToProtoData) {
  test::DemographicsTestParams param = GetParam();

  base::HistogramTester histogram;

  const base::Time now = base::Time::Now();
  test::UpdateNetworkTime(now, g_browser_process->network_time_tracker());
  const int test_birth_year = test::GetMaximumEligibleBirthYear(now);
  const UserDemographicsProto::Gender test_gender =
      UserDemographicsProto::GENDER_FEMALE;

  // Add the test synced user birth year and gender priority prefs to the sync
  // server data.
  test::AddUserBirthYearAndGenderToSyncServer(GetFakeServer()->AsWeakPtr(),
                                              test_birth_year, test_gender);

  // TODO(crbug.com/40688248): Try to replace the below set-up code with
  // functions from SyncTest.
  Profile* test_profile = ProfileManager::GetLastUsedProfileIfLoaded();

  // Enable sync for the test profile.
  std::unique_ptr<SyncServiceImplHarness> harness =
      test::InitializeProfileForSync(test_profile,
                                     GetFakeServer()->AsWeakPtr());
  ASSERT_TRUE(harness->SetupSync());

  // Make sure that there is only one Profile to allow reporting the user's
  // birth year and gender.
  ASSERT_EQ(1, num_clients());

  // Generate a log record.
  std::unique_ptr<ChromeUserMetricsExtension> uma_proto = GenerateLogRecord();
  ASSERT_TRUE(uma_proto);

  // Check log content and the histogram.
  if (param.expect_reported_demographics) {
    EXPECT_EQ(test::GetNoisedBirthYear(local_state(), test_birth_year),
              uma_proto->user_demographics().birth_year());
    EXPECT_EQ(test_gender, uma_proto->user_demographics().gender());
    histogram.ExpectUniqueSample("UMA.UserDemographics.Status",
                                 UserDemographicsStatus::kSuccess, 1);
  } else {
    EXPECT_FALSE(uma_proto->has_user_demographics());
    histogram.ExpectTotalCount("UMA.UserDemographics.Status", /*count=*/0);
  }

#if !BUILDFLAG(IS_CHROMEOS)
  // Sign out the user to revoke all refresh tokens. This prevents any posted
  // tasks from successfully fetching an access token during the tear-down
  // phase and crashing on a DCHECK. See crbug/1102746 for more details.
  harness->SignOutPrimaryAccount();
#endif  // !BUILDFLAG(IS_CHROMEOS)
}

#if BUILDFLAG(IS_CHROMEOS_ASH)
// Cannot test for the enabled feature on Chrome OS because there are always
// multiple profiles.
static const auto kDemographicsTestParams = testing::Values(
    test::DemographicsTestParams{/*enable_feature=*/false,
                                 /*expect_reported_demographics=*/false});
#else
static const auto kDemographicsTestParams = testing::Values(
    test::DemographicsTestParams{/*enable_feature=*/false,
                                 /*expect_reported_demographics=*/false},
    test::DemographicsTestParams{/*enable_feature=*/true,
                                 /*expect_reported_demographics=*/true});
#endif

INSTANTIATE_TEST_SUITE_P(,
                         MetricsServiceUserDemographicsBrowserTest,
                         kDemographicsTestParams);

}  // namespace
}  // namespace metrics
