// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_base.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
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
#include "components/app_constants/constants.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "components/sync/driver/sync_service.h"
#include "components/sync/driver/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "components/user_manager/scoped_user_manager.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
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

apps::mojom::AppPtr MakeApp(const std::string& app_id,
                            apps::mojom::AppType app_type,
                            const std::string& publisher_id,
                            apps::mojom::Readiness readiness,
                            apps::mojom::InstallReason install_reason,
                            apps::mojom::InstallSource install_source) {
  apps::mojom::AppPtr app = apps::mojom::App::New();
  app->app_id = app_id;
  app->app_type = app_type;
  app->publisher_id = publisher_id;
  app->readiness = readiness;
  app->install_reason = install_reason;
  app->install_source = install_source;
  return app;
}

apps::AppPtr MakeApp(const std::string& app_id,
                     apps::AppType app_type,
                     const std::string& publisher_id,
                     apps::Readiness readiness,
                     apps::InstallReason install_reason,
                     apps::InstallSource install_source) {
  auto app = AppPublisher::MakeApp(app_type, app_id, readiness, publisher_id,
                                   install_reason, install_source);
  app->publisher_id = publisher_id;
  return app;
}

void AddMojomApp(apps::AppRegistryCache& cache,
                 const std::string& app_id,
                 apps::mojom::AppType app_type,
                 const std::string& publisher_id,
                 apps::mojom::Readiness readiness,
                 apps::mojom::InstallReason install_reason,
                 apps::mojom::InstallSource install_source,
                 bool should_notify_initialized) {
  std::vector<apps::mojom::AppPtr> deltas;
  deltas.push_back(MakeApp(app_id, app_type, publisher_id, readiness,
                           install_reason, install_source));
  cache.OnApps(std::move(deltas), app_type, should_notify_initialized);
}

