// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_mode/metrics/low_disk_metrics_service.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/scoped_testing_local_state.h"
#include "chrome/test/base/testing_browser_process.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/prefs/testing_pref_service.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {

class LowDiskMetricsServiceTest
    : public testing::TestWithParam<
          std::tuple<KioskLowDiskSeverity, uint64_t>> {
 public:
  LowDiskMetricsServiceTest()
      : local_state_(std::make_unique<ScopedTestingLocalState>(
            TestingBrowserProcess::GetGlobal())) {
    UserDataAuthClient::InitializeFake();
  }

  LowDiskMetricsServiceTest(const LowDiskMetricsServiceTest&) = delete;
  LowDiskMetricsServiceTest& operator=(const LowDiskMetricsServiceTest&) =
      delete;

  KioskLowDiskSeverity severity() const { return std::get<0>(GetParam()); }
  uint64_t disk_free_bytes() const { return std::get<1>(GetParam()); }

  TestingPrefServiceSimple* local_state() { return local_state_->Get(); }

  void SetUp() override {
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }

  void TearDown() override {
    local_state()->RemoveUserPref(prefs::kKioskMetrics);
  }
  base::HistogramTester* histogram_tester() { return histogram_tester_.get(); }

  std::optional<KioskLowDiskSeverity> GetLowDiskSeverityFromLocalState() {
    const auto& metrics_dict = local_state()->GetDict(prefs::kKioskMetrics);
    const auto severity_value = metrics_dict.FindInt(kKioskLowDiskSeverity);
    if (!severity_value) {
      return std::nullopt;
    }

    return static_cast<KioskLowDiskSeverity>(severity_value.value());
  }

  void SendLowDiskSpaceEvent(uint64_t disk_free_bytes) {
    FakeUserDataAuthClient::Get()->NotifyLowDiskSpace(disk_free_bytes);
    task_environment_.RunUntilIdle();
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<ScopedTestingLocalState> local_state_;
};

TEST_P(LowDiskMetricsServiceTest, Severity) {
  auto service = LowDiskMetricsService::CreateForTesting(local_state());

  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       0);
  histogram_tester()->ExpectTotalCount(
      kKioskSessionLowDiskHighestSeverityHistogram, 0);

  SendLowDiskSpaceEvent(disk_free_bytes());
  histogram_tester()->ExpectBucketCount(kKioskSessionLowDiskSeverityHistogram,
                                        severity(), 1);
  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       1);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(), severity());

  service = LowDiskMetricsService::CreateForTesting(local_state());
  histogram_tester()->ExpectBucketCount(
      kKioskSessionLowDiskHighestSeverityHistogram, severity(), 1);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(),
            KioskLowDiskSeverity::kNone);
}

TEST_F(LowDiskMetricsServiceTest, Update) {
  auto service = LowDiskMetricsService::CreateForTesting(local_state());

  SendLowDiskSpaceEvent(kLowDiskMediumThreshold + 1);
  histogram_tester()->ExpectBucketCount(kKioskSessionLowDiskSeverityHistogram,
                                        KioskLowDiskSeverity::kNone, 1);
  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       1);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(),
            KioskLowDiskSeverity::kNone);

  // Update to higher severity.
  SendLowDiskSpaceEvent(kLowDiskMediumThreshold - 1);
  histogram_tester()->ExpectBucketCount(kKioskSessionLowDiskSeverityHistogram,
                                        KioskLowDiskSeverity::kMedium, 1);
  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       2);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(),
            KioskLowDiskSeverity::kMedium);

  // Update to higher severity.
  SendLowDiskSpaceEvent(kLowDiskSevereThreshold - 1);
  histogram_tester()->ExpectBucketCount(kKioskSessionLowDiskSeverityHistogram,
                                        KioskLowDiskSeverity::kHigh, 1);
  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       3);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(),
            KioskLowDiskSeverity::kHigh);

  // Do not update to higher severity.
  SendLowDiskSpaceEvent(kLowDiskSevereThreshold + 1);
  histogram_tester()->ExpectBucketCount(kKioskSessionLowDiskSeverityHistogram,
                                        KioskLowDiskSeverity::kMedium, 2);
  histogram_tester()->ExpectTotalCount(kKioskSessionLowDiskSeverityHistogram,
                                       4);
  EXPECT_EQ(GetLowDiskSeverityFromLocalState().value(),
            KioskLowDiskSeverity::kHigh);
}

INSTANTIATE_TEST_SUITE_P(
    LowDiskMetricsServiceTest,
    LowDiskMetricsServiceTest,
    testing::Values(
        std::tuple(KioskLowDiskSeverity::kNone, kLowDiskMediumThreshold + 1),
        std::tuple(KioskLowDiskSeverity::kMedium, kLowDiskMediumThreshold - 1),
        std::tuple(KioskLowDiskSeverity::kHigh, kLowDiskSevereThreshold - 1)));

}  // namespace ash
