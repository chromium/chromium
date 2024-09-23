// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/app_list_feature_usage_metrics.h"

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/test/ash_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/command_line.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chromeos/ash/components/feature_usage/feature_usage_metrics.h"
#include "components/user_manager/user_type.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

// Expanded metric names recorded by FeatureUsageMetrics.
constexpr char kClamshellMetric[] = "ChromeOS.FeatureUsage.ClamshellLauncher";
constexpr char kClamshellUsetimeMetric[] =
    "ChromeOS.FeatureUsage.ClamshellLauncher.Usetime";
constexpr char kTabletMetric[] = "ChromeOS.FeatureUsage.TabletLauncher";
constexpr char kTabletUsetimeMetric[] =
    "ChromeOS.FeatureUsage.TabletLauncher.Usetime";

// Shorten these identifiers for readability.
constexpr int kEligible =
    static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEligible);
constexpr int kEnabled =
    static_cast<int>(feature_usage::FeatureUsageMetrics::Event::kEnabled);
constexpr int kUsedWithSuccess = static_cast<int>(
    feature_usage::FeatureUsageMetrics::Event::kUsedWithSuccess);

// Uses NoSessionAshTestBase because some tests need to simulate kiosk login.
class AppListFeatureUsageMetricsTest : public NoSessionAshTestBase {
 public:
  AppListFeatureUsageMetricsTest()
      : NoSessionAshTestBase(
            base::test::TaskEnvironment::TimeSource::MOCK_TIME) {}
  ~AppListFeatureUsageMetricsTest() override = default;

  // Simulates a device that supports tablet mode.
  void SimulateTabletModeSupport() {
    base::CommandLine::ForCurrentProcess()->AppendSwitch(
        switches::kAshEnableTabletMode);
    auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
    tablet_mode_controller->OnECLidAngleDriverStatusChanged(
        /*is_supported=*/true);
    tablet_mode_controller->OnDeviceListsComplete();
  }

  void FastForwardBy(base::TimeDelta delta) {
    task_environment()->FastForwardBy(delta);
  }

  // Fast forwards the clock past the time when FeatureUsageMetrics reports
  // its initial set of eligible/enabled metrics.
  void FastForwardPastMetricsReportingInterval() {
    FastForwardBy(feature_usage::FeatureUsageMetrics::kInitialInterval);
  }

  base::HistogramTester histograms_;
};

TEST_F(AppListFeatureUsageMetricsTest, CountsStartAtZero) {
  SimulateUserLogin("user@gmail.com");

  histograms_.ExpectTotalCount(kClamshellMetric, 0);
  histograms_.ExpectTotalCount(kClamshellUsetimeMetric, 0);
  histograms_.ExpectTotalCount(kTabletMetric, 0);
  histograms_.ExpectTotalCount(kTabletUsetimeMetric, 0);
}

TEST_F(AppListFeatureUsageMetricsTest, InitialMetricsWithoutTabletModeSupport) {
  ASSERT_FALSE(Shell::Get()->tablet_mode_controller()->CanEnterTabletMode());

  SimulateUserLogin("user@gmail.com");
  FastForwardPastMetricsReportingInterval();

  histograms_.ExpectBucketCount(kClamshellMetric, kEligible, 1);
  histograms_.ExpectBucketCount(kClamshellMetric, kEnabled, 1);
  // Not eligible for tablet mode.
  histograms_.ExpectBucketCount(kTabletMetric, kEligible, 0);
  histograms_.ExpectBucketCount(kTabletMetric, kEnabled, 0);
}

TEST_F(AppListFeatureUsageMetricsTest, InitialMetricsWithTabletModeSupport) {
  SimulateTabletModeSupport();
  ASSERT_TRUE(Shell::Get()->tablet_mode_controller()->CanEnterTabletMode());

  SimulateUserLogin("user@gmail.com");
  FastForwardPastMetricsReportingInterval();

  histograms_.ExpectBucketCount(kClamshellMetric, kEligible, 1);
  histograms_.ExpectBucketCount(kClamshellMetric, kEnabled, 1);
  // Eligible for tablet mode.
  histograms_.ExpectBucketCount(kTabletMetric, kEligible, 1);
  histograms_.ExpectBucketCount(kTabletMetric, kEnabled, 1);
}