void AddApp(apps::AppRegistryCache& cache,
            const std::string& app_id,
            apps::AppType app_type,
            const std::string& publisher_id,
            apps::Readiness readiness,
            apps::InstallReason install_reason,
            apps::InstallSource install_source,
            bool should_notify_initialized) {
  std::vector<apps::AppPtr> deltas;
  deltas.push_back(MakeApp(app_id, app_type, publisher_id, readiness,
                           install_reason, install_source));
  cache.OnApps(std::move(deltas), app_type, should_notify_initialized);

  AddMojomApp(cache, app_id, apps::ConvertAppTypeToMojomAppType(app_type),
              publisher_id, apps::ConvertReadinessToMojomReadiness(readiness),
              apps::ConvertInstallReasonToMojomInstallReason(install_reason),
              apps::ConvertInstallSourceToMojomInstallSource(install_source),
              should_notify_initialized);
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

    ash::ProfileHelper::Get()->SetUserToProfileMappingForTesting(
        user, testing_profile_.get());

    sync_service_ = static_cast<syncer::TestSyncService*>(
        SyncServiceFactory::GetInstance()->SetTestingFactoryAndUse(
            testing_profile_.get(),
            base::BindRepeating(&TestingSyncFactoryFunction)));
    sync_service_->SetFirstSetupComplete(true);
  }

  void ResetAppPlatformMetricsService() {
    app_platform_metrics_service_.reset();
    app_platform_metrics_service_ =
        std::make_unique<AppPlatformMetricsService>(testing_profile_.get());

    app_platform_metrics_service_->Start(
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
            ->AppRegistryCache(),
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
            ->InstanceRegistry());
  }

  void InstallApps() {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get());
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();

    AddApp(cache, /*app_id=*/"a", AppType::kArc, "com.google.A",
           Readiness::kReady, InstallReason::kUser, InstallSource::kPlayStore,
           true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/"bu", AppType::kBuiltIn, "", Readiness::kReady,
           InstallReason::kSystem, InstallSource::kSystem,
           true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/borealis::kClientAppId, AppType::kBorealis, "",
           Readiness::kReady, InstallReason::kUser, InstallSource::kUnknown,
           true /* should_notify_initialized */);

    borealis::CreateFakeApp(testing_profile_.get(), "borealistest",
                            "borealis/123");
    std::string borealis_app(borealis::FakeAppId("borealistest"));
    AddApp(cache, /*app_id=*/borealis_app.c_str(), AppType::kBorealis, "",
           Readiness::kReady, InstallReason::kUser, InstallSource::kUnknown,
           true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/crostini::kCrostiniTerminalSystemAppId,
           AppType::kCrostini, "", Readiness::kReady, InstallReason::kUser,
           InstallSource::kUnknown, true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/"w", AppType::kWeb, "https://foo.com",
           Readiness::kReady, InstallReason::kSync, InstallSource::kSync,
           false /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/"w2", AppType::kWeb, "https://foo2.com",
           Readiness::kReady, InstallReason::kSync, InstallSource::kSync,
           true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/"s", AppType::kSystemWeb, "https://os-settings",
           Readiness::kReady, InstallReason::kSystem, InstallSource::kSystem,
           true /* should_notify_initialized */);

    std::vector<AppPtr> deltas;
    deltas.push_back(MakeApp(/*app_id=*/"u", AppType::kUnknown, "",
                             Readiness::kReady, InstallReason::kUnknown,
                             InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"m", AppType::kMacOs, "", Readiness::kReady,
        InstallReason::kUnknown, InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"p", AppType::kPluginVm, "", Readiness::kReady,
        InstallReason::kUser, InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"l", AppType::kStandaloneBrowser, "", Readiness::kReady,
        InstallReason::kSystem, InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"lcr", AppType::kStandaloneBrowserChromeApp, "",
        Readiness::kReady, InstallReason::kUser,
        InstallSource::kChromeWebStore));
    deltas.push_back(MakeApp(
        /*app_id=*/"r", AppType::kRemote, "", Readiness::kReady,
        InstallReason::kPolicy, InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"subapp", AppType::kWeb, "", Readiness::kReady,
        InstallReason::kSubApp, InstallSource::kUnknown));
    cache.OnApps(std::move(deltas), AppType::kUnknown,
                 false /* should_notify_initialized */);

    std::vector<apps::mojom::AppPtr> mojom_deltas;
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"u", apps::mojom::AppType::kUnknown, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kUnknown,
        apps::mojom::InstallSource::kUnknown));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"m", apps::mojom::AppType::kMacOs, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kUnknown,
        apps::mojom::InstallSource::kUnknown));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"p", apps::mojom::AppType::kPluginVm, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kUser,
        apps::mojom::InstallSource::kUnknown));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"l", apps::mojom::AppType::kStandaloneBrowser, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kSystem,
        apps::mojom::InstallSource::kUnknown));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"lcr", apps::mojom::AppType::kStandaloneBrowserChromeApp, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kUser,
        apps::mojom::InstallSource::kChromeWebStore));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"r", apps::mojom::AppType::kRemote, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kPolicy,
        apps::mojom::InstallSource::kUnknown));
    mojom_deltas.push_back(MakeApp(
        /*app_id=*/"subapp", apps::mojom::AppType::kWeb, "",
        apps::mojom::Readiness::kReady, apps::mojom::InstallReason::kSubApp,
        apps::mojom::InstallSource::kUnknown));
    cache.OnApps(std::move(mojom_deltas), apps::mojom::AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void InstallOneApp(const std::string& app_id,
                     AppType app_type,
                     const std::string& publisher_id,
                     Readiness readiness,
                     InstallSource install_source) {
    auto* proxy =
        apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get());
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();
    AddApp(cache, app_id, app_type, publisher_id, readiness,
           InstallReason::kUser, install_source,
           false /* should_notify_initialized */);
  }

  void VerifyMetrics() {
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kArc, apps::InstallReason::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBuiltIn),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBuiltIn, apps::InstallReason::kSystem),
        /*expected_count=*/1);

    // Should be 3 Borealis apps: The installer/launcher created by the
    // BorealisApps class, plus the two created in this test.
    const int borealis_apps_count = 3;
    histogram_tester_.ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBorealis),
        /*sample=*/borealis_apps_count,
        /*bucket_count=*/1);

    // The installer/launcher is preinstalled, the others are user-installed.
    histogram_tester_.ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBorealis, apps::InstallReason::kDefault),
        /*sample=*/1,
        /*bucket_count=*/1);
    histogram_tester_.ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBorealis, apps::InstallReason::kUser),
        /*sample=*/borealis_apps_count - 1,
        /*bucket_count=*/1);

    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kCrostini),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kCrostini, apps::InstallReason::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kChromeApp),
        /*expected_count=*/0);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kWeb),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kWeb, apps::InstallReason::kSync),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kMacOs),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kMacOs, apps::InstallReason::kUnknown),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kPluginVm),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kPluginVm, apps::InstallReason::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowser, apps::InstallReason::kSystem),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp,
            apps::InstallReason::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp,
            apps::InstallReason::kUser),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kRemote),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kRemote, apps::InstallReason::kPolicy),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kSystemWeb),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kSystemWeb, apps::InstallReason::kSystem),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kWeb),
        /*expected_count=*/1);
    histogram_tester_.ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kWeb, apps::InstallReason::kSubApp),
        /*expected_count=*/1);
  }

  void ModifyInstance(const std::string& app_id,
                      aura::Window* window,
                      apps::InstanceState state) {
    apps::InstanceParams params(app_id, window);
    params.state = std::make_pair(state, base::Time::Now());
    apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
        ->InstanceRegistry()
        .CreateOrUpdateInstance(std::move(params));
  }

  void ModifyWebAppInstance(const std::string& app_id,
                            aura::Window* window,
                            apps::InstanceState state) {
    apps::InstanceParams params(app_id, window);
    params.state = std::make_pair(state, base::Time::Now());
    apps::AppServiceProxyFactory::GetForProfile(testing_profile_.get())
        ->InstanceRegistry()
        .CreateOrUpdateInstance(std::move(params));
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

  std::unique_ptr<aura::Window> CreateWebAppWindow(aura::Window* parent) {
    std::unique_ptr<aura::Window> window(
        aura::test::CreateTestWindowWithDelegate(&delegate1_, 1, gfx::Rect(),
                                                 parent));
    return window;
  }

  void VerifyAppLaunchPerAppTypeHistogram(base::HistogramBase::Count count,
                                          AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(kAppLaunchPerAppTypeHistogramName,
                                         app_type_name, count);
  }

  void VerifyAppLaunchPerAppTypeV2Histogram(base::HistogramBase::Count count,
                                            AppTypeNameV2 app_type_name_v2) {
    histogram_tester().ExpectBucketCount(kAppLaunchPerAppTypeV2HistogramName,
                                         app_type_name_v2, count);
  }

  void VerifyAppRunningDuration(const base::TimeDelta time_delta,
                                AppTypeName app_type_name) {
    DictionaryPrefUpdate update(GetPrefService(), kAppRunningDuration);
    std::string key = GetAppTypeHistogramName(app_type_name);

    absl::optional<base::TimeDelta> unreported_duration =
        base::ValueToTimeDelta(update->FindPath(key));
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

  void VerifyAppUsageTimeCountHistogram(base::HistogramBase::Count count,
                                        AppTypeNameV2 app_type_name) {
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
    int usage_time = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != GURL(kUrl)) {
        continue;
      }
      usage_time += *(test_ukm_recorder()->GetEntryMetric(entry, "Duration"));
      test_ukm_recorder()->ExpectEntryMetric(entry, "UserDeviceMatrix", 0);
      test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                             (int)app_type_name);
    }
    ASSERT_EQ(usage_time, duration);
  }

  void VerifyAppUsageTimeUkm(const GURL& url,
                             int duration,
                             AppTypeName app_type_name) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
    int usage_time = 0;
    for (const auto* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder()->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != url) {
        continue;
      }
      usage_time += *(test_ukm_recorder()->GetEntryMetric(entry, "Duration"));
      test_ukm_recorder()->ExpectEntryMetric(entry, "UserDeviceMatrix", 0);
      test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                             (int)app_type_name);
    }
    ASSERT_EQ(usage_time, duration);
  }

  void VerifyNoAppUsageTimeUkm() {
    auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
    ASSERT_EQ(0U, entries.size());
  }

  void VerifyInstalledAppsUkm(const std::string& app_info,
                              AppTypeName app_type_name,
                              apps::mojom::InstallReason install_reason,
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
      test_ukm_recorder()->ExpectEntryMetric(entry, "InstallReason",
                                             (int)install_reason);
      test_ukm_recorder()->ExpectEntryMetric(entry, "InstallSource2",
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

  void VerifyAppsUninstallUkm(const std::string& app_info,
                              AppTypeName app_type_name,
                              apps::mojom::UninstallSource uninstall_source) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UninstallApp");
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
      test_ukm_recorder()->ExpectEntryMetric(entry, "UninstallSource",
                                             (int)uninstall_source);
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

  AppPlatformMetricsService* app_platform_metrics_service() {
    return app_platform_metrics_service_.get();
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
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_F(AppPlatformMetricsServiceTest, UntilMidnight) {
  task_environment_.FastForwardBy(base::Hours(3));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_F(AppPlatformMetricsServiceTest, LessThanOneDay) {
  task_environment_.FastForwardBy(base::Hours(1));
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
  task_environment_.FastForwardBy(base::Days(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests the UMA metrics that count the number of installed apps.
TEST_F(AppPlatformMetricsServiceTest, InstallApps) {
  task_environment_.FastForwardBy(base::Hours(3));
  VerifyMetrics();

  InstallOneApp("aa", AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);
  task_environment_.FastForwardBy(base::Days(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*expected_count=*/2);
}

TEST_F(AppPlatformMetricsServiceTest, BrowserWindow) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);

  BrowserList* active_browser_list = BrowserList::GetInstance();
  // Expect BrowserList is empty at the beginning.
  EXPECT_EQ(0U, active_browser_list->size());
  std::unique_ptr<Browser> browser1 = CreateBrowserWithAuraWindow1();

  EXPECT_EQ(1U, active_browser_list->size());

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::Minutes(20));
  // Set the browser window running in the background.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kInactiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Minutes(30), AppTypeName::kChromeBrowser);

  // Test multiple browsers.
  std::unique_ptr<Browser> browser2 = CreateBrowserWithAuraWindow2();
  EXPECT_EQ(2U, active_browser_list->size());

  ModifyInstance(app_constants::kChromeAppId,
                 browser2->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kChromeBrowser);

  task_environment_.FastForwardBy(base::Minutes(20));
  ModifyInstance(app_constants::kChromeAppId,
                 browser2->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Hours(1), AppTypeName::kChromeBrowser);

  // Test date change.
  task_environment_.FastForwardBy(base::Days(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kChromeBrowser);
  VerifyAppRunningDurationHistogram(base::Hours(1),
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
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Close the window after running one hour.
  task_environment_.FastForwardBy(base::Minutes(50));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::Hours(1));
  VerifyAppRunningDuration(base::Hours(1), AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::Hours(1));

  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(1),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 1, /*expected_count=*/1,
                              AppTypeName::kArc);
  VerifyAppRunningDuration(base::TimeDelta(), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::Days(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(1),
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
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::Hours(1));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::Hours(2));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 1, /*expected_count=*/1,
                              AppTypeName::kArc);

  task_environment_.FastForwardBy(base::Hours(2));

  // Close the window after running five hours.
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Hours(2), AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::Days(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/2,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(2),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppRunningDuration(base::Hours(0), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);
}

// Tests the UMA metrics when an app window is reactivated.
TEST_F(AppPlatformMetricsServiceTest, ReactiveWindow) {
  std::string app_id = "aa";
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);
  task_environment_.FastForwardBy(base::Minutes(30));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::Minutes(30));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Activa the window after running one hour.
  task_environment_.FastForwardBy(base::Hours(1));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kArc);

  // Close the window after running half hour.
  task_environment_.FastForwardBy(base::Minutes(20));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Hours(1) + base::Minutes(30),
                           AppTypeName::kArc);

  // One day passes.
  task_environment_.FastForwardBy(base::Minutes(20));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/1,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(1) + base::Minutes(30),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 2, /*expected_count=*/1,
                              AppTypeName::kArc);

  // 20 hours passes.
  task_environment_.FastForwardBy(base::Hours(20));

  // Create a new window.
  window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/1, AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::Minutes(50));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Activa the window after running one hour.
  task_environment_.FastForwardBy(base::Hours(1));
  ModifyInstance(app_id, window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Hours(1));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::Hours(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/2,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(3),
                                    /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppActivatedHistogram(/*count*/ 2, /*expected_count=*/2,
                              AppTypeName::kArc);

  // Inactiva the window after running one hour.
  task_environment_.FastForwardBy(base::Hours(3));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Close the window after running five hour.
  task_environment_.FastForwardBy(base::Hours(1));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Hours(3), AppTypeName::kArc);

  // One more day passes.
  task_environment_.FastForwardBy(base::Days(1));
  VerifyAppRunningDurationCountHistogram(/*expected_count=*/3,
                                         AppTypeName::kArc);
  VerifyAppRunningDurationHistogram(base::Hours(3),
                                    /*expected_count=*/2, AppTypeName::kArc);
  VerifyAppActivatedCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppRunningDuration(base::Hours(0), AppTypeName::kArc);
  VerifyAppActivatedCount(/*expected_count=*/0, AppTypeName::kArc);
}

