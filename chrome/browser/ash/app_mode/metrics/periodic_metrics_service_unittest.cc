// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_mode/metrics/periodic_metrics_service.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/network/network_handler_test_helper.h"
#include "chromeos/ash/components/sync_wifi/network_test_helper.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/user_activity/user_activity_detector.h"

namespace ash {

namespace {

struct KioskSessionInternetAccessTestCase {
  std::string test_name;
  bool is_first_app_offline_enabled;
  bool is_second_app_offline_enabled;
  bool should_make_offline;
  KioskInternetAccessInfo expected_metric;
};

struct KioskSessionUserActivityTestCase {
  std::string test_name;
  base::TimeDelta idle_timeout;
  KioskUserActivity expected_metric;
};

}  // namespace

class BasePeriodicMetricsServiceTest {
 public:
  BasePeriodicMetricsServiceTest()
      : task_environment_(base::test::TaskEnvironment::TimeSource::MOCK_TIME),
        local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())),
        network_handler_test_helper_(
            std::make_unique<NetworkHandlerTestHelper>()),
        periodic_metrics_service_(local_state()),
        histogram_tester_(std::make_unique<base::HistogramTester>()) {}

  BasePeriodicMetricsServiceTest(const BasePeriodicMetricsServiceTest&) =
      delete;
  BasePeriodicMetricsServiceTest& operator=(
      const BasePeriodicMetricsServiceTest&) = delete;

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

  void EmulateKioskRestart(bool is_first_app_offline_enabled = true,
                           bool is_second_app_offline_enabled = true) {
    // Emulate running a kiosk session.
    RecordPreviousSessionMetrics();
    StartRecordingPeriodicMetrics(is_first_app_offline_enabled);
    // Nothing was saved in prefs. That means the kiosk session started the
    // first time.
    histogram_tester()->ExpectTotalCount(
        kKioskSessionRestartInternetAccessHistogram, 0);
    histogram_tester()->ExpectTotalCount(
        kKioskSessionRestartUserActivityHistogram, 0);

    // Emulate kiosk session restart.
    RecordPreviousSessionMetrics();
    StartRecordingPeriodicMetrics(is_second_app_offline_enabled);
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  sync_preferences::TestingPrefServiceSyncable user_prefs_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
  std::unique_ptr<NetworkHandlerTestHelper> network_handler_test_helper_;
  PeriodicMetricsService periodic_metrics_service_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
};

class PeriodicMetricsServiceTest : public BasePeriodicMetricsServiceTest,
                                   public ::testing::Test {
 public:
  PeriodicMetricsServiceTest() = default;
  PeriodicMetricsServiceTest(const PeriodicMetricsServiceTest&) = delete;
  PeriodicMetricsServiceTest& operator=(const PeriodicMetricsServiceTest&) =
      delete;
};

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

class InternetPeriodicMetricsServiceTest
    : public ::testing::TestWithParam<KioskSessionInternetAccessTestCase>,
      public BasePeriodicMetricsServiceTest {
 public:
  InternetPeriodicMetricsServiceTest() = default;
  InternetPeriodicMetricsServiceTest(
      const InternetPeriodicMetricsServiceTest&) = delete;
  InternetPeriodicMetricsServiceTest& operator=(
      const InternetPeriodicMetricsServiceTest&) = delete;

  void SetUp() override { network_handler_test_helper_->AddDefaultProfiles(); }

  void TearDown() override {
    network_handler_test_helper_.reset();
    local_state()->RemoveUserPref(prefs::kKioskMetrics);
  }

  void MakeOffline() {
    network_handler_test_helper_->ResetDevicesAndServices();
  }
};

