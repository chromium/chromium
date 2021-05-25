// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_platform_metrics_service.h"

#include <memory>
#include <utility>

#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/util/values/values_util.h"
#include "chrome/browser/apps/app_service/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"

namespace apps {

namespace {

constexpr char kStartTime[] = "1 Jan 2021 21:00";

constexpr apps::InstanceState kActiveInstanceState =
    static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
constexpr apps::InstanceState kInactiveInstanceState =
    static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                     apps::InstanceState::kRunning);

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

apps::mojom::AppPtr MakeApp(const char* app_id,
                            apps::mojom::AppType app_type,
                            apps::mojom::InstallSource install_source) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->app_type = app_type;
  app->install_source = install_source;
  return app;
}

}  // namespace

// Tests for family user metrics service.
class AppPlatformMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    base::Time start_time;
    EXPECT_TRUE(base::Time::FromUTCString(kStartTime, &start_time));
    base::TimeDelta forward_by = start_time - base::Time::Now();
    EXPECT_LT(base::TimeDelta(), forward_by);
    task_environment_.AdvanceClock(forward_by);
    GetPrefService()->SetInteger(
        kAppPlatformMetricsDayId,
        start_time.UTCMidnight().since_origin().InDaysFloored());

    chromeos::PowerManagerClient::InitializeFake();
    app_platform_metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(&testing_profile_);

    app_platform_metrics_service_->Start(
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_)
            ->AppRegistryCache(),
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_)
            ->InstanceRegistry());

    InstallApps();
  }

  void TearDown() override {
    app_platform_metrics_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
    browser_window1_.reset();
    browser_window2_.reset();
  }

  void InstallApps() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_);
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(/*app_id=*/"u", apps::mojom::AppType::kUnknown,
                             apps::mojom::InstallSource::kUnknown));
    deltas.push_back(MakeApp(/*app_id=*/"a", apps::mojom::AppType::kArc,
                             apps::mojom::InstallSource::kUser));
    deltas.push_back(MakeApp(/*app_id=*/"bu", apps::mojom::AppType::kBuiltIn,
                             apps::mojom::InstallSource::kSystem));
    deltas.push_back(MakeApp(/*app_id=*/"c", apps::mojom::AppType::kCrostini,
                             apps::mojom::InstallSource::kUser));
    deltas.push_back(MakeApp(/*app_id=*/"w", apps::mojom::AppType::kWeb,
                             apps::mojom::InstallSource::kSync));
    deltas.push_back(MakeApp(
        /*app_id=*/"m", apps::mojom::AppType::kMacOs,
        apps::mojom::InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"p", apps::mojom::AppType::kPluginVm,
        apps::mojom::InstallSource::kUser));
    deltas.push_back(MakeApp(
        /*app_id=*/"l", apps::mojom::AppType::kStandaloneBrowser,
        apps::mojom::InstallSource::kSystem));
    deltas.push_back(MakeApp(
        /*app_id=*/"r", apps::mojom::AppType::kRemote,
        apps::mojom::InstallSource::kPolicy));
    deltas.push_back(MakeApp(/*app_id=*/"bo", apps::mojom::AppType::kBorealis,
                             apps::mojom::InstallSource::kOem));
    deltas.push_back(MakeApp(/*app_id=*/"s", apps::mojom::AppType::kSystemWeb,
                             apps::mojom::InstallSource::kDefault));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void InstallOneApp(const std::string& app_id, apps::mojom::AppType app_type) {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(&testing_profile_);
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(
        MakeApp(app_id.c_str(), app_type, apps::mojom::InstallSource::kUser));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void VerifyMetrics() {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kArc, apps::mojom::InstallSource::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBuiltIn),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kBuiltIn, apps::mojom::InstallSource::kSystem),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kCrostini),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kCrostini, apps::mojom::InstallSource::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kChromeApp),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kWeb),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kWeb, apps::mojom::InstallSource::kSync),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kMacOs),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kMacOs, apps::mojom::InstallSource::kUnknown),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kPluginVm),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kPluginVm, apps::mojom::InstallSource::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kStandaloneBrowser,
            apps::mojom::InstallSource::kSystem),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kRemote),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kRemote, apps::mojom::InstallSource::kPolicy),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBorealis),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kBorealis, apps::mojom::InstallSource::kOem),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kSystemWeb),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kSystemWeb, apps::mojom::InstallSource::kDefault),
        /*expected_count=*/1);
  }

  void ModifyInstance(const std::string& app_id,
                      aura::Window* window,
                      apps::InstanceState state) {
    std::unique_ptr<apps::Instance> instance = std::make_unique<apps::Instance>(
        app_id, std::make_unique<apps::Instance::InstanceKey>(window));
    instance->UpdateState(state, base::Time::Now());

    std::vector<std::unique_ptr<apps::Instance>> deltas;
    deltas.push_back(std::move(instance));

    apps::AppServiceProxyFactory::GetForProfile(&testing_profile_)
        ->InstanceRegistry()
        .OnInstances(deltas);
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow1() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        &delegate1_, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(&testing_profile_, true);
    params.type = Browser::TYPE_NORMAL;
    browser_window1_ =
        std::make_unique<TestBrowserWindowAura>(std::move(window));
    params.window = browser_window1_.get();
    return std::unique_ptr<Browser>(Browser::Create(params));
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow2() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        &delegate2_, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(&testing_profile_, true);
    params.type = Browser::TYPE_NORMAL;
    browser_window2_ =
        std::make_unique<TestBrowserWindowAura>(std::move(window));
    params.window = browser_window2_.get();
    return std::unique_ptr<Browser>(Browser::Create(params));
  }

  void VerifyAppRunningDuration(const base::TimeDelta time_delta,
                                AppTypeName app_type_name) {
    DictionaryPrefUpdate update(GetPrefService(), kAppRunningDuration);
    std::string key = GetAppTypeHistogramName(app_type_name);

    absl::optional<base::TimeDelta> unreported_duration =
        util::ValueToTimeDelta(update->FindPath(key));
    if (time_delta.is_zero()) {
      EXPECT_FALSE(unreported_duration.has_value());
      return;
    }

    ASSERT_TRUE(unreported_duration.has_value());
    EXPECT_EQ(time_delta, unreported_duration.value());
  }

  void VerifyAppRunningDurationCountHistogram(base::HistogramBase::Count count,
                                              AppTypeName app_type_name) {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        count);
  }

  void VerifyAppRunningDurationHistogram(base::TimeDelta time_delta,
                                         base::HistogramBase::Count count,
                                         AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        time_delta, count);
  }

  void VerifyAppRunningPercentageCountHistogram(
      base::HistogramBase::Count count,
      AppTypeName app_type_name) {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
            app_type_name),
        count);
  }

  void VerifyAppRunningPercentageHistogram(
      int count,
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(
        AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
            app_type_name),
        count, expected_count);
  }

  void VerifyAppActivatedCount(int count, AppTypeName app_type_name) {
    DictionaryPrefUpdate update(GetPrefService(), kAppActivatedCount);
    std::string key = GetAppTypeHistogramName(app_type_name);

    absl::optional<int> activated_count = update->FindIntPath(key);
    if (count == 0) {
      EXPECT_FALSE(activated_count.has_value());
      return;
    }

    ASSERT_TRUE(activated_count.has_value());
    EXPECT_EQ(count, activated_count.value());
  }

  void VerifyAppActivatedCountHistogram(base::HistogramBase::Count count,
                                        AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        count);
  }

  void VerifyAppActivatedHistogram(int count,
                                   base::HistogramBase::Count expected_count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        count, expected_count);
  }

  void VerifyAppUsageTimeCountHistogram(base::HistogramBase::Count count,
                                        AppTypeName app_type_name) {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        count);
  }

  void VerifyAppUsageTimeHistogram(base::TimeDelta time_delta,
                                   base::HistogramBase::Count count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        time_delta, count);
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
  std::unique_ptr<TestBrowserWindowAura> browser_window1_;
  std::unique_ptr<TestBrowserWindowAura> browser_window2_;
  aura::test::TestWindowDelegate delegate1_;
  aura::test::TestWindowDelegate delegate2_;
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

  InstallOneApp("aa", apps::mojom::AppType::kArc);
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*expected_count=*/2);
}

