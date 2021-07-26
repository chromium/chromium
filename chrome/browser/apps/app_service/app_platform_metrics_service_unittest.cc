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
#include "chrome/browser/ash/login/users/fake_chrome_user_manager.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/sync/sync_service_factory.h"
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
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/common/constants.h"
#include "services/metrics/public/cpp/ukm_source.h"
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
                            const std::string& publisher_id,
                            apps::mojom::Readiness readiness,
                            apps::mojom::InstallSource install_source) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->app_type = app_type;
  app->publisher_id = publisher_id;
  app->readiness = readiness;
  app->install_source = install_source;
  return app;
}

std::unique_ptr<KeyedService> TestingSyncFactoryFunction(
    content::BrowserContext* context) {
  return std::make_unique<syncer::TestSyncService>();
}

}  // namespace

// Tests for app platform metrics service.
class AppPlatformMetricsServiceTest : public testing::Test {
 public:
  void SetUp() override {
    AddRegularUser("user@test.com");

    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();

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
        std::make_unique<AppPlatformMetricsService>(testing_profile_.get());

    app_platform_metrics_service_->Start(
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
            ->AppRegistryCache(),
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
            ->InstanceRegistry());

    InstallApps();
  }

  void TearDown() override {
    app_platform_metrics_service_.reset();
    chromeos::PowerManagerClient::Shutdown();
    browser_window1_.reset();
    browser_window2_.reset();
  }