// Tests the app running percentage UMA metrics when launch a browser window
// and an ARC app in one day.
TEST_F(AppPlatformMetricsServiceTest, AppRunningPercentrage) {
  // Launch a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Hours(1));

  // Set the browser window running in the background.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Launch an ARC app.
  std::string app_id = "aa";
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  // Close the window after running one hour.
  task_environment_.FastForwardBy(base::Hours(1));
  ModifyInstance(app_id, window.get(), apps::InstanceState::kDestroyed);

  // One day passes.
  task_environment_.FastForwardBy(base::Hours(1));
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
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  task_environment_.FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1, AppTypeNameV2::kArc);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1, AppTypeName::kArc);

  task_environment_.FastForwardBy(base::Minutes(2));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Create a browser window
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(3));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeNameV2::kArc);
  VerifyAppUsageTimeHistogram(base::Minutes(2),
                              /*expected_count=*/1, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1,
                                   AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1,
                                   AppTypeNameV2::kChromeBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(3),
                              /*expected_count=*/1,
                              AppTypeName::kChromeBrowser);
  VerifyNoAppUsageTimeUkm();

  task_environment_.FastForwardBy(base::Minutes(15));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/2, AppTypeNameV2::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/4,
                                   AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/4,
                                   AppTypeNameV2::kChromeBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/3,
                              AppTypeName::kChromeBrowser);
  VerifyNoAppUsageTimeUkm();

  // Set the browser window inactive.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Set time passed 2 hours to record the usage time AppKM with duration = 18
  // minutes.
  task_environment_.FastForwardBy(base::Minutes(95));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/1080000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTimeUkm) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  // Set sync is not allowed.
  sync_service()->SetDisableReasons(
      syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY);

  task_environment_.FastForwardBy(base::Hours(2));

  VerifyNoAppUsageTimeUkm();

  // Set sync is allowed by setting an empty disable reason set.
  sync_service()->SetDisableReasons(syncer::SyncService::DisableReasonSet());

  task_environment_.FastForwardBy(base::Hours(1));
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  task_environment_.FastForwardBy(base::Hours(1));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/10800000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTimeUkmReportAfterReboot) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(30));

  // Create a web app tab.
  const std::string web_app_id = "w";
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab as activated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(20));
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);

  VerifyNoAppUsageTimeUkm();

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // AppKM is restored from the user pref and reported after 5 minutes after
  // reboot.
  ResetAppPlatformMetricsService();
  VerifyNoAppUsageTimeUkm();

  task_environment_.FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/1800000,
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, /*duration=*/1200000, AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(10));
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Verify UKM is not reported.
  auto entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(2U, entries.size());

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // only the new AppKM is reported.
  ResetAppPlatformMetricsService();
  task_environment_.FastForwardBy(base::Minutes(5));

  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(3U, entries.size());
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/2400000,
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, /*duration=*/1200000, AppTypeName::kChromeBrowser);

  // Reset PlatformMetricsService to simulate the system reboot, and verify no
  // more AppKM is reported.
  ResetAppPlatformMetricsService();
  task_environment_.FastForwardBy(base::Minutes(5));
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(3U, entries.size());
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/2400000,
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, /*duration=*/1200000, AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTimeUkmWithMultipleWindows) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser1 = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Set the browser window1 active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Set the browser window1 inactive.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(1));

  std::unique_ptr<Browser> browser2 = CreateBrowserWithAuraWindow2();
  EXPECT_EQ(2U, BrowserList::GetInstance()->size());

  // Set the browser window2 active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser2->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(7));

  // Close windows.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);
  ModifyInstance(app_constants::kChromeAppId,
                 browser2->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  VerifyNoAppUsageTimeUkm();

  // Verify UKM is reported after 2hours.
  task_environment_.FastForwardBy(base::Minutes(107));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/720000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest,
       UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowswer) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Create a web app tab.
  const std::string web_app_id = "w";
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Set the web app tab as activated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(5));
  VerifyNoAppUsageTimeUkm();

  // Set the browser window and web app tabs as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(2));

  // Set the web app tab as activated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(), kActiveInstanceState);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(3));
  VerifyNoAppUsageTimeUkm();

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(1));

  // Set the web app tab as destroyed.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       apps::InstanceState::kDestroyed);

  // Set the browser window as destroyed.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);
  VerifyNoAppUsageTimeUkm();

  task_environment_.FastForwardBy(base::Minutes(109));

  // Verify the app usage time AppKM for the web app and browser window.
  auto entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(2U, entries.size());
  VerifyAppUsageTimeUkm(url, /*duration=*/480000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/60000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest,
       UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Create a web app tab.
  const std::string web_app_id = "w";
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab as activated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(), kActiveInstanceState);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(5));
  VerifyNoAppUsageTimeUkm();

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(3));

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  VerifyNoAppUsageTimeUkm();
  task_environment_.FastForwardBy(base::Minutes(112));

  // Verify the app usage time AppKM.
  auto entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(2U, entries.size());
  VerifyAppUsageTimeUkm(url, /*duration=*/300000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/180000,
                        AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  // Set the web app tab as activated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(2));

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Verify no more app usage time AppKM is recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(2U, entries.size());

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(118));

  // Verify only the web app UKM is reported.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(3U, entries.size());
  VerifyAppUsageTimeUkm(url, /*duration=*/420000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/180000,
                        AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(1));

  // Set the browser window as destroyed.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  // Set the web app tab as destroyed.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       apps::InstanceState::kDestroyed);

  // Verify no more app usage time AppKM is recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(3U, entries.size());

  task_environment_.FastForwardBy(base::Minutes(119));

  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(4U, entries.size());
  VerifyAppUsageTimeUkm(url, /*duration=*/420000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/240000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, UsageTimeUkmForMultipleWebAppOpenInTab) {
  // Create a browser window.
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  std::unique_ptr<Browser> browser = CreateBrowserWithAuraWindow1();
  EXPECT_EQ(1U, BrowserList::GetInstance()->size());

  // Create web app tabs.
  const std::string web_app_id1 = "w";
  const GURL url1 = GURL("https://foo.com");
  auto web_app_window1 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());
  const std::string web_app_id2 = "w2";
  const GURL url2 = GURL("https://foo2.com");
  auto web_app_window2 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab 1 as activated.
  ModifyWebAppInstance(web_app_id1, web_app_window1.get(),
                       kActiveInstanceState);
  ModifyWebAppInstance(web_app_id2, web_app_window2.get(),
                       kInactiveInstanceState);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(5));

  // Set the web app tab 2 as activated.
  ModifyWebAppInstance(web_app_id2, web_app_window2.get(),
                       kActiveInstanceState);
  ModifyWebAppInstance(web_app_id1, web_app_window1.get(),
                       kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(4));

  // Set the web app tabs as inactivated.
  ModifyWebAppInstance(web_app_id1, web_app_window1.get(),
                       kInactiveInstanceState);
  ModifyWebAppInstance(web_app_id2, web_app_window2.get(),
                       kInactiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(3));
  VerifyNoAppUsageTimeUkm();

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Destroy the browser windows, and web app tabs.
  ModifyWebAppInstance(web_app_id1, web_app_window1.get(),
                       apps::InstanceState::kDestroyed);
  ModifyWebAppInstance(web_app_id2, web_app_window2.get(),
                       apps::InstanceState::kDestroyed);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  task_environment_.FastForwardBy(base::Minutes(108));

  // Verify the app usage time AppKM for the web apps and browser window.
  auto entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
  ASSERT_EQ(3U, entries.size());
  VerifyAppUsageTimeUkm(url1, /*duration=*/300000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url2, /*duration=*/240000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/180000,
                        AppTypeName::kChromeBrowser);
}