TEST_F(AppPlatformMetricsServiceTest, BrowserWindow) {
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension);

  BrowserList* active_browser_list = BrowserList::GetInstance();
  // Expect BrowserList is empty at the beginning.
  EXPECT_EQ(0U, active_browser_list->size());
  std::unique_ptr<Browser> browser1 = CreateBrowserWithAuraWindow1();

  EXPECT_EQ(1U, active_browser_list->size());

  // Set the browser window active.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(20));
  // Set the browser window running in the background.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kInactiveInstanceState);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppRunningDuration(base::TimeDelta::FromMinutes(30),
                           AppTypeName::kChromeBrowser);

  // Test multiple browsers.
  std::unique_ptr<Browser> browser2 = CreateBrowserWithAuraWindow2();
  EXPECT_EQ(2U, active_browser_list->size());

  ModifyInstance(extension_misc::kChromeAppId,
                 browser2->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(20));
  ModifyInstance(extension_misc::kChromeAppId,
                 browser2->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppRunningDuration(base::TimeDelta::FromHours(1),
                           AppTypeName::kChromeBrowser);

  // Test date change.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kChromeBrowser);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(1),
                                    /*expected_count=*/1,
                                    AppTypeName::kChromeBrowser);
  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kChromeBrowser);
  VerifyAppRunningPercentageHistogram(100,
                                      /*expected_count=*/1,
                                      AppTypeName::kChromeBrowser);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1,
                                   AppTypeName::kChromeBrowser);
  VerifyAppActivatedHistogram(/*count*/ 2, /*expected_count=*/1,
                              AppTypeName::kChromeBrowser);
}