  void AddRegularUser(const std::string& email) {
    fake_user_manager_ = new ash::FakeChromeUserManager;
    scoped_user_manager_ = std::make_unique<user_manager::ScopedUserManager>(
        base::WrapUnique(fake_user_manager_));

    AccountId account_id = AccountId::FromUserEmail(email);
    const user_manager::User* user = fake_user_manager_->AddUser(account_id);
    fake_user_manager_->UserLoggedIn(account_id, user->username_hash(),
                                     /*browser_restart=*/false,
                                     /*is_child=*/false);
    fake_user_manager_->SimulateUserProfileLoad(account_id);

    TestingProfile::Builder builder;
    builder.AddTestingFactory(SyncServiceFactory::GetInstance(),
                              SyncServiceFactory::GetDefaultFactory());
    testing_profile_ = builder.Build();

    chromeos::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            testing_profile_.get(),
            base::BindRepeating(&TestingSyncFactoryFunction)));
    sync_service_->SetFirstSetupComplete(true);
  }

  void InstallApps() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get());
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();

    deltas.push_back(MakeApp(/*app_id=*/"a", apps::mojom::AppType::kArc,
                             "com.google.A", apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kUser));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kArc,
                 true /* should_notify_initialized */);
    deltas.clear();

    deltas.push_back(MakeApp(/*app_id=*/"bu", apps::mojom::AppType::kBuiltIn,
                             "", apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kSystem));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kBuiltIn,
                 true /* should_notify_initialized */);
    deltas.clear();

    deltas.push_back(MakeApp(/*app_id=*/"c", apps::mojom::AppType::kCrostini,
                             "", apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kUser));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kCrostini,
                 true /* should_notify_initialized */);
    deltas.clear();

    deltas.push_back(MakeApp(/*app_id=*/"w", apps::mojom::AppType::kWeb,
                             "https://foo.com", apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kSync));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kWeb,
                 true /* should_notify_initialized */);
    deltas.clear();

    deltas.push_back(MakeApp(
        /*app_id=*/"s", apps::mojom::AppType::kSystemWeb, "https://os-settings",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kDefault));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kWeb,
                 true /* should_notify_initialized */);
    deltas.clear();

    deltas.push_back(MakeApp(/*app_id=*/"u", apps::mojom::AppType::kUnknown, "",
                             apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"m", apps::mojom::AppType::kMacOs, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"p", apps::mojom::AppType::kPluginVm, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kUser));
    deltas.push_back(MakeApp(
        /*app_id=*/"l", apps::mojom::AppType::kStandaloneBrowser, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kSystem));
    deltas.push_back(MakeApp(
        /*app_id=*/"lcr", apps::mojom::AppType::kStandaloneBrowserExtension, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kUser));
    deltas.push_back(MakeApp(
        /*app_id=*/"r", apps::mojom::AppType::kRemote, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallSource::kPolicy));
    deltas.push_back(MakeApp(/*app_id=*/"bo", apps::mojom::AppType::kBorealis,
                             "", apps::mojom::Readiness::kReady,
                             apps::mojom::InstallSource::kOem));
    cache.OnApps(std::move(deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void InstallOneApp(const std::string& app_id,
                     apps::mojom::AppType app_type,
                     const std::string& publisher_id,
                     apps::mojom::Readiness readiness) {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get());
    std::vector<apps::mojom::AppPtr> deltas;
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    deltas.push_back(MakeApp(app_id.c_str(), app_type, publisher_id, readiness,
                             apps::mojom::InstallSource::kUser));
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
            AppTypeName::kStandaloneBrowserExtension),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kStandaloneBrowserExtension,
            apps::mojom::InstallSource::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowserExtension),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallSourceHistogramNameForTest(
            AppTypeName::kStandaloneBrowserExtension,
            apps::mojom::InstallSource::kUser),
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

    apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
        ->InstanceRegistry()
        .OnInstances(deltas);
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow1() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        &delegate1_, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(testing_profile_.get(), true);
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
    Browser::CreateParams params(testing_profile_.get(), true);
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

  void VerifyAppUsageTimeUkm(const std::string& app_id,
                             int duration,
                             AppTypeName app_type_name) {
    const std::string kUrl = std::string("app://") + app_id;
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
    int count = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != GURL(kUrl)) {
        continue;
      }
      ++count;
      test_ukm_recorder()->ExpectEntryMetric(entry, "UserDeviceMatrix", 0);
      test_ukm_recorder()->ExpectEntryMetric(entry, "Duration", duration);
      test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                             (int)app_type_name);
    }
    ASSERT_EQ(1, count);
  }

  void VerifyInstalledAppsUkm(const std::string& app_info,
                              AppTypeName app_type_name,
                              apps::mojom::InstallSource install_source,
                              InstallTime install_time) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InstalledApp");
    int count = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != GURL(app_info)) {
        continue;
      }
      ++count;
      test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                             (int)app_type_name);
      test_ukm_recorder()->ExpectEntryMetric(entry, "InstallSource",
                                             (int)install_source);
      test_ukm_recorder()->ExpectEntryMetric(entry, "InstallTime",
                                             (int)install_time);
    }
    ASSERT_EQ(1, count);
  }

  void VerifyAppsLaunchUkm(const std::string& app_info,
                           AppTypeName app_type_name,
                           apps::mojom::LaunchSource launch_source) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.Launch");
    int count = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != GURL(app_info)) {
        continue;
      }
      ++count;
      test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                             (int)app_type_name);
      test_ukm_recorder()->ExpectEntryMetric(entry, "LaunchSource",
                                             (int)launch_source);
    }
    ASSERT_EQ(1, count);
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

 protected:
  sync_preferences::TestingPrefServiceSyncable* GetPrefService() {
    return testing_profile_->GetTestingPrefService();
  }

  int GetDayIdPref() {
    return GetPrefService()->GetInteger(kAppPlatformMetricsDayId);
  }

  std::unique_ptr<AppPlatformMetricsService> GetAppPlatformMetricsService() {
    return std::move(app_platform_metrics_service_);
  }

  TestingProfile* profile() { return testing_profile_.get(); }

  syncer::TestSyncService* sync_service() { return sync_service_; }

  base::HistogramTester& histogram_tester() { return histogram_tester_; }

  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};

 private:
  std::unique_ptr<TestingProfile> testing_profile_;
  syncer::TestSyncService* sync_service_ = nullptr;
  base::HistogramTester histogram_tester_;
  std::unique_ptr<AppPlatformMetricsService> app_platform_metrics_service_;
  std::unique_ptr<TestBrowserWindowAura> browser_window1_;
  std::unique_ptr<TestBrowserWindowAura> browser_window2_;
  aura::test::TestWindowDelegate delegate1_;
  aura::test::TestWindowDelegate delegate2_;
  ash::FakeChromeUserManager* fake_user_manager_ = nullptr;
  std::unique_ptr<user_manager::ScopedUserManager> scoped_user_manager_;
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
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

  InstallOneApp("aa", apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);
  task_environment_.FastForwardBy(base::TimeDelta::FromDays(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*expected_count=*/2);
}

