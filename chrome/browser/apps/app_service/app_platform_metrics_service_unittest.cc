// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics_service.h"

#include <memory>
#include <utility>

#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace apps {

namespace {

constexpr char kStartTime[] = "1 Jan 2021 21:00";

void SetScreenOff(bool is_screen_off) {
  power_manager::ScreenIdleState screen_idle_state;
  screen_idle_state.set_off(is_screen_off);
  chromeos::FakePowerManagerClient::Get()->SendScreenIdleStateChanged(
      screen_idle_state);
}

void SetSuspendImminent() {
  chromeos::FakePowerManagerClient::Get()->SendSuspendImminent(
      power_manager::SuspendImminent_Reason_OTHER);
}

apps::mojom::AppPtr MakeApp(const char* app_id, apps::mojom::AppType app_type) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->app_type = app_type;
  return app;
}

}  // namespace

// Tests for family user metrics service.
class AppPlatformMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);

    chromeos::PowerManagerClient::InitializeFake();
    app_platform_metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(&testing_profile_);

    app_platform_metrics_service_->Start(
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_)
            ->AppRegistryCache());

    InstallApps();
  }

  void TearDown() override {
    app_platform_metrics_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
  }

  void InstallApps() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_);
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(/*app_id=*/"u", apps::mojom::AppType::kUnknown));
    deltas.push_back(MakeApp(/*app_id=*/"a", apps::mojom::AppType::kArc));
    deltas.push_back(MakeApp(/*app_id=*/"bu", apps::mojom::AppType::kBuiltIn));
    deltas.push_back(MakeApp(/*app_id=*/"c", apps::mojom::AppType::kCrostini));
    deltas.push_back(MakeApp(/*app_id=*/"w", apps::mojom::AppType::kWeb));
    deltas.push_back(MakeApp(
        /*app_id=*/"m", apps::mojom::AppType::kMacOs));
    deltas.push_back(MakeApp(
        /*app_id=*/"p", apps::mojom::AppType::kPluginVm));
    deltas.push_back(MakeApp(
        /*app_id=*/"l", apps::mojom::AppType::kStandaloneBrowser));
    deltas.push_back(MakeApp(
        /*app_id=*/"r", apps::mojom::AppType::kRemote));
    deltas.push_back(MakeApp(/*app_id=*/"bo", apps::mojom::AppType::kBorealis));
    deltas.push_back(MakeApp(/*app_id=*/"s", apps::mojom::AppType::kSystemWeb));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void InstallOneApp() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_);
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(/*app_id=*/"aa", apps::mojom::AppType::kArc));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void VerifyMetrics() {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBuiltIn),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kCrostini),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kChromeApp),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kWeb),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kMacOs),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kPluginVm),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kRemote),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBorealis),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kSystemWeb),
        /*expected_count=*/1);
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return testing_profile_.GetTestingPrefService();
  }

  int GetDayIdPref() {
    return GetPrefService()->GetInteger(kAppPlatformMetricsDayId);
  }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  TestingProfile testing_profile_;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<AppPlatformMetricsService> app_platform_metrics_service_;
};

// Tests OnNewDay() is called after more than one day passes.
TEST_F(AppPlatformMetricsServiceTest, MoreThanOneDay) {
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1) +
                                  base::TimeDelta::FromHours(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(AppPlatformMetricsServiceTest, UntilMidnight) {
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(3));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(AppPlatformMetricsServiceTest, LessThanOneDay) {
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*expected_count=*/0);
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called after one day passes, even when the device is
// idle.
TEST_F(AppPlatformMetricsServiceTest, MoreThanOneDayDeviceIdle) {
  SetScreenOff(true);
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests the UMA metrics that count the number of installed apps.
TEST_F(AppPlatformMetricsServiceTest, InstallApps) {
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(3));
  VerifyMetrics();

  InstallOneApp();
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*expected_count=*/2);
}

}  // namespace apps