// Tests the UMA metrics when launching an app in one day .
TEST_F(AppPlatformMetricsServiceTest, OpenWindowInOneDay) {
  std::string app_id = "aa";
  InstallOneApp(app_id, apps::mojom::AppType::kArc);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Close the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(50));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  VerifyAppRunningDuration(base::TimeDelta::FromHours(1), AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(1),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 1, /*expected_count=*/1,
                              AppTypeName::kArc);
  VerifyAppRunningDuration(base::TimeDelta(), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(1),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kArc);
  VerifyAppRunningPercentageHistogram(100,
                                      /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
}

// Tests the UMA metrics when launching an app multiple days.
TEST_F(AppPlatformMetricsServiceTest, OpenWindowInMultipleDays) {
  std::string app_id = "aa";
  InstallOneApp(app_id, apps::mojom::AppType::kArc);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 1, /*expected_count=*/1,
                              AppTypeName::kArc);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(2));

  // Close the window after running five hours.
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppRunningDuration(base::TimeDelta::FromHours(2), AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/2,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(2),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppRunningDuration(base::TimeDelta::FromHours(0), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);
}

// Tests the UMA metrics when an app window is reactivated.
TEST_F(AppPlatformMetricsServiceTest, ReactiveWindow) {
  std::string app_id = "aa";
  InstallOneApp(app_id, apps::mojom::AppType::kArc);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(30));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(30));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Activa the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kArc);

  // Close the window after running half hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(20));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppRunningDuration(
      base::TimeDelta::FromHours(1) + base::TimeDelta::FromMinutes(30),
      AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(20));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(
      base::TimeDelta::FromHours(1) + base::TimeDelta::FromMinutes(30),
      /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 2, /*expected_count=*/1,
                              AppTypeName::kArc);

  // 20 hours passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(20));

  // Create a new window.
  window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(50));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Activa the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/2,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 2, /*expected_count=*/2,
                              AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(3));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Close the window after running five hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(10));
  VerifyAppRunningDuration(base::TimeDelta::FromHours(3), AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/3,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::TimeDelta::FromHours(3),
                                    /*expected_count=*/2, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppRunningDuration(base::TimeDelta::FromHours(0), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);
}

// Tests the app running percentage UMA metrics when launch a browser window
// and an ARC app in one day.
TEST_F(AppPlatformMetricsServiceTest, AppRunningPercentrage) {
  // Launch a browser window.
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));

  // Set the browser window running in the background.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Launch an ARC app.
  std::string app_id = "aa";
  InstallOneApp(app_id, apps::mojom::AppType::kArc);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  // Close the window after running one hour.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  // One day passes.
  task_environment_.FastForwardBy(base::TimeDelta::FromHours(1));
  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kChromeBrowser);
  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kArc);
  VerifyAppRunningPercentageHistogram(50,
                                      /*expected_count=*/1,
                                      AppTypeName::kChromeBrowser);
  VerifyAppRunningPercentageHistogram(50,
                                      /*expected_count=*/1, AppTypeName::kArc);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTime) {
  // Create an ARC app window.
  std::string app_id = "aa";
  InstallOneApp(app_id, apps::mojom::AppType::kArc);
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppUsageTimeHistogram(base::TimeDelta::FromMinutes(5),
                              /*expected_count=*/1, AppTypeName::kArc);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(2));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Create a browser window
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(3));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppUsageTimeHistogram(base::TimeDelta::FromMinutes(2),
                              /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1,
                                   AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeHistogram(base::TimeDelta::FromMinutes(3),
                              /*expected_count=*/1,
                              AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(15));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/4,
                                   AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeHistogram(base::TimeDelta::FromMinutes(5),
                              /*expected_count=*/3,
                              AppTypeName::kChromeBrowser);
}

}  // namespace apps