TEST_F(AppListFeatureUsageMetricsTest, NotEligibleInKioskMode) {
  SimulateKioskMode(user_manager::UserType::kKioskApp);
  FastForwardPastMetricsReportingInterval();

  histograms_.ExpectBucketCount(kClamshellMetric, kEligible, 0);
  histograms_.ExpectBucketCount(kClamshellMetric, kEnabled, 0);
  histograms_.ExpectBucketCount(kTabletMetric, kEligible, 0);
  histograms_.ExpectBucketCount(kTabletMetric, kEnabled, 0);
}

TEST_F(AppListFeatureUsageMetricsTest, ShowAndHideLauncherInClamshell) {
  SimulateUserLogin("user@gmail.com");
  Shell::Get()->app_list_controller()->ShowAppList(
      AppListShowSource::kSearchKey);
  histograms_.ExpectBucketCount(kClamshellMetric, kUsedWithSuccess, 1);

  const base::TimeDelta kUsetime = base::Seconds(2);
  FastForwardBy(kUsetime);
  Shell::Get()->app_list_controller()->DismissAppList();
  histograms_.ExpectTimeBucketCount(kClamshellUsetimeMetric, kUsetime, 1);

  // Tablet usage is not recorded.
  histograms_.ExpectTotalCount(kTabletUsetimeMetric, 0);
}

TEST_F(AppListFeatureUsageMetricsTest, ShowAndHideLauncherInTablet) {
  SimulateTabletModeSupport();
  ASSERT_TRUE(Shell::Get()->tablet_mode_controller()->CanEnterTabletMode());
  SimulateUserLogin("user@gmail.com");

  // Entering tablet mode shows the home screen launcher.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  histograms_.ExpectBucketCount(kTabletMetric, kUsedWithSuccess, 1);

  const base::TimeDelta kUsetime = base::Seconds(2);
  FastForwardBy(kUsetime);
  // Creating a window hides the launcher.
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  histograms_.ExpectTimeBucketCount(kTabletUsetimeMetric, kUsetime, 1);

  // Clamshell usage is not recorded.
  histograms_.ExpectTotalCount(kClamshellUsetimeMetric, 0);
}

TEST_F(AppListFeatureUsageMetricsTest,
       EnterTabletModeWithLauncherAndWindowOpen) {
  SimulateTabletModeSupport();
  ASSERT_TRUE(Shell::Get()->tablet_mode_controller()->CanEnterTabletMode());
  SimulateUserLogin("user@gmail.com");
  std::unique_ptr<views::Widget> widget =
      CreateTestWidget(views::Widget::InitParams::WIDGET_OWNS_NATIVE_WIDGET);
  Shell::Get()->app_list_controller()->ShowAppList(
      AppListShowSource::kSearchKey);

  // Entering tablet mode with a window open does not show the launcher.
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  FastForwardBy(base::Seconds(1));
  histograms_.ExpectTotalCount(kTabletUsetimeMetric, 0);

  // Show the tablet launcher for 1 second.
  Shell::Get()->app_list_controller()->GoHome(GetPrimaryDisplay().id());
  FastForwardBy(base::Seconds(1));
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);

  // Only 1 second of usage is recorded.
  histograms_.ExpectTimeBucketCount(kTabletUsetimeMetric, base::Seconds(1), 1);
}

TEST_F(AppListFeatureUsageMetricsTest, OpenClamshellThenTabletThenExit) {
  SimulateTabletModeSupport();
  SimulateUserLogin("user@gmail.com");

  Shell::Get()->app_list_controller()->ShowAppList(
      AppListShowSource::kSearchKey);
  histograms_.ExpectBucketCount(kClamshellMetric, kUsedWithSuccess, 1);

  // Switching from clamshell to tablet with the launcher open records usage
  // time for clamshell launcher and starts a tablet launcher usage session.
  const base::TimeDelta kClamshellUsetime = base::Seconds(1);
  FastForwardBy(kClamshellUsetime);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(true);
  histograms_.ExpectTimeBucketCount(kClamshellUsetimeMetric, kClamshellUsetime,
                                    1);
  histograms_.ExpectBucketCount(kTabletMetric, kUsedWithSuccess, 1);

  // Ending tablet mode records usage time for tablet launcher.
  const base::TimeDelta kTabletUsetime = base::Seconds(2);
  FastForwardBy(kTabletUsetime);
  Shell::Get()->tablet_mode_controller()->SetEnabledForTest(false);
  histograms_.ExpectTimeBucketCount(kTabletUsetimeMetric, kTabletUsetime, 1);
}

}  // namespace
}  // namespace ash