TEST_F(AppPlatformMetricsServiceTest, InstalledAppsUkm) {
  // Verify the apps installed during the init phase.
  VerifyInstalledAppsUkm("app://com.google.A", AppTypeName::kArc,
                         apps::mojom::InstallReason::kUser,
                         apps::mojom::InstallSource::kPlayStore,
                         InstallTime::kInit);
  VerifyInstalledAppsUkm(
      "app://bu", AppTypeName::kBuiltIn, apps::mojom::InstallReason::kSystem,
      apps::mojom::InstallSource::kSystem, InstallTime::kInit);
  VerifyInstalledAppsUkm(
      "app://s", AppTypeName::kSystemWeb, apps::mojom::InstallReason::kSystem,
      apps::mojom::InstallSource::kSystem, InstallTime::kInit);
  VerifyInstalledAppsUkm("https://foo.com", AppTypeName::kWeb,
                         apps::mojom::InstallReason::kSync,
                         apps::mojom::InstallSource::kSync, InstallTime::kInit);

  // Install a new ARC app during the running time.
  InstallOneApp("aa", AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Verify the ARC app installed during the running time.
  VerifyInstalledAppsUkm("app://com.google.AA", AppTypeName::kArc,
                         apps::mojom::InstallReason::kUser,
                         apps::mojom::InstallSource::kPlayStore,
                         InstallTime::kRunning);
}

TEST_F(AppPlatformMetricsServiceTest, LaunchApps) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  proxy->Launch(
      /*app_id=*/borealis::kClientAppId, ui::EventFlags::EF_NONE,
      apps::mojom::LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://borealis/client", AppTypeName::kBorealis,
                      apps::mojom::LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kBorealis);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kBorealis);

  proxy->Launch(
      /*app_id=*/borealis::FakeAppId("borealistest"), ui::EventFlags::EF_NONE,
      apps::mojom::LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://borealis/123", AppTypeName::kBorealis,
                      apps::mojom::LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(2, AppTypeName::kBorealis);
  VerifyAppLaunchPerAppTypeV2Histogram(2, AppTypeNameV2::kBorealis);

  proxy->Launch(
      /*app_id=*/crostini::kCrostiniTerminalSystemAppId,
      ui::EventFlags::EF_NONE, apps::mojom::LaunchSource::kFromChromeInternal,
      nullptr);
  VerifyAppsLaunchUkm("app://CrostiniTerminal/Terminal", AppTypeName::kCrostini,
                      apps::mojom::LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kCrostini);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kCrostini);

  proxy->Launch(
      /*app_id=*/"a", ui::EventFlags::EF_NONE,
      apps::mojom::LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://com.google.A", AppTypeName::kArc,
                      apps::mojom::LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kArc);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kArc);

  proxy->LaunchAppWithUrl(
      /*app_id=*/"w", ui::EventFlags::EF_NONE, GURL("https://boo.com/a"),
      apps::mojom::LaunchSource::kFromFileManager, nullptr);
  VerifyAppsLaunchUkm("https://foo.com", AppTypeName::kWeb,
                      apps::mojom::LaunchSource::kFromFileManager);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kWeb);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kWebWindow);

  // TODO(crbug.com/1253250): Register non-mojom apps and use
  // AppServiceProxy::LaunchAppWithParams to test launching.
  proxy->BrowserAppLauncher()->LaunchAppWithParamsForTesting(
      apps::AppLaunchParams("w2",
                            apps::mojom::LaunchContainer::kLaunchContainerTab,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            apps::mojom::LaunchSource::kFromTest));
  VerifyAppsLaunchUkm("https://foo2.com", AppTypeName::kChromeBrowser,
                      apps::mojom::LaunchSource::kFromTest);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kChromeBrowser);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kWebTab);

  proxy->BrowserAppLauncher()->LaunchAppWithParamsForTesting(
      apps::AppLaunchParams("s",
                            apps::mojom::LaunchContainer::kLaunchContainerTab,
                            WindowOpenDisposition::NEW_FOREGROUND_TAB,
                            apps::mojom::LaunchSource::kFromTest));
  VerifyAppsLaunchUkm("app://s", AppTypeName::kSystemWeb,
                      apps::mojom::LaunchSource::kFromTest);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kSystemWeb);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kSystemWeb);
}