TEST_F(AppPlatformMetricsServiceTest, BrowserWindow) {
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension,
                "Chrome", apps::mojom::Readiness::kReady);

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
  InstallOneApp(app_id, apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);

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
  InstallOneApp(app_id, apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);

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
  InstallOneApp(app_id, apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);

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
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension,
                "Chrome", apps::mojom::Readiness::kReady);
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
  InstallOneApp(app_id, apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);

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
  InstallOneApp(app_id, apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);
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
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension,
                "Chrome", apps::mojom::Readiness::kReady);
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
  VerifyAppUsageTimeUkm(extension_misc::kChromeAppId, /*duration=*/180000,
                        AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(15));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/4,
                                   AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeHistogram(base::TimeDelta::FromMinutes(5),
                              /*expected_count=*/3,
                              AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTimeUkm) {
  // Create a browser window.
  InstallOneApp(extension_misc::kChromeAppId, apps::mojom::AppType::kExtension,
                "Chrome", apps::mojom::Readiness::kReady);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(extension_misc::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  // Set sync is not allowed.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);

  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));

  // Verify UKM is not reported.
  const auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(0U, entries.size());

  // Set sync is allowed by setting an empty disable reason set.
  sync_service()->SetDisableReasons(syncer::SyncService::DisableReasonSet());
  task_environment_.FastForwardBy(base::TimeDelta::FromMinutes(5));
  VerifyAppUsageTimeUkm(extension_misc::kChromeAppId, /*duration=*/600000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, InstalledAppsUkm) {
  // Verify the apps installed during the init phase.
  VerifyInstalledAppsUkm("app://com.google.A", AppTypeName::kArc,
                         apps::mojom::InstallSource::kUser, InstallTime::kInit);
  VerifyInstalledAppsUkm("app://bu", AppTypeName::kBuiltIn,
                         apps::mojom::InstallSource::kSystem,
                         InstallTime::kInit);
  VerifyInstalledAppsUkm("https://os-settings", AppTypeName::kSystemWeb,
                         apps::mojom::InstallSource::kDefault,
                         InstallTime::kInit);
  VerifyInstalledAppsUkm("https://foo.com", AppTypeName::kChromeBrowser,
                         apps::mojom::InstallSource::kSync, InstallTime::kInit);

  // Install a new ARC app during the running time.
  InstallOneApp("aa", apps::mojom::AppType::kArc, "com.google.AA",
                apps::mojom::Readiness::kReady);

  // Verify the ARC app installed during the running time.
  VerifyInstalledAppsUkm("app://com.google.AA", AppTypeName::kArc,
                         apps::mojom::InstallSource::kUser,
                         InstallTime::kRunning);
}

TEST_F(AppPlatformMetricsServiceTest, LaunchAppsUkm) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  proxy->Launch(
      /*app_id=*/"c", ui::EventFlags::EF_NONE,
      apps::mojom::LaunchSource::kFromChromeInternal, nullptr);
  // Verify UKM is not reported for the Crostini app.
  const auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.Launch");
  ASSERT_EQ(0U, entries.size());

  proxy->Launch(
      /*app_id=*/"a", ui::EventFlags::EF_NONE,
      apps::mojom::LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://com.google.A", AppTypeName::kArc,
                      apps::mojom::LaunchSource::kFromChromeInternal);

  proxy->LaunchAppWithUrl(
      /*app_id=*/"w", ui::EventFlags::EF_NONE, GURL("https://boo.com/a"),
      apps::mojom::LaunchSource::kFromFileManager, nullptr);
  VerifyAppsLaunchUkm("https://foo.com", AppTypeName::kChromeBrowser,
                      apps::mojom::LaunchSource::kFromFileManager);

  proxy->BrowserAppLauncher()->LaunchAppWithParams(apps::AppLaunchParams(
      "s", apps::mojom::LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB,
      apps::mojom::AppLaunchSource::kSourceTest));
  VerifyAppsLaunchUkm("https://os-settings", AppTypeName::kChromeBrowser,
                      apps::mojom::LaunchSource::kFromTest);
}

}  // namespace apps
