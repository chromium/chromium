// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

namespace {

struct KioskSessionInternetAccessTestCase {
  std::string test_name;
  bool is_first_app_offline_enabled;
  bool is_second_app_offline_enabled;
  bool should_make_offline;
  KioskInternetAccessInfo expected_metric;
};

}  // namespace

class PeriodicMetricsServiceTest
    : public ::testing::TestWithParam<KioskSessionInternetAccessTestCase> {
 public:
  PeriodicMetricsServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        network_handler_test_helper_(
            std::make_unique<NetworkHandlerTestHelper>()),
        periodic_metrics_service_(local_state()),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {}

  PeriodicMetricsServiceTest(const PeriodicMetricsServiceTest&) = delete;
  PeriodicMetricsServiceTest& operator=(const PeriodicMetricsServiceTest&) =
      delete;

  void SetUp() override { network_handler_test_helper_->AddDefaultProfiles(); }

  void TearDown() override {
    network_handler_test_helper_.reset();
    local_state()->RemoveUserPref(prefs::kKioskMetrics);
  }

  base::test::TaskEnvironment* task_environment() { return &task_environment_; }

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  void RecordPreviousSessionMetrics() {
    periodic_metrics_service_.RecordPreviousSessionMetrics();
  }

  void StartRecordingPeriodicMetrics(bool is_offline_enabled = true) {
    periodic_metrics_service_.StartRecordingPeriodicMetrics(is_offline_enabled);
    // Some periodic metrics are calculated asynchronously.
    task_environment_.RunUntilIdle();
  }

  void MakeOffline() {
    network_handler_test_helper_->ResetDevicesAndServices();
  }

  void EmulateKioskRestart(bool is_first_app_offline_enabled,
                           bool is_second_app_offline_enabled) {
    // Emulate running a kiosk session.
    RecordPreviousSessionMetrics();
    StartRecordingPeriodicMetrics(is_first_app_offline_enabled);
    // Nothing was saved in prefs. That means the kiosk session started the
    // first time. So `kKioskSessionRestartInternetAccessHistogram` should be
    // empty.
    histogram_tester()->ExpectTotalCount(
        kKioskSessionRestartInternetAccessHistogram, 0);

    // Emulate kiosk session restart.
    RecordPreviousSessionMetrics();
    StartRecordingPeriodicMetrics(is_second_app_offline_enabled);
  }

 private:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  PeriodicMetricsService periodic_metrics_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

using InternetPeriodicMetricsServiceTest = PeriodicMetricsServiceTest;

TEST_F(PeriodicMetricsServiceTest, PeriodicMetrics) {
  const char* const kPeriodicMetrics[] = {
      kKioskRamUsagePercentageHistogram, kKioskSwapUsagePercentageHistogram,
      kKioskDiskUsagePercentageHistogram, kKioskChromeProcessCountHistogram};
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 0);
  }

  StartRecordingPeriodicMetrics();
  // Check that periodic metrics were recoreded right calling
  // `StartRecordingPeriodicMetrics`.
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 1);
  }

  // Next time periodic metrics should be recorded only after
  // `kPeriodicMetricsInterval`.
  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 1);
  }

  task_environment()->FastForwardBy(kPeriodicMetricsInterval / 2);
  for (const char* metric : kPeriodicMetrics) {
    histogram_tester()->ExpectTotalCount(metric, 2);
  }
}

TEST_P(InternetPeriodicMetricsServiceTest, KioskInternetMetric) {
  const KioskSessionInternetAccessTestCase& test_config = GetParam();
  if (test_config.should_make_offline)
    MakeOffline();

  EmulateKioskRestart(test_config.is_first_app_offline_enabled,
                      test_config.is_second_app_offline_enabled);
  histogram_tester()->ExpectBucketCount(
      kKioskSessionRestartInternetAccessHistogram, test_config.expected_metric,
      1);
  histogram_tester()->ExpectTotalCount(
      kKioskSessionRestartInternetAccessHistogram, 1);
}

INSTANTIATE_TEST_SUITE_P(
    InternetAccessInfos,
    InternetPeriodicMetricsServiceTest,
    testing::ValuesIn<KioskSessionInternetAccessTestCase>({
        {/*test_name=*/"OnlineAndAppSupportsOffline",
         /*is_first_app_offline_enabled=*/true,
         /*is_second_app_offline_enabled=*/true, /*should_make_offline=*/false,
         /*expected_metric=*/
         KioskInternetAccessInfo::kOnlineAndAppSupportsOffline},
        {/*test_name=*/"OfflineAndAppSupportsOffline",
         /*is_first_app_offline_enabled=*/true,
         /*is_second_app_offline_enabled=*/true, /*should_make_offline=*/true,
         /*expected_metric=*/
         KioskInternetAccessInfo::kOfflineAndAppSupportsOffline},
        {/*test_name=*/"OfflineAndAppRequiresInternet",
         /*is_first_app_offline_enabled=*/false,
         /*is_second_app_offline_enabled=*/false, /*should_make_offline=*/true,
         /*expected_metric=*/
         KioskInternetAccessInfo::kOfflineAndAppRequiresInternet},
        {/*test_name=*/"DifferentSupportOfflineMode",
         /*is_first_app_offline_enabled=*/false,
         /*is_second_app_offline_enabled=*/true, /*should_make_offline=*/false,
         /*expected_metric=*/
         KioskInternetAccessInfo::kOnlineAndAppRequiresInternet},
    }),
    [](const testing::TestParamInfo<
        InternetPeriodicMetricsServiceTest::ParamType>& info) {
      return info.param.test_name;
    });

}  // namespace ash