TEST_F(AppPlatformMetricsServiceTest, UninstallAppUkm) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  proxy->UninstallSilently(
      /*app_id=*/crostini::kCrostiniTerminalSystemAppId,
      apps::mojom::UninstallSource::kAppList);
  VerifyAppsUninstallUkm("app://CrostiniTerminal/Terminal",
                         AppTypeName::kCrostini,
                         apps::mojom::UninstallSource::kAppList);

  proxy->UninstallSilently(
      /*app_id=*/"a", apps::mojom::UninstallSource::kAppList);
  VerifyAppsUninstallUkm("app://com.google.A", AppTypeName::kArc,
                         apps::mojom::UninstallSource::kAppList);
}

// Tests for app platform input metrics.
class AppPlatformInputMetricsTest : public AppPlatformMetricsServiceTest {
 public:
  void SetUp() override {
    AppPlatformMetricsServiceTest::SetUp();

    ash_test_helper_ = std::make_unique<ash::AshTestHelper>();
    ash_test_helper_->SetUp();

    widget_ = ash::AshTestBase::CreateTestWidget();
  }

  void TearDown() override {
    widget_.reset();
    ash_test_helper_->TearDown();
    AppPlatformMetricsServiceTest::TearDown();
  }

  AppPlatformInputMetrics* app_platform_input_metrics() {
    return app_platform_metrics_service()->app_platform_input_metrics_.get();
  }