TEST_P(InternetPeriodicMetricsServiceTest, KioskInternetMetric) {
  const KioskSessionInternetAccessTestCase& test_config = GetParam();
  if (test_config.should_make_offline) {
    MakeOffline();
  }

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

class UserActivityPeriodicMetricsServiceTest
    : public ::testing::TestWithParam<KioskSessionUserActivityTestCase>,
      public BasePeriodicMetricsServiceTest {
 public:
  UserActivityPeriodicMetricsServiceTest() = default;
  UserActivityPeriodicMetricsServiceTest(
      const UserActivityPeriodicMetricsServiceTest&) = delete;
  UserActivityPeriodicMetricsServiceTest& operator=(
      const UserActivityPeriodicMetricsServiceTest&) = delete;

  void TearDown() override {
    local_state()->RemoveUserPref(prefs::kKioskMetrics);
  }

  void SetDeviceIdleTime(base::TimeDelta idle_time,
                         base::TimeTicks from_time = base::TimeTicks::Now()) {
    ui::UserActivityDetector::Get()->set_last_activity_time_for_test(from_time -
                                                                     idle_time);
  }
};

TEST_P(UserActivityPeriodicMetricsServiceTest, KioskUserActivityMetric) {
  const KioskSessionUserActivityTestCase& test_config = GetParam();

  SetDeviceIdleTime(test_config.idle_timeout);
  EmulateKioskRestart();

  histogram_tester()->ExpectBucketCount(
      kKioskSessionRestartUserActivityHistogram, test_config.expected_metric,
      1);
  histogram_tester()->ExpectTotalCount(
      kKioskSessionRestartUserActivityHistogram, 1);
}

INSTANTIATE_TEST_SUITE_P(
    UserActivityInfos,
    UserActivityPeriodicMetricsServiceTest,
    testing::ValuesIn<KioskSessionUserActivityTestCase>({
        {/*test_name=*/"UserActiv",
         /*idle_timeout=*/base::Seconds(0),
         /*expected_metric=*/KioskUserActivity::kActive},
        {/*test_name=*/"UserAlmostIdle",
         /*idle_timeout=*/kFirstIdleTimeout / 2,
         /*expected_metric=*/KioskUserActivity::kActive},
        {/*test_name=*/"UserIdle",
         /*idle_timeout=*/kFirstIdleTimeout,
         /*expected_metric=*/KioskUserActivity::kIdle},
    }),
    [](const testing::TestParamInfo<
        UserActivityPeriodicMetricsServiceTest::ParamType>& info) {
      return info.param.test_name;
    });

// Check that after the first metrics record, ChromeOS is using a new idle
// timeout.
TEST_F(UserActivityPeriodicMetricsServiceTest, UserActivityTimeoutChanged) {
  ASSERT_TRUE(kFirstIdleTimeout < kRegularIdleTimeout / 2);
  SetDeviceIdleTime(kFirstIdleTimeout);

  // Emulate running a kiosk session. The user is idle.
  RecordPreviousSessionMetrics();
  StartRecordingPeriodicMetrics();

  // User was active during.
  SetDeviceIdleTime(
      kRegularIdleTimeout / 2,
      /*from_time=*/base::TimeTicks::Now() + kPeriodicMetricsInterval);
  task_environment()->FastForwardBy(kPeriodicMetricsInterval);

  // Emulate kiosk session restart.
  RecordPreviousSessionMetrics();

  histogram_tester()->ExpectBucketCount(
      kKioskSessionRestartUserActivityHistogram, KioskUserActivity::kActive, 1);
  histogram_tester()->ExpectTotalCount(
      kKioskSessionRestartUserActivityHistogram, 1);
}

TEST_F(UserActivityPeriodicMetricsServiceTest, UserBecomesIdle) {
  ASSERT_TRUE(kFirstIdleTimeout < kRegularIdleTimeout / 2);
  SetDeviceIdleTime(kFirstIdleTimeout / 2);

  // Emulate running a kiosk session.
  RecordPreviousSessionMetrics();
  StartRecordingPeriodicMetrics();

  // User becomes idle and metrics should be updated because of the timeout.
  SetDeviceIdleTime(kRegularIdleTimeout, /*from_time=*/base::TimeTicks::Now() +
                                             kPeriodicMetricsInterval);
  task_environment()->FastForwardBy(kPeriodicMetricsInterval);

  // Emulate kiosk session restart.
  RecordPreviousSessionMetrics();

  histogram_tester()->ExpectBucketCount(
      kKioskSessionRestartUserActivityHistogram, KioskUserActivity::kIdle, 1);
  histogram_tester()->ExpectTotalCount(
      kKioskSessionRestartUserActivityHistogram, 1);
}

}  // namespace ash