  aura::Window* window() { return widget_->GetNativeWindow(); }

  void CreateInputEvent(InputEventSource event_source) {
    switch (event_source) {
      case InputEventSource::kUnknown:
        break;
      case InputEventSource::kMouse: {
        ui::MouseEvent mouse_event(ui::ET_MOUSE_RELEASED, gfx::Point(),
                                   gfx::Point(), base::TimeTicks(), 0, 0);
        ui::Event::DispatcherApi(&mouse_event).set_target(window());
        app_platform_input_metrics()->OnMouseEvent(&mouse_event);
        break;
      }
      case InputEventSource::kStylus: {
        ui::TouchEvent touch_event(
            ui::ET_TOUCH_RELEASED, gfx::Point(), base::TimeTicks(),
            ui::PointerDetails(ui::EventPointerType::kPen, 0));
        ui::Event::DispatcherApi(&touch_event).set_target(window());
        app_platform_input_metrics()->OnTouchEvent(&touch_event);
        break;
      }
      case InputEventSource::kTouch: {
        ui::TouchEvent touch_event(
            ui::ET_TOUCH_RELEASED, gfx::Point(), base::TimeTicks(),
            ui::PointerDetails(ui::EventPointerType::kTouch, 0));
        ui::Event::DispatcherApi(&touch_event).set_target(window());
        app_platform_input_metrics()->OnTouchEvent(&touch_event);
        break;
      }
      case InputEventSource::kKeyboard: {
        ui::KeyEvent key_event(ui::ET_KEY_RELEASED, ui::VKEY_MENU,
                               ui::EF_ALT_DOWN);
        ui::Event::DispatcherApi(&key_event).set_target(window());
        app_platform_input_metrics()->OnKeyEvent(&key_event);
        break;
      }
    }
  }

  std::unique_ptr<Browser> CreateBrowser() {
    Browser::CreateParams params(profile(), true);
    params.type = Browser::TYPE_NORMAL;
    browser_window_ = std::make_unique<TestBrowserWindow>();
    params.window = browser_window_.get();
    browser_window_->SetNativeWindow(window());
    params.window = browser_window_.get();
    return std::unique_ptr<Browser>(Browser::Create(params));
  }

  void VerifyUkm(const std::string& app_info,
                 AppTypeName app_type_name,
                 int event_count,
                 InputEventSource event_source) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
    ASSERT_EQ(1U, entries.size());
    const auto* entry = entries[0];
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entry, GURL(app_info));
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                           (int)app_type_name);
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppInputEventCount",
                                           event_count);
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppInputEventSource",
                                           (int)event_source);
  }

  void VerifyUkm(int count,
                 const std::string& app_info,
                 AppTypeName app_type_name,
                 int event_count,
                 InputEventSource event_source) {
    const auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
    ASSERT_EQ(count, (int)entries.size());
    const auto* entry = entries[count - 1];
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entry, GURL(app_info));
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                           (int)app_type_name);
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppInputEventCount",
                                           event_count);
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppInputEventSource",
                                           (int)event_source);
  }

  void VerifyNoUkm() {
    auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
    ASSERT_EQ(0U, entries.size());
  }

 private:
  std::unique_ptr<ash::AshTestHelper> ash_test_helper_;

  // Where down events are dispatched to.
  std::unique_ptr<views::Widget> widget_;

  std::unique_ptr<TestBrowserWindow> browser_window_;
};

// Verify no more input event is recorded when the window is destroyed.
TEST_F(AppPlatformInputMetricsTest, WindowIsDestroyed) {
  ModifyInstance(/*app_id=*/"a", window(), kActive);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);

  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kDestroyed);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnTwoHours();
  // Verify no more input event is recorded.
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);
}

TEST_F(AppPlatformInputMetricsTest, MouseEvent) {
  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);
}

TEST_F(AppPlatformInputMetricsTest, StylusEvent) {
  ModifyInstance(/*app_id=*/"w", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("https://foo.com", AppTypeName::kWeb, /*event_count=*/1,
            InputEventSource::kStylus);
}

TEST_F(AppPlatformInputMetricsTest, TouchEvents) {
  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kTouch);
  CreateInputEvent(InputEventSource::kTouch);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/2,
            InputEventSource::kTouch);
}

TEST_F(AppPlatformInputMetricsTest, KeyEvents) {
  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kKeyboard);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kKeyboard);

  CreateInputEvent(InputEventSource::kKeyboard);
  CreateInputEvent(InputEventSource::kKeyboard);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(1U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 2 input metrics events are recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());
  std::set<int> counts;
  for (const auto* entry : entries) {
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entry,
                                                 GURL("app://com.google.A"));
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                           (int)AppTypeName::kArc);
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppInputEventSource",
                                           (int)InputEventSource::kKeyboard);
    counts.insert(
        *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount")));
  }
  EXPECT_TRUE(base::Contains(counts, 1));
  EXPECT_TRUE(base::Contains(counts, 2));
}

TEST_F(AppPlatformInputMetricsTest, MultipleEvents) {
  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kMouse);
  CreateInputEvent(InputEventSource::kMouse);
  CreateInputEvent(InputEventSource::kKeyboard);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();

  // Verify 3 input metrics events are recorded.
  const auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(3U, entries.size());
  int event_source;
  int mouse_event_count = 0;
  int keyboard_event_count = 0;
  int stylus_event_count = 0;
  for (const auto* entry : entries) {
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entry,
                                                 GURL("app://com.google.A"));
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                           (int)AppTypeName::kArc);
    event_source =
        *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventSource"));
    if (event_source == (int)InputEventSource::kMouse) {
      mouse_event_count =
          *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount"));
    } else if (event_source == (int)InputEventSource::kKeyboard) {
      keyboard_event_count =
          *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount"));
    } else if (event_source == (int)InputEventSource::kStylus) {
      stylus_event_count =
          *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount"));
    }
  }
  EXPECT_EQ(2, mouse_event_count);
  EXPECT_EQ(1, keyboard_event_count);
  EXPECT_EQ(1, stylus_event_count);
}

TEST_F(AppPlatformInputMetricsTest, BrowserWindow) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto browser = CreateBrowser();

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId, window(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm(std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser, /*event_count=*/1,
            InputEventSource::kMouse);

  // Create a web app tab1.
  const std::string web_app_id1 = "w";
  const GURL url1 = GURL("https://foo.com");
  auto web_app_window1 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab1 as activated.
  ModifyInstance(web_app_id1, web_app_window1.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(1U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 2 input metrics events are recorded.
  VerifyUkm(2, url1.spec(), AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kMouse);

  // Create a web app tab2.
  const std::string web_app_id2 = "w2";
  const GURL url2 = GURL("https://foo2.com");
  auto web_app_window2 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab2 as activated.
  ModifyInstance(web_app_id2, web_app_window2.get(), kActiveInstanceState);
  ModifyInstance(web_app_id1, web_app_window1.get(), kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 3 input metrics events are recorded.
  VerifyUkm(3, url2.spec(), AppTypeName::kChromeBrowser,
            /*event_count=*/2, InputEventSource::kStylus);

  // Set the web app tab2 as destroyed, and web app tab1 as activated.
  ModifyInstance(web_app_id2, web_app_window2.get(),
                 apps::InstanceState::kDestroyed);
  ModifyInstance(web_app_id1, web_app_window1.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kKeyboard);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(3U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 4 input metrics events are recorded.
  VerifyUkm(4, url1.spec(), AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kKeyboard);

  // Set the web app tab1 as inactivated.
  ModifyInstance(web_app_id1, web_app_window1.get(), kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(4U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 5 input metrics events are recorded.
  VerifyUkm(5, std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kStylus);
}

TEST_F(AppPlatformInputMetricsTest, InputEventsUkmReportAfterReboot) {
  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kKeyboard);
  CreateInputEvent(InputEventSource::kStylus);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  ModifyInstance(/*app_id=*/"a", window(), kInactiveInstanceState);

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // AppKM is restored from the user pref and reported after 5 minutes after
  // reboot.
  ResetAppPlatformMetricsService();
  VerifyNoUkm();

  ModifyInstance(/*app_id=*/"a", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kStylus);

  app_platform_input_metrics()->OnFiveMinutes();
  // Verify 2 input metrics events are recorded from pref.
  auto entries =
      test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());
  int event_source;
  int keyboard_event_count = 0;
  int stylus_event_count = 0;
  for (const auto* entry : entries) {
    test_ukm_recorder()->ExpectEntrySourceHasUrl(entry,
                                                 GURL("app://com.google.A"));
    test_ukm_recorder()->ExpectEntryMetric(entry, "AppType",
                                           (int)AppTypeName::kArc);
    event_source =
        *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventSource"));
    if (event_source == (int)InputEventSource::kKeyboard) {
      keyboard_event_count =
          *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount"));
    } else if (event_source == (int)InputEventSource::kStylus) {
      stylus_event_count =
          *(test_ukm_recorder()->GetEntryMetric(entry, "AppInputEventCount"));
    }
  }
  EXPECT_EQ(1, keyboard_event_count);
  EXPECT_EQ(2, stylus_event_count);

  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();

  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());

  ModifyInstance(/*app_id=*/"a", window(), kInactiveInstanceState);

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // only the new AppKM is reported.
  ResetAppPlatformMetricsService();
  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());

  app_platform_input_metrics()->OnFiveMinutes();
  // Verify the input metrics events are recorded from pref.
  VerifyUkm(/*count=*/3, "app://com.google.A", AppTypeName::kArc,
            /*event_count=*/2, InputEventSource::kStylus);

  // Reset PlatformMetricsService to simulate the system reboot, and verify no
  // more AppKM is reported.
  ResetAppPlatformMetricsService();
  app_platform_input_metrics()->OnFiveMinutes();
  // Verify no more input events UKM recorded.
  entries = test_ukm_recorder()->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(3U, entries.size());
}

}  // namespace apps
