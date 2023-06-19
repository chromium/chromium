// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/app_types.h"
#include "ash/constants/ash_features.h"
#include "ash/test/ash_test_base.h"
#include "ash/test/ash_test_helper.h"
#include "base/json/values_util.h"
#include "base/metrics/histogram_base.h"
#include "base/run_loop.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/scoped_run_loop_timeout.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "base/unguessable_token.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_launcher.h"
#include "chrome/browser/apps/app_service/extension_apps_utils.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service_test_base.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/borealis/borealis_util.h"
#include "chrome/browser/ash/borealis/testing/apps.h"
#include "chrome/browser/ash/crostini/crostini_test_helper.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service.h"
#include "chrome/browser/ash/guest_os/guest_os_registry_service_factory.h"
#include "chrome/browser/metrics/structured/event_logging_features.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_list.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/test/base/test_browser_window_aura.h"
#include "chrome/test/base/testing_profile.h"
#include "chromeos/dbus/power/fake_power_manager_client.h"
#include "chromeos/dbus/power_manager/idle.pb.h"
#include "chromeos/dbus/power_manager/suspend.pb.h"
#include "components/app_constants/constants.h"
#include "components/metrics/structured/recorder.h"
#include "components/metrics/structured/structured_events.h"
#include "components/metrics/structured/structured_metrics_features.h"
#include "components/metrics/structured/test/test_structured_metrics_provider.h"
#include "components/prefs/pref_service.h"
#include "components/prefs/scoped_user_pref_update.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_registry_cache.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/instance.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/sync/service/sync_service.h"
#include "components/sync/test/test_sync_service.h"
#include "components/sync_preferences/testing_pref_service_syncable.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_task_environment.h"
#include "services/metrics/public/cpp/ukm_source.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/client/aura_constants.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/test/test_windows.h"
#include "ui/aura/window.h"

using ::testing::_;
using ::testing::Eq;
using ::testing::NotNull;
using ::testing::Sequence;
using ::testing::StrEq;

namespace apps {

namespace {

constexpr char kChromeAppId[] = "plfjlfohfjjpmmifkbcmalnmcebkklkh";
constexpr char kExtensionId[] = "mhjfbmdgcfjbbpaeojofohoefgiehjai";
constexpr char kAndroidAppId[] = "a";
constexpr char kAndroidAppPublisherId[] = "com.google.A";

constexpr apps::InstanceState kActiveInstanceState =
    static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
constexpr apps::InstanceState kInactiveInstanceState =
    static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                     apps::InstanceState::kRunning);

namespace cros_events = metrics::structured::events::v2::cr_os_events;

// Mock observer that observes app platform metrics event callbacks for testing
// purposes.
class MockAppPlatformMetricsObserver : public AppPlatformMetrics::Observer {
 public:
  MockAppPlatformMetricsObserver() = default;
  MockAppPlatformMetricsObserver(const MockAppPlatformMetricsObserver&) =
      delete;
  MockAppPlatformMetricsObserver& operator=(
      const MockAppPlatformMetricsObserver&) = delete;
  ~MockAppPlatformMetricsObserver() override = default;

  MOCK_METHOD(void,
              OnAppInstalled,
              (const std::string& app_id,
               AppType app_type,
               InstallSource app_install_source,
               InstallReason app_install_reason,
               InstallTime app_install_time),
              (override));

  MOCK_METHOD(void,
              OnAppLaunched,
              (const std::string& app_id,
               AppType app_type,
               apps::LaunchSource launch_source),
              (override));

  MOCK_METHOD(void,
              OnAppUninstalled,
              (const std::string& app_id,
               AppType app_type,
               UninstallSource app_uninstall_source),
              (override));

  MOCK_METHOD(void,
              OnAppUsage,
              (const std::string& app_id,
               AppType app_type,
               const base::UnguessableToken& instance_id,
               base::TimeDelta running_time),
              (override));

  MOCK_METHOD(void, OnAppPlatformMetricsDestroyed, (), (override));
};

// Mock observer implementation for the `AppPlatformMetricsService` component.
class MockObserver : public AppPlatformMetricsService::Observer {
 public:
  MockObserver() = default;
  MockObserver(const MockObserver&) = delete;
  MockObserver& operator=(const MockObserver&) = delete;
  ~MockObserver() override = default;

  MOCK_METHOD(void,
              OnAppPlatformMetricsInit,
              (AppPlatformMetrics * app_platform_metrics),
              (override));

  MOCK_METHOD(void, OnAppPlatformMetricsServiceWillBeDestroyed, (), (override));
};

class FakePublisher : public AppPublisher {
 public:
  FakePublisher(AppServiceProxy* proxy, AppType app_type)
      : AppPublisher(proxy) {
    RegisterPublisher(app_type);
  }

  MOCK_METHOD4(Launch,
               void(const std::string& app_id,
                    int32_t event_flags,
                    LaunchSource launch_source,
                    WindowInfoPtr window_info));

  MOCK_METHOD2(LaunchAppWithParams,
               void(AppLaunchParams&& params, LaunchCallback callback));

  MOCK_METHOD6(LoadIcon,
               void(const std::string& app_id,
                    const IconKey& icon_key,
                    apps::IconType icon_type,
                    int32_t size_hint_in_dip,
                    bool allow_placeholder_icon,
                    LoadIconCallback callback));
};

// Impl for testing structured metrics that forwards all writes to the recorder
// directly.
class TestRecorder
    : public metrics::structured::StructuredMetricsClient::RecordingDelegate {
 public:
  TestRecorder() {
    metrics::structured::StructuredMetricsClient::Get()->SetDelegate(this);
  }

  bool IsReadyToRecord() const override { return true; }

  void RecordEvent(metrics::structured::Event&& event) override {
    metrics::structured::Recorder::GetInstance()->RecordEvent(std::move(event));
  }
};

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

}  // namespace

// Tests for app platform metrics service.
class AppPlatformMetricsServiceTest
    : public AppPlatformMetricsServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 public:
  void SetUp() override {
    AppPlatformMetricsServiceTestBase::SetUp();
    if (IsLacrosPrimary()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{ash::features::kLacrosSupport,
                                ash::features::kLacrosPrimary,
                                ash::features::kLacrosOnly,
                                ash::features::kLacrosProfileMigrationForceOff},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {},
          /*disabled_features=*/{
              ash::features::kLacrosSupport, ash::features::kLacrosPrimary,
              ash::features::kLacrosOnly,
              ash::features::kLacrosProfileMigrationForceOff});
    }

    InstallApps();
    // The WebAppProvider system must be started after the apps are added, as
    // the tests explicitly check that the apps start in the 'initializing'
    // state (where they have not yet been registered with the WebAppProvider
    // system).
    web_app::test::AwaitStartWebAppProviderAndSubsystems(profile());
  }

  void TearDown() override {
    AppPlatformMetricsServiceTestBase::TearDown();
    browser_window1_.reset();
    browser_window2_.reset();
  }

  AppTypeName GetWebAppTypeName() {
    return IsLacrosPrimary() ? AppTypeName::kStandaloneBrowserWebApp
                             : AppTypeName::kWeb;
  }

  void InstallApps() {
    auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
    apps::AppRegistryCache& cache = proxy->AppRegistryCache();

    AddApp(cache, kAndroidAppId, AppType::kArc, kAndroidAppPublisherId,
           Readiness::kReady, InstallReason::kUser, InstallSource::kPlayStore,
           true /* should_notify_initialized */);

    AddApp(cache, /*app_id=*/borealis::kClientAppId, AppType::kBorealis, "",
           Readiness::kReady, InstallReason::kUser, InstallSource::kUnknown,
           true /* should_notify_initialized */);

    borealis::CreateFakeApp(profile(), "borealistest", "steam://rungameid/123");
    std::string borealis_app(borealis::FakeAppId("borealistest"));
    AddApp(cache, /*app_id=*/borealis_app.c_str(), AppType::kBorealis, "",
           Readiness::kReady, InstallReason::kUser, InstallSource::kUnknown,
           true /* should_notify_initialized */);

    vm_tools::apps::ApplicationList app_list =
        crostini::CrostiniTestHelper::BasicAppList("test");
    guest_os::GuestOsRegistryServiceFactory::GetForProfile(profile())
        ->UpdateApplicationList(app_list);
    AddApp(cache, /*app_id=*/
           crostini::CrostiniTestHelper::GenerateAppId("test"),
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

    AddApp(cache, /*app_id=*/app_constants::kLacrosAppId,
           AppType::kStandaloneBrowser, "Lacros", Readiness::kReady,
           InstallReason::kSystem, InstallSource::kSystem,
           true /* should_notify_initialized */);

    AddApp(cache,
           /*app_id=*/MuxId(profile(), kChromeAppId),
           AppType::kStandaloneBrowserChromeApp, "Vine", Readiness::kReady,
           InstallReason::kUser, InstallSource::kChromeWebStore,
           true /* should_notify_initialized */, true /*is_platform_app*/);

    AddApp(cache,
           /*app_id=*/MuxId(profile(), kExtensionId),
           AppType::kStandaloneBrowserExtension, "PDF Viewer",
           Readiness::kReady, InstallReason::kUser,
           InstallSource::kChromeWebStore,
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
        /*app_id=*/"r", AppType::kRemote, "", Readiness::kReady,
        InstallReason::kPolicy, InstallSource::kUnknown));
    deltas.push_back(MakeApp(
        /*app_id=*/"subapp", AppType::kWeb, "", Readiness::kReady,
        InstallReason::kSubApp, InstallSource::kUnknown));
    cache.OnApps(std::move(deltas), AppType::kUnknown,
                 false /* should_notify_initialized */);
  }

  void VerifyMetrics() {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kArc, apps::InstallReason::kUser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBuiltIn),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBuiltIn, apps::InstallReason::kSystem),
        /*count=*/1);

    // Should be 4 Borealis apps: The installer + launcher created by the
    // BorealisApps class, plus the two created in this test.
    const int borealis_pre_installed = 2;
    const int borealis_installed_by_test = 2;
    histogram_tester().ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kBorealis),
        /*sample=*/borealis_pre_installed + borealis_installed_by_test,
        /*expected_bucket_count=*/1);

    // The installer + launcher are preinstalled, the others are user-installed.
    histogram_tester().ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBorealis, apps::InstallReason::kDefault),
        /*sample=*/borealis_pre_installed,
        /*expected_bucket_count=*/1);
    histogram_tester().ExpectUniqueSample(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kBorealis, apps::InstallReason::kUser),
        /*sample=*/borealis_installed_by_test,
        /*expected_bucket_count=*/1);

    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kCrostini),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kCrostini, apps::InstallReason::kUser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kChromeApp),
        /*count=*/0);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            GetWebAppTypeName()),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            GetWebAppTypeName(), apps::InstallReason::kSync),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kMacOs),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kMacOs, apps::InstallReason::kUnknown),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kPluginVm),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kPluginVm, apps::InstallReason::kUser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowser, apps::InstallReason::kSystem),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp,
            apps::InstallReason::kUser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kStandaloneBrowserChromeApp,
            apps::InstallReason::kUser),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kRemote),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kRemote, apps::InstallReason::kPolicy),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountHistogramNameForTest(
            AppTypeName::kSystemWeb),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            AppTypeName::kSystemWeb, apps::InstallReason::kSystem),
        /*count=*/1);
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsCountPerInstallReasonHistogramNameForTest(
            GetWebAppTypeName(), apps::InstallReason::kSubApp),
        /*count=*/1);
  }

  std::unique_ptr<Browser> CreateBrowserWithAuraWindow1() {
    std::unique_ptr<aura::Window> window = std::make_unique<aura::Window>(
        &delegate1_, aura::client::WINDOW_TYPE_NORMAL);
    window->SetId(0);
    window->Init(ui::LAYER_TEXTURED);
    Browser::CreateParams params(profile(), true);
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
    Browser::CreateParams params(profile(), true);
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
    const base::Value::Dict& dict =
        GetPrefService()->GetDict(kAppRunningDuration);
    std::string key = GetAppTypeHistogramName(app_type_name);

    absl::optional<base::TimeDelta> unreported_duration =
        base::ValueToTimeDelta(dict.FindByDottedPath(key));
    if (time_delta.is_zero()) {
      EXPECT_FALSE(unreported_duration.has_value());
      return;
    }

    ASSERT_TRUE(unreported_duration.has_value());
    EXPECT_EQ(time_delta, unreported_duration.value());
  }

  void VerifyAppRunningDurationCountHistogram(
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        expected_count);
  }

  void VerifyAppRunningDurationHistogram(
      base::TimeDelta time_delta,
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppRunningPercentageCountHistogram(
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
            app_type_name),
        expected_count);
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

  void VerifyAppActivatedCount(int expected_count, AppTypeName app_type_name) {
    const base::Value::Dict& dict =
        GetPrefService()->GetDict(kAppActivatedCount);
    std::string key = GetAppTypeHistogramName(app_type_name);

    absl::optional<int> activated_count = dict.FindIntByDottedPath(key);
    if (expected_count == 0) {
      EXPECT_FALSE(activated_count.has_value());
      return;
    }

    ASSERT_TRUE(activated_count.has_value());
    EXPECT_EQ(expected_count, activated_count.value());
  }

  void VerifyAppActivatedCountHistogram(
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        expected_count);
  }

  void VerifyAppActivatedHistogram(int count,
                                   base::HistogramBase::Count expected_count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        count, expected_count);
  }

  void VerifyAppUsageTimeCountHistogram(
      base::HistogramBase::Count expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        expected_count);
  }

  void VerifyAppUsageTimeCountHistogram(
      base::HistogramBase::Count expected_count,
      AppTypeNameV2 app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        expected_count);
  }

  void VerifyAppUsageTimeHistogram(base::TimeDelta time_delta,
                                   base::HistogramBase::Count expected_count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppUsageTimeHistogram(base::TimeDelta time_delta,
                                   base::HistogramBase::Count expected_count,
                                   AppTypeNameV2 app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppUsageTimeUkmWithUkmName(const std::string& ukm_name,
                                        const std::string& app_id,
                                        int duration,
                                        AppTypeName app_type_name) {
    const std::string kUrl = std::string("app://") + app_id;
    const auto entries = test_ukm_recorder()->GetEntriesByName(ukm_name);
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

  void VerifyAppUsageTimeUkm(const std::string& app_id,
                             int duration,
                             AppTypeName app_type_name) {
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTime", app_id, duration,
                                     app_type_name);
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTimeReusedSourceId",
                                     app_id, duration, app_type_name);
  }

  void VerifyAppUsageTimeUkmWithUkmName(const std::string& ukm_name,
                                        const GURL& url,
                                        int duration,
                                        AppTypeName app_type_name) {
    const auto entries = test_ukm_recorder()->GetEntriesByName(ukm_name);
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

  void VerifyAppUsageTimeUkm(const GURL& url,
                             int duration,
                             AppTypeName app_type_name) {
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTime", url, duration,
                                     app_type_name);
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTimeReusedSourceId", url,
                                     duration, app_type_name);
  }

  void VerifyAppUsageTimeUkm(uint32_t count) {
    auto entries =
        test_ukm_recorder()->GetEntriesByName("ChromeOSApp.UsageTime");
    ASSERT_EQ(count, entries.size());

    entries = test_ukm_recorder()->GetEntriesByName(
        "ChromeOSApp.UsageTimeReusedSourceId");
    ASSERT_EQ(count, entries.size());
  }

  void VerifyNoAppUsageTimeUkm() { VerifyAppUsageTimeUkm(/*count=*/0); }

  void VerifyInstalledAppsUkm(const std::string& app_info,
                              AppTypeName app_type_name,
                              apps::InstallReason install_reason,
                              apps::InstallSource install_source,
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
                           LaunchSource launch_source) {
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
                              UninstallSource uninstall_source) {
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

  bool IsLacrosPrimary() const { return GetParam(); }

 protected:
  base::test::ScopedFeatureList feature_list_;

 private:
  std::unique_ptr<TestBrowserWindowAura> browser_window1_;
  std::unique_ptr<TestBrowserWindowAura> browser_window2_;
  aura::test::TestWindowDelegate delegate1_;
  aura::test::TestWindowDelegate delegate2_;
};

// Tests OnNewDay() is called after more than one day passes.
TEST_P(AppPlatformMetricsServiceTest, MoreThanOneDay) {
  task_environment_.FastForwardBy(base::Days(1) + base::Hours(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called at midnight.
TEST_P(AppPlatformMetricsServiceTest, UntilMidnight) {
  task_environment_.FastForwardBy(base::Hours(3));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is not called before midnight.
TEST_P(AppPlatformMetricsServiceTest, LessThanOneDay) {
  task_environment_.FastForwardBy(base::Hours(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*count=*/0);
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests OnNewDay() is called after one day passes, even when the device is
// idle.
TEST_P(AppPlatformMetricsServiceTest, MoreThanOneDayDeviceIdle) {
  SetScreenOff(true);
  SetSuspendImminent();
  task_environment_.FastForwardBy(base::Days(1));
  VerifyMetrics();
  EXPECT_EQ(AppPlatformMetricsService::GetDayIdForTesting(base::Time::Now()),
            GetDayIdPref());
}

// Tests the UMA metrics that count the number of installed apps.
TEST_P(AppPlatformMetricsServiceTest, InstallApps) {
  task_environment_.FastForwardBy(base::Hours(3));
  VerifyMetrics();

  InstallOneApp("aa", AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);
  task_environment_.FastForwardBy(base::Days(1));
  histogram_tester().ExpectTotalCount(
      AppPlatformMetrics::GetAppsCountHistogramNameForTest(AppTypeName::kArc),
      /*count=*/2);
}

TEST_P(AppPlatformMetricsServiceTest, BrowserWindow) {
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
TEST_P(AppPlatformMetricsServiceTest, OpenWindowInOneDay) {
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
TEST_P(AppPlatformMetricsServiceTest, OpenWindowInMultipleDays) {
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
TEST_P(AppPlatformMetricsServiceTest, ReactiveWindow) {
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
TEST_P(AppPlatformMetricsServiceTest, AppRunningPercentage) {
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

TEST_P(AppPlatformMetricsServiceTest, UsageTime) {
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

TEST_P(AppPlatformMetricsServiceTest, UsageTimeForLacros) {
  if (!IsLacrosPrimary()) {
    return;
  }

  // Install Chrome apps (hosted apps) during the running time.
  std::string kChromeAppId1 = "bb";
  InstallOneApp(MuxId(profile(), kChromeAppId1),
                AppType::kStandaloneBrowserChromeApp, "BB", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kBrowser);

  const base::UnguessableToken instance_id0 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id2 = base::UnguessableToken::Create();

  // Create a StandaloneBrowser window, and set it as activated for
  // `kLacrosAppId`.
  auto window1 = std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window1.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));
  // Verify recording 5 minutes for AppTypeName::kStandaloneBrowser and
  // AppTypeNameV2::kStandaloneBrowser.
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowser);

  // Create a chrome app tab for `kChromeAppId1`, and set it as activated. We
  // don't need to set the Lacros window as inactivated, because the activated
  // chrome app tab can set the Lacros window as inactivated. And when the
  // chrome app tabs are inactivated, the Lacros window can be set as activated.
  ModifyInstance(instance_id1, MuxId(profile(), kChromeAppId1), window1.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));
  // Verify recording 5 minutes for AppTypeName::kStandaloneBrowser and
  // AppTypeNameV2::kStandaloneBrowserChromeAppTab.
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/2,
                              AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowserChromeAppTab);

  // The chrome app tab is inactivated, so the Lacros window is set as activated
  // in code.
  ModifyInstance(instance_id1, MuxId(profile(), kChromeAppId1), window1.get(),
                 kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));
  // Verify recording 5 minutes for AppTypeName::kStandaloneBrowser and
  // AppTypeNameV2::kStandaloneBrowser.
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/3,
                              AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/2,
                              AppTypeNameV2::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowserChromeAppTab);

  // Set the Lacros window as inactivated.
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window1.get(),
                 kInactiveInstanceState);

  // Create a new window for `kChromeAppId`, and set it as activated.
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id2, MuxId(profile(), kChromeAppId), window2.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));
  // Verify recording 5 minutes for AppTypeName::kStandaloneBrowserChromeApp and
  // AppTypeNameV2::kStandaloneBrowserChromeAppWindow.
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/3,
                              AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeName::kStandaloneBrowserChromeApp);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/2,
                              AppTypeNameV2::kStandaloneBrowser);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowserChromeAppTab);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1,
                              AppTypeNameV2::kStandaloneBrowserChromeAppWindow);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkm) {
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
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Fast forward by 2 hours and verify no usage data is reported to UKM.
  task_environment_.FastForwardBy(base::Hours(2));
  VerifyNoAppUsageTimeUkm();

  // Set sync is allowed by setting an empty disable reason set.
  sync_service()->SetDisableReasons(syncer::SyncService::DisableReasonSet());

  static constexpr base::TimeDelta kAppUsageDuration = base::Hours(1);
  task_environment_.FastForwardBy(kAppUsageDuration);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Fast forward by 2 hours and verify usage data reported to UKM only includes
  // usage data since sync was last enabled.
  task_environment_.FastForwardBy(base::Hours(2));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId,
                        (int)kAppUsageDuration.InMilliseconds(),
                        AppTypeName::kChromeBrowser);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmReportAfterReboot) {
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
  VerifyAppUsageTimeUkm(/*count=*/2);

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // only the new AppKM is reported.
  ResetAppPlatformMetricsService();
  task_environment_.FastForwardBy(base::Minutes(5));

  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/2400000,
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, /*duration=*/1200000, AppTypeName::kChromeBrowser);

  // Reset PlatformMetricsService to simulate the system reboot, and verify no
  // more AppKM is reported.
  ResetAppPlatformMetricsService();
  task_environment_.FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/2400000,
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, /*duration=*/1200000, AppTypeName::kChromeBrowser);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmWithMultipleWindows) {
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

TEST_P(AppPlatformMetricsServiceTest,
       UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser) {
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
  VerifyAppUsageTimeUkm(/*count=*/2);
  VerifyAppUsageTimeUkm(url, /*duration=*/480000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/60000,
                        AppTypeName::kChromeBrowser);
}

TEST_P(AppPlatformMetricsServiceTest,
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
  VerifyAppUsageTimeUkm(/*count=*/2);
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
  VerifyAppUsageTimeUkm(/*count=*/2);

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(web_app_id, web_app_window.get(),
                       kInactiveInstanceState);

  task_environment_.FastForwardBy(base::Minutes(118));

  // Verify only the web app UKM is reported.
  VerifyAppUsageTimeUkm(/*count=*/3);
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
  VerifyAppUsageTimeUkm(/*count=*/3);

  task_environment_.FastForwardBy(base::Minutes(119));

  VerifyAppUsageTimeUkm(/*count=*/4);
  VerifyAppUsageTimeUkm(url, /*duration=*/420000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/240000,
                        AppTypeName::kChromeBrowser);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmForMultipleWebAppOpenInTab) {
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
  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(url1, /*duration=*/300000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url2, /*duration=*/240000, AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, /*duration=*/180000,
                        AppTypeName::kChromeBrowser);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmForStandaloneBrowserApps) {
  // Create a StandaloneBrowser window, and set it as activated for
  // `kLacrosAppId`.
  auto window1 = std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_constants::kLacrosAppId, window1.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Create a Chrome app window, and set it as activated for `kChromeAppId`.
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_constants::kLacrosAppId, window1.get(),
                 kInactiveInstanceState);
  ModifyInstance(MuxId(profile(), kChromeAppId), window2.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(4));

  // Create a Extension window, and set it as activated for `kExtensionId`.
  auto window3 = std::make_unique<aura::Window>(nullptr);
  window3->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(MuxId(profile(), kChromeAppId), window2.get(),
                 kInactiveInstanceState);
  ModifyInstance(MuxId(profile(), kExtensionId), window3.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(3));

  // Set the Extension window as inactived.
  ModifyInstance(MuxId(profile(), kExtensionId), window3.get(),
                 kInactiveInstanceState);

  // Set time passed 2 hours to record the usage time AppKM.
  task_environment_.FastForwardBy(base::Minutes(108));
  VerifyAppUsageTimeUkm(app_constants::kLacrosAppId, /*duration=*/300000,
                        AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeUkm(kChromeAppId, /*duration=*/240000,
                        AppTypeName::kStandaloneBrowserChromeApp);
  VerifyAppUsageTimeUkm(kExtensionId, /*duration=*/180000,
                        AppTypeName::kStandaloneBrowserExtension);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmForWebAppsOpenInLacrosTabs) {
  if (!IsLacrosPrimary()) {
    return;
  }

  const base::UnguessableToken instance_id0 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id2 = base::UnguessableToken::Create();

  const std::string web_app_id1 = "w";
  const GURL url1 = GURL("https://foo.com");

  const std::string web_app_id2 = "w2";
  const GURL url2 = GURL("https://foo2.com");

  // Create a StandaloneBrowser window, and set it as activated for
  // `kLacrosAppId`.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Create a web app tab for `web_app_id1`, and set it as activated. We don't
  // need to set the Lacros window as inactivated, because the activated web app
  // tab can set the Lacros window as inactivated. And when the web app tabs are
  // inactivated, the Lacros window can be set as activated.
  ModifyInstance(instance_id1, web_app_id1, window.get(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(4));

  // Create a web app tab for `web_app_id2`, and set it as activated.
  ModifyInstance(instance_id2, web_app_id2, window.get(), kActiveInstanceState);
  ModifyInstance(instance_id1, web_app_id1, window.get(),
                 kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(3));

  // The web app tabs are inactivated, so the Lacros window is set as activated
  // in code.
  ModifyInstance(instance_id2, web_app_id2, window.get(),
                 kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Set the Lacros window as inactivated.
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window.get(),
                 kInactiveInstanceState);

  // Set time passed 2 hours to record the usage time AppKM.
  task_environment_.FastForwardBy(base::Minutes(108));

  // The Lacros window is activated for 5 minutes before web app tabs are
  // created, and the Lacros window is set as activated for 5 minutes again when
  // web app tabs are inactivated.
  VerifyAppUsageTimeUkm(app_constants::kLacrosAppId, /*duration=*/600000,
                        AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeUkm(url1, /*duration=*/240000,
                        AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeUkm(url2, /*duration=*/180000,
                        AppTypeName::kStandaloneBrowser);
}

TEST_P(AppPlatformMetricsServiceTest, UsageTimeUkmForStandaloneChromeApps) {
  if (!IsLacrosPrimary()) {
    return;
  }

  // Install Chrome apps (hosted apps) during the running time.
  std::string kChromeAppId1 = "bb";
  InstallOneApp(MuxId(profile(), kChromeAppId1),
                AppType::kStandaloneBrowserChromeApp, "BB", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kBrowser);

  const base::UnguessableToken instance_id0 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id2 = base::UnguessableToken::Create();

  // Create a StandaloneBrowser window, and set it as activated for
  // `kLacrosAppId`.
  auto window1 = std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window1.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Create a chrome app tab for `kChromeAppId1`, and set it as activated. We
  // don't need to set the Lacros window as inactivated, because the activated
  // chrome app tab can set the Lacros window as inactivated. And when the
  // chrome app tabs are inactivated, the Lacros window can be set as activated.
  ModifyInstance(instance_id1, MuxId(profile(), kChromeAppId1), window1.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(4));

  // The chrome app tab is inactivated, so the Lacros window is set as activated
  // in code.
  ModifyInstance(instance_id1, MuxId(profile(), kChromeAppId1), window1.get(),
                 kInactiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  // Set the Lacros window as inactivated.
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window1.get(),
                 kInactiveInstanceState);

  // Create a new window for `kChromeAppId`, and set it as activated.
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window2->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id2, MuxId(profile(), kChromeAppId), window2.get(),
                 kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(20));

  // Set the `kChromeAppId` window as inactivated.
  ModifyInstance(instance_id2, MuxId(profile(), kChromeAppId), window2.get(),
                 kInactiveInstanceState);

  // Set time passed 2 hours to record the usage time AppKM.
  task_environment_.FastForwardBy(base::Minutes(86));

  // The Lacros window is activated for 5 minutes before the chrome app tab is
  // created, and the Lacros window is set as activated for 5 minutes again when
  // the chrome app tab is inactivated.
  VerifyAppUsageTimeUkm(app_constants::kLacrosAppId, /*duration=*/600000,
                        AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeUkm(kChromeAppId1, /*duration=*/240000,
                        AppTypeName::kStandaloneBrowser);
  VerifyAppUsageTimeUkm(kChromeAppId, /*duration=*/1200000,
                        AppTypeName::kStandaloneBrowserChromeApp);
}

TEST_P(AppPlatformMetricsServiceTest,
       UsageTimeUkmForWebAppWithStandaloneLacrosWindow) {
  if (!IsLacrosPrimary()) {
    return;
  }

  const base::UnguessableToken instance_id = base::UnguessableToken::Create();

  const std::string web_app_id = "w";
  const GURL url = GURL("https://foo.com");

  // Create a StandaloneBrowser web app window, and set it as activated for
  // `web_app_id`.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(instance_id, web_app_id, window.get(), kActiveInstanceState);
  task_environment_.FastForwardBy(base::Minutes(5));

  ModifyInstance(instance_id, web_app_id, window.get(), kInactiveInstanceState);

  // Set time passed 2 hours to record the usage time AppKM.
  task_environment_.FastForwardBy(base::Minutes(115));
  VerifyAppUsageTimeUkm(url, /*duration=*/300000,
                        AppTypeName::kStandaloneBrowserWebApp);
}

TEST_P(AppPlatformMetricsServiceTest, InstalledAppsUkm) {
  // Verify the apps installed during the init phase.
  VerifyInstalledAppsUkm("app://com.google.A", AppTypeName::kArc,
                         apps::InstallReason::kUser,
                         apps::InstallSource::kPlayStore, InstallTime::kInit);
  VerifyInstalledAppsUkm("app://bu", AppTypeName::kBuiltIn,
                         apps::InstallReason::kSystem,
                         apps::InstallSource::kSystem, InstallTime::kInit);
  VerifyInstalledAppsUkm("app://s", AppTypeName::kSystemWeb,
                         apps::InstallReason::kSystem,
                         apps::InstallSource::kSystem, InstallTime::kInit);
  VerifyInstalledAppsUkm("https://foo.com", GetWebAppTypeName(),
                         apps::InstallReason::kSync, apps::InstallSource::kSync,
                         InstallTime::kInit);
  VerifyInstalledAppsUkm("app://" + std::string(app_constants::kLacrosAppId),
                         AppTypeName::kStandaloneBrowser,
                         apps::InstallReason::kSystem,
                         apps::InstallSource::kSystem, InstallTime::kInit);
  VerifyInstalledAppsUkm(
      "app://" + std::string(kChromeAppId),
      AppTypeName::kStandaloneBrowserChromeApp, apps::InstallReason::kUser,
      apps::InstallSource::kChromeWebStore, InstallTime::kInit);
  VerifyInstalledAppsUkm(
      "app://" + std::string(kExtensionId),
      AppTypeName::kStandaloneBrowserExtension, apps::InstallReason::kUser,
      apps::InstallSource::kChromeWebStore, InstallTime::kInit);

  // Install a new ARC app during the running time.
  InstallOneApp("aa", AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);

  // Verify the ARC app installed during the running time.
  VerifyInstalledAppsUkm(
      "app://com.google.AA", AppTypeName::kArc, apps::InstallReason::kUser,
      apps::InstallSource::kPlayStore, InstallTime::kRunning);

  // Install Chrome apps (hosted apps) during the running time.
  std::string kChromeAppId1 = "bb";
  std::string kChromeAppId2 = "cc";
  InstallOneApp(MuxId(profile(), kChromeAppId1),
                AppType::kStandaloneBrowserChromeApp, "BB", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kBrowser);
  InstallOneApp(MuxId(profile(), kChromeAppId2),
                AppType::kStandaloneBrowserChromeApp, "CC", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kWindow);

  // Verify Chrome apps (hosted apps) installed during the running time.
  VerifyInstalledAppsUkm(
      "app://" + kChromeAppId1, AppTypeName::kStandaloneBrowser,
      apps::InstallReason::kUser, apps::InstallSource::kChromeWebStore,
      InstallTime::kRunning);
  VerifyInstalledAppsUkm(
      "app://" + kChromeAppId2, AppTypeName::kStandaloneBrowserChromeApp,
      apps::InstallReason::kUser, apps::InstallSource::kChromeWebStore,
      InstallTime::kRunning);
}

TEST_P(AppPlatformMetricsServiceTest, LaunchApps) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  // Simulate registering publishers for the launch interface to record metrics.
  proxy->RegisterPublishersForTesting();
  FakePublisher fake_arc_apps(proxy, AppType::kArc);
  FakePublisher fake_borealis_apps(proxy, AppType::kBorealis);
  FakePublisher fake_standalone_browser(proxy, AppType::kStandaloneBrowser);
  FakePublisher fake_standalone_browser_chrome_app(
      proxy, AppType::kStandaloneBrowserChromeApp);
  FakePublisher fake_standalone_browser_extension(
      proxy, AppType::kStandaloneBrowserExtension);

  EXPECT_CALL(fake_borealis_apps,
              Launch(/*app_id=*/borealis::kClientAppId, ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/borealis::kClientAppId, ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://borealis/client", AppTypeName::kBorealis,
                      LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kBorealis);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kBorealis);

  std::string fake_borealis_app = borealis::FakeAppId("borealistest");
  EXPECT_CALL(fake_borealis_apps, Launch(fake_borealis_app, ui::EF_NONE,
                                         LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(fake_borealis_app, ui::EF_NONE,
                LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://borealis/123", AppTypeName::kBorealis,
                      LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(2, AppTypeName::kBorealis);
  VerifyAppLaunchPerAppTypeV2Histogram(2, AppTypeNameV2::kBorealis);

  proxy->Launch(
      /*app_id=*/crostini::CrostiniTestHelper::GenerateAppId("test"),
      ui::EF_NONE, LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://test/test", AppTypeName::kCrostini,
                      LaunchSource::kFromChromeInternal);

  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kCrostini);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kCrostini);

  EXPECT_CALL(fake_arc_apps, Launch(kAndroidAppId, ui::EF_NONE,
                                    LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(kAndroidAppId, ui::EF_NONE, LaunchSource::kFromChromeInternal,
                nullptr);
  VerifyAppsLaunchUkm("app://com.google.A", AppTypeName::kArc,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kArc);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kArc);

  EXPECT_CALL(fake_standalone_browser,
              Launch(/*app_id=*/app_constants::kLacrosAppId, ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/app_constants::kLacrosAppId, ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://" + std::string(app_constants::kLacrosAppId),
                      AppTypeName::kStandaloneBrowser,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kStandaloneBrowser);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kStandaloneBrowser);

  EXPECT_CALL(fake_standalone_browser_chrome_app,
              Launch(/*app_id=*/MuxId(profile(), kChromeAppId), ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/MuxId(profile(), kChromeAppId), ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://" + std::string(kChromeAppId),
                      AppTypeName::kStandaloneBrowserChromeApp,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(1,
                                     AppTypeName::kStandaloneBrowserChromeApp);
  VerifyAppLaunchPerAppTypeV2Histogram(
      1, AppTypeNameV2::kStandaloneBrowserChromeAppWindow);

  // Install Chrome apps (hosted apps) during the running time.
  std::string kChromeAppId1 = "bb";
  std::string kChromeAppId2 = "cc";
  InstallOneApp(MuxId(profile(), kChromeAppId1),
                AppType::kStandaloneBrowserChromeApp, "BB", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kBrowser);
  InstallOneApp(MuxId(profile(), kChromeAppId2),
                AppType::kStandaloneBrowserChromeApp, "CC", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kWindow);

  // Launch `kChromeAppId1`.
  EXPECT_CALL(fake_standalone_browser_chrome_app,
              Launch(/*app_id=*/MuxId(profile(), kChromeAppId1), ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/MuxId(profile(), kChromeAppId1), ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  // Verify `kChromeAppId1` launching as kStandaloneBrowser.
  VerifyAppsLaunchUkm("app://" + kChromeAppId1, AppTypeName::kStandaloneBrowser,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(2 /*launch kLacrosAppId + kChromeAppId1*/,
                                     AppTypeName::kStandaloneBrowser);
  VerifyAppLaunchPerAppTypeV2Histogram(
      1, AppTypeNameV2::kStandaloneBrowserChromeAppTab);

  // Launch `kChromeAppId2` in a Lacros window tab.
  EXPECT_CALL(fake_standalone_browser_chrome_app,
              Launch(/*app_id=*/MuxId(profile(), kChromeAppId2), ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/MuxId(profile(), kChromeAppId2), ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  // Verify `kChromeAppId2` launching as kStandaloneBrowserChromeApp.
  VerifyAppsLaunchUkm("app://" + kChromeAppId2,
                      AppTypeName::kStandaloneBrowserChromeApp,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(2 /*Launch kChromeAppId + kChromeAppId2*/,
                                     AppTypeName::kStandaloneBrowserChromeApp);
  VerifyAppLaunchPerAppTypeV2Histogram(
      2 /*Launch kChromeAppId + kChromeAppId2*/,
      AppTypeNameV2::kStandaloneBrowserChromeAppWindow);

  EXPECT_CALL(fake_standalone_browser_extension,
              Launch(/*app_id=*/MuxId(profile(), kExtensionId), ui::EF_NONE,
                     LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(
      /*app_id=*/MuxId(profile(), kExtensionId), ui::EF_NONE,
      LaunchSource::kFromChromeInternal, nullptr);
  VerifyAppsLaunchUkm("app://" + std::string(kExtensionId),
                      AppTypeName::kStandaloneBrowserExtension,
                      LaunchSource::kFromChromeInternal);
  VerifyAppLaunchPerAppTypeHistogram(1,
                                     AppTypeName::kStandaloneBrowserExtension);
  VerifyAppLaunchPerAppTypeV2Histogram(
      1, AppTypeNameV2::kStandaloneBrowserExtension);

  proxy->LaunchAppWithUrl(
      /*app_id=*/"w", ui::EF_NONE, GURL("https://boo.com/a"),
      LaunchSource::kFromFileManager, nullptr);
  VerifyAppsLaunchUkm("https://foo.com", GetWebAppTypeName(),
                      LaunchSource::kFromFileManager);
  VerifyAppLaunchPerAppTypeHistogram(1, GetWebAppTypeName());
  VerifyAppLaunchPerAppTypeV2Histogram(
      1, IsLacrosPrimary() ? AppTypeNameV2::kStandaloneBrowserWebAppWindow
                           : AppTypeNameV2::kWebWindow);

  // TODO(crbug.com/1253250): Register non-mojom apps and use
  // AppServiceProxy::LaunchAppWithParams to test launching.
  proxy->BrowserAppLauncher()->LaunchAppWithParamsForTesting(AppLaunchParams(
      "w2", LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, LaunchSource::kFromTest));
  if (IsLacrosPrimary()) {
    VerifyAppsLaunchUkm("https://foo2.com", AppTypeName::kStandaloneBrowser,
                        LaunchSource::kFromTest);
    VerifyAppLaunchPerAppTypeHistogram(
        3 /*Launch kLacrosAppId + kChromeAppId1 + `w2`*/,
        AppTypeName::kStandaloneBrowser);
  } else {
    VerifyAppsLaunchUkm("https://foo2.com", AppTypeName::kChromeBrowser,
                        LaunchSource::kFromTest);
    VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kChromeBrowser);
  }
  VerifyAppLaunchPerAppTypeV2Histogram(
      1, IsLacrosPrimary() ? AppTypeNameV2::kStandaloneBrowserWebAppTab
                           : AppTypeNameV2::kWebTab);

  proxy->BrowserAppLauncher()->LaunchAppWithParamsForTesting(AppLaunchParams(
      "s", LaunchContainer::kLaunchContainerTab,
      WindowOpenDisposition::NEW_FOREGROUND_TAB, LaunchSource::kFromTest));
  VerifyAppsLaunchUkm("app://s", AppTypeName::kSystemWeb,
                      LaunchSource::kFromTest);
  VerifyAppLaunchPerAppTypeHistogram(1, AppTypeName::kSystemWeb);
  VerifyAppLaunchPerAppTypeV2Histogram(1, AppTypeNameV2::kSystemWeb);
}

TEST_P(AppPlatformMetricsServiceTest, UninstallAppUkm) {
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  FakePublisher fake_arc_apps(proxy, AppType::kArc);
  FakePublisher fake_standalone_browser_chrome_app(
      proxy, AppType::kStandaloneBrowserChromeApp);
  FakePublisher fake_standalone_browser_extension(
      proxy, AppType::kStandaloneBrowserExtension);

  proxy->UninstallSilently(kAndroidAppId, UninstallSource::kAppList);
  VerifyAppsUninstallUkm("app://com.google.A", AppTypeName::kArc,
                         UninstallSource::kAppList);

  proxy->UninstallSilently(
      /*app_id=*/MuxId(profile(), kChromeAppId), UninstallSource::kAppList);
  VerifyAppsUninstallUkm("app://" + std::string(kChromeAppId),
                         AppTypeName::kStandaloneBrowserChromeApp,
                         UninstallSource::kAppList);

  proxy->UninstallSilently(
      /*app_id=*/MuxId(profile(), kExtensionId), UninstallSource::kAppList);
  VerifyAppsUninstallUkm("app://" + std::string(kExtensionId),
                         AppTypeName::kStandaloneBrowserExtension,
                         UninstallSource::kAppList);
}

TEST_P(AppPlatformMetricsServiceTest,
       ShouldClearUsageInfoFromPrefStoreSubsequently) {
  // Create a new window for the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);

  // Set the window active state.
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kActive);
  static constexpr base::TimeDelta kAppRunningDuration = base::Minutes(5);
  task_environment_.FastForwardBy(kAppRunningDuration);

  // Close app window to stop tracking further usage and verify usage info is
  // persisted in the pref store.
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kDestroyed);
  const auto& usage_dict_pref = GetPrefService()->GetDict(kAppUsageTime);
  ASSERT_THAT(usage_dict_pref.size(), Eq(1UL));
  ASSERT_THAT(usage_dict_pref.Find(kInstanceId.ToString()), NotNull());
  EXPECT_THAT(*usage_dict_pref.FindDict(kInstanceId.ToString())
                   ->FindString(kUsageTimeAppIdKey),
              StrEq(kAndroidAppId));
  EXPECT_THAT(
      base::ValueToTimeDelta(usage_dict_pref.FindDict(kInstanceId.ToString())
                                 ->Find(kUsageTimeDurationKey)),
      Eq(kAppRunningDuration));

  // Fast forward by two hours so it reports usage data and we can verify usage
  // info is cleared from the pref store.
  task_environment_.FastForwardBy(base::Hours(2));
  VerifyAppRunningDuration(kAppRunningDuration, AppTypeName::kArc);
  ASSERT_TRUE(GetPrefService()->GetDict(kAppUsageTime).empty());
}

TEST_P(AppPlatformMetricsServiceTest,
       ShouldClearUsageInfoFromPrefStoreWhenSyncDisabled) {
  // Save usage entry with no usage data to the pref store.
  {
    const base::UnguessableToken& kInstanceId =
        base::UnguessableToken::Create();
    ScopedDictPrefUpdate usage_dict(GetPrefService(), kAppUsageTime);
    AppPlatformMetrics::UsageTime usage_time;
    usage_time.app_id = "TestApp";
    usage_dict->SetByDottedPath(kInstanceId.ToString(),
                                usage_time.ConvertToDict());
  }

  // Disable sync state.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Fast forward by two hours and verify usage info is cleared from the pref
  // store.
  task_environment_.FastForwardBy(base::Hours(2));
  ASSERT_TRUE(GetPrefService()->GetDict(kAppUsageTime).empty());
}

TEST_P(AppPlatformMetricsServiceTest,
       ShouldNotClearUsageInfoFromPrefStoreIfReportingUsageSet) {
  // Create a new window for the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);

  // Set the window active state.
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kActive);
  static constexpr base::TimeDelta kAppRunningDuration = base::Minutes(5);
  task_environment_.FastForwardBy(kAppRunningDuration);

  // Close app window to stop tracking further usage and verify usage info is
  // persisted in the pref store.
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kDestroyed);
  const auto& usage_dict_pref = GetPrefService()->GetDict(kAppUsageTime);
  ASSERT_THAT(usage_dict_pref.size(), Eq(1UL));
  ASSERT_THAT(usage_dict_pref.Find(kInstanceId.ToString()), NotNull());
  EXPECT_THAT(*usage_dict_pref.FindDict(kInstanceId.ToString())
                   ->FindString(kUsageTimeAppIdKey),
              StrEq(kAndroidAppId));
  EXPECT_THAT(
      base::ValueToTimeDelta(usage_dict_pref.FindDict(kInstanceId.ToString())
                                 ->Find(kUsageTimeDurationKey)),
      Eq(kAppRunningDuration));

  // Set reporting usage time for the current app instance and persist it in the
  // pref store.
  {
    ScopedDictPrefUpdate usage_dict(GetPrefService(), kAppUsageTime);
    usage_dict->FindDictByDottedPath(kInstanceId.ToString())
        ->Set(kReportingUsageTimeDurationKey,
              base::TimeDeltaToValue(kAppRunningDuration));
    usage_dict->FindDictByDottedPath(kInstanceId.ToString())
        ->Set(kUsageTimeAppPublisherIdKey, kAndroidAppPublisherId);
  }

  // Fast forward by two hours so it reports usage data and we can verify usage
  // info is not cleared from the pref store.
  task_environment_.FastForwardBy(base::Hours(2));
  VerifyAppRunningDuration(kAppRunningDuration, AppTypeName::kArc);
  const auto& updated_usage_dict_pref =
      GetPrefService()->GetDict(kAppUsageTime);
  ASSERT_THAT(updated_usage_dict_pref.size(), Eq(1UL));
  EXPECT_THAT(updated_usage_dict_pref.Find(kInstanceId.ToString()), NotNull());
  EXPECT_THAT(*usage_dict_pref.FindDict(kInstanceId.ToString())
                   ->FindString(kUsageTimeAppIdKey),
              StrEq(kAndroidAppId));
  EXPECT_THAT(*usage_dict_pref.FindDict(kInstanceId.ToString())
                   ->FindString(kUsageTimeAppPublisherIdKey),
              StrEq(kAndroidAppPublisherId));
  EXPECT_THAT(
      base::ValueToTimeDelta(usage_dict_pref.FindDict(kInstanceId.ToString())
                                 ->Find(kUsageTimeDurationKey)),
      Eq(base::TimeDelta()));
  EXPECT_THAT(
      base::ValueToTimeDelta(usage_dict_pref.FindDict(kInstanceId.ToString())
                                 ->Find(kReportingUsageTimeDurationKey)),
      Eq(kAppRunningDuration));
}

TEST_P(AppPlatformMetricsServiceTest, ShouldNotPersistUsageDataIfSyncDisabled) {
  // Disable sync state.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Create a new window for the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);

  // Set the window active state and simulate app usage.
  const base::UnguessableToken& kInstanceId = base::UnguessableToken::Create();
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kActive);
  static constexpr base::TimeDelta kAppRunningDuration = base::Minutes(5);
  task_environment_.FastForwardBy(kAppRunningDuration);

  // Close app window to stop tracking further usage and verify usage info is
  // not persisted in the pref store.
  ModifyInstance(kInstanceId, kAndroidAppId, window.get(),
                 ::apps::InstanceState::kDestroyed);
  ASSERT_TRUE(GetPrefService()->GetDict(kAppUsageTime).empty());
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppPlatformMetricsServiceTest,
                         testing::Bool() /* IsLacrosPrimary */);

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
TEST_P(AppPlatformInputMetricsTest, WindowIsDestroyed) {
  ModifyInstance(kAndroidAppId, window(), kActive);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);

  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kDestroyed);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnTwoHours();
  // Verify no more input event is recorded.
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);
}

TEST_P(AppPlatformInputMetricsTest, MouseEvent) {
  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/1,
            InputEventSource::kMouse);
}

TEST_P(AppPlatformInputMetricsTest, StylusEvent) {
  ModifyInstance(/*app_id=*/"w", window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("https://foo.com", GetWebAppTypeName(),
            /*event_count=*/1, InputEventSource::kStylus);
}

TEST_P(AppPlatformInputMetricsTest, TouchEvents) {
  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kTouch);
  CreateInputEvent(InputEventSource::kTouch);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://com.google.A", AppTypeName::kArc, /*event_count=*/2,
            InputEventSource::kTouch);
}

TEST_P(AppPlatformInputMetricsTest, KeyEvents) {
  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
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

TEST_P(AppPlatformInputMetricsTest, MultipleEvents) {
  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
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

TEST_P(AppPlatformInputMetricsTest, BrowserWindow) {
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

TEST_P(AppPlatformInputMetricsTest, InputEventsUkmReportAfterReboot) {
  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
  CreateInputEvent(InputEventSource::kKeyboard);
  CreateInputEvent(InputEventSource::kStylus);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();
  ModifyInstance(kAndroidAppId, window(), kInactiveInstanceState);

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // AppKM is restored from the user pref and reported after 5 minutes after
  // reboot.
  ResetAppPlatformMetricsService();
  VerifyNoUkm();

  ModifyInstance(kAndroidAppId, window(), apps::InstanceState::kActive);
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

  ModifyInstance(kAndroidAppId, window(), kInactiveInstanceState);

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

TEST_P(AppPlatformInputMetricsTest, LacrosWindow) {
  ModifyInstance(app_constants::kLacrosAppId, window(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://" + std::string(app_constants::kLacrosAppId),
            AppTypeName::kStandaloneBrowser, /*event_count=*/1,
            InputEventSource::kStylus);
}

TEST_P(AppPlatformInputMetricsTest, StandaloneBrowserChromeApp) {
  ModifyInstance(MuxId(profile(), kChromeAppId), window(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kKeyboard);
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://" + std::string(kChromeAppId),
            AppTypeName::kStandaloneBrowserChromeApp, /*event_count=*/1,
            InputEventSource::kKeyboard);
}

TEST_P(AppPlatformInputMetricsTest, StandaloneBrowserExtension) {
  ModifyInstance(MuxId(profile(), kExtensionId), window(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm("app://" + std::string(kExtensionId),
            AppTypeName::kStandaloneBrowserExtension, /*event_count=*/1,
            InputEventSource::kMouse);
}

TEST_P(AppPlatformInputMetricsTest, LacrosWindowAndWebAppAndChromeApp) {
  if (!IsLacrosPrimary()) {
    return;
  }

  window()->SetProperty(aura::client::kAppType,
                        static_cast<int>(ash::AppType::LACROS));

  const base::UnguessableToken instance_id0 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id1 = base::UnguessableToken::Create();
  const base::UnguessableToken instance_id2 = base::UnguessableToken::Create();

  // Set window as activated for `kLacrosAppId`.
  ModifyInstance(instance_id0, app_constants::kLacrosAppId, window(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnTwoHours();
  // Verify 1 input metrics event for kMouse is recorded.
  VerifyUkm("app://" + std::string(app_constants::kLacrosAppId),
            AppTypeName::kStandaloneBrowser, /*event_count=*/1,
            InputEventSource::kMouse);

  // Set the web app tab1 as activated. We don't need to set the Lacros window
  // as inactivated, because the activated web app tab can set the Lacros window
  // as inactivated. And when the web app tabs are inactivated, the Lacros
  // window can be set as activated.
  const std::string web_app_id1 = "w";
  const GURL url1 = GURL("https://foo.com");
  task_environment_.FastForwardBy(base::Minutes(4));
  ModifyInstance(instance_id1, web_app_id1, window(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse);
  app_platform_input_metrics()->OnTwoHours();
  // Verify 2 input metrics events are recorded.
  VerifyUkm(2, url1.spec(), AppTypeName::kStandaloneBrowser,
            /*event_count=*/1, InputEventSource::kMouse);

  // Install a Chrome app (hosted app) during the running time.
  std::string kChromeAppId1 = "bb";
  InstallOneApp(MuxId(profile(), kChromeAppId1),
                AppType::kStandaloneBrowserChromeApp, "BB", Readiness::kReady,
                InstallSource::kChromeWebStore,
                /*is_platform_app=*/false, WindowMode::kBrowser);
  // Set the Chrome app tab as activated.
  ModifyInstance(instance_id2, MuxId(profile(), kChromeAppId1), window(),
                 kActiveInstanceState);
  ModifyInstance(instance_id1, web_app_id1, window(), kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus);
  app_platform_input_metrics()->OnTwoHours();
  // Verify 3 input metrics events are recorded.
  VerifyUkm(3, "app://" + kChromeAppId1, AppTypeName::kStandaloneBrowser,
            /*event_count=*/1, InputEventSource::kStylus);

  // Set the Chrome app tab as inactivated, then the Lacros window should be set
  // as activated in code.
  ModifyInstance(instance_id2, MuxId(profile(), kChromeAppId1), window(),
                 kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kKeyboard);
  app_platform_input_metrics()->OnTwoHours();
  // Verify 4 input metrics events are recorded.
  VerifyUkm(4, "app://" + std::string(app_constants::kLacrosAppId),
            AppTypeName::kStandaloneBrowser,
            /*event_count=*/1, InputEventSource::kKeyboard);
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppPlatformInputMetricsTest,
                         testing::Bool() /* IsLacrosPrimary */);

// Tests for app platform metrics observers.
class AppPlatformMetricsObserverTest : public AppPlatformMetricsServiceTest {
 protected:
  void SetUp() override {
    AppPlatformMetricsServiceTest::SetUp();

    // We transfer ownership of the unique_ptr for the app platform metrics
    // service in some test scenarios. Therefore, we save a copy of the raw
    // pointer so it can be used during teardown.
    app_platform_metrics_service_ = app_platform_metrics_service();
    app_platform_metrics_service_->AppPlatformMetrics()->AddObserver(
        &observer_);
  }

  void TearDown() override {
    // App platform metrics component has not been destructed yet, so we
    // unregister the observer to reduce noise (observer is destructed before
    // the component is).
    app_platform_metrics_service_->AppPlatformMetrics()->RemoveObserver(
        &observer_);
    AppPlatformMetricsServiceTest::TearDown();
  }

  MockAppPlatformMetricsObserver observer_;
  raw_ptr<AppPlatformMetricsService, DanglingUntriaged>
      app_platform_metrics_service_;
};

TEST_P(AppPlatformMetricsObserverTest, ShouldNotifyObserverOnAppInstalled) {
  // Observers should be notified even when app sync is disabled.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  const std::string app_id(borealis::FakeAppId("borealis-fake"));
  EXPECT_CALL(
      observer_,
      OnAppInstalled(app_id, AppType::kBorealis, InstallSource::kUnknown,
                     InstallReason::kUser, InstallTime::kRunning))
      .Times(1);
  InstallOneApp(app_id, AppType::kBorealis,
                /*publisher_id=*/"", Readiness::kReady, InstallSource::kUnknown,
                /*is_platform_app=*/false, WindowMode::kBrowser);
}

TEST_P(AppPlatformMetricsObserverTest, ShouldNotifyObserverOnAppLaunch) {
  // Observers should be notified even when app sync is disabled.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Launch a pre-installed app and verify the observer is notified.
  EXPECT_CALL(observer_, OnAppLaunched(kAndroidAppId, AppType::kArc,
                                       apps::LaunchSource::kFromChromeInternal))
      .Times(1);

  auto* const proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_arc_apps(proxy, AppType::kArc);
  proxy->Launch(kAndroidAppId, ui::EF_NONE,
                apps::LaunchSource::kFromChromeInternal, nullptr);
  task_environment_.RunUntilIdle();
}

TEST_P(AppPlatformMetricsObserverTest, ShouldNotifyObserverOnAppUninstall) {
  // Observers should be notified even when app sync is disabled.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Uninstall a pre-installed app and verify the observer is notified.
  EXPECT_CALL(observer_, OnAppUninstalled(kAndroidAppId, AppType::kArc,
                                          UninstallSource::kAppList))
      .Times(1);

  auto* const proxy = AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_arc_apps(proxy, AppType::kArc);
  proxy->UninstallSilently(kAndroidAppId, UninstallSource::kAppList);
  task_environment_.RunUntilIdle();
}

TEST_P(AppPlatformMetricsObserverTest, ShouldNotifyObserverOnAppUsage) {
  // Observers should be notified even when app sync is disabled.
  sync_service()->SetDisableReasons(
      {syncer::SyncService::DISABLE_REASON_ENTERPRISE_POLICY});

  // Create a new window for the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);

  // Set the window active state and verify the observer is notified
  // with the appropriate running time with every notification.
  const base::UnguessableToken& instance_id = base::UnguessableToken::Create();
  ModifyInstance(instance_id, kAndroidAppId, window.get(),
                 kActiveInstanceState);

  // Usage metrics are recorded every 5 minutes and on window inactivation, so
  // we can expect two notifications with relevant usage times (5 minutes + 3
  // minutes) across a 8 minute usage window.
  Sequence s;
  EXPECT_CALL(observer_, OnAppUsage(kAndroidAppId, AppType::kArc, instance_id,
                                    base::Minutes(5)))
      .Times(1)
      .InSequence(s);
  EXPECT_CALL(observer_, OnAppUsage(kAndroidAppId, AppType::kArc, instance_id,
                                    base::Minutes(3)))
      .Times(1)
      .InSequence(s);

  // Fast forward to trigger first notification.
  task_environment_.FastForwardBy(base::Minutes(8));

  // Set app inactive. This should also trigger the second notification with
  // usage time delta after the first one.
  ModifyInstance(instance_id, kAndroidAppId, window.get(),
                 kInactiveInstanceState);
}

TEST_P(AppPlatformMetricsObserverTest, ShouldNotNotifyUnregisteredObservers) {
  auto* const proxy = AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  proxy->AppPlatformMetrics()->RemoveObserver(&observer_);

  // Uninstall a pre-installed app and verify the unregistered observer
  // is not notified.
  EXPECT_CALL(observer_, OnAppUninstalled(kAndroidAppId, AppType::kArc,
                                          UninstallSource::kAppList))
      .Times(0);
  proxy->UninstallSilently(kAndroidAppId, UninstallSource::kAppList);
  task_environment_.RunUntilIdle();
}

TEST_P(AppPlatformMetricsObserverTest, ShouldNotifyObserverOnDestruction) {
  // Create a new instance of `AppPlatformMetricsService` here so we can
  // test destruction lifecycle without affecting pre-existing test teardown
  // fixtures.
  auto app_platform_metrics_service =
      std::make_unique<AppPlatformMetricsService>(profile());
  app_platform_metrics_service->Start(
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->AppRegistryCache(),
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->InstanceRegistry());
  app_platform_metrics_service->AppPlatformMetrics()->AddObserver(&observer_);

  EXPECT_CALL(observer_, OnAppPlatformMetricsDestroyed()).Times(1);
  app_platform_metrics_service.reset();
  task_environment_.RunUntilIdle();
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppPlatformMetricsObserverTest,
                         testing::Bool() /* IsLacrosPrimary */);

// Tests for app discovery metrics test.
class AppDiscoveryMetricsTest : public AppPlatformMetricsServiceTest {
 public:
  void SetUp() override {
    test_recorder_ = std::make_unique<TestRecorder>();
    test_structured_metrics_provider_ =
        std::make_unique<metrics::structured::TestStructuredMetricsProvider>();
    test_structured_metrics_provider_->EnableRecording();

    metrics::structured::Recorder::GetInstance()->SetUiTaskRunner(
        task_environment_.GetMainThreadTaskRunner());

    if (IsLacrosPrimary()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{ash::features::kLacrosSupport,
                                ash::features::kLacrosPrimary,
                                metrics::structured::kAppDiscoveryLogging,
                                metrics::structured::kEventSequenceLogging},
          {});
    } else {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{metrics::structured::kAppDiscoveryLogging,
                                metrics::structured::kEventSequenceLogging},
          /*disabled_features=*/{ash::features::kLacrosSupport,
                                 ash::features::kLacrosPrimary});
    }

    AppPlatformMetricsServiceTestBase::SetUp();
  }

  metrics::structured::TestStructuredMetricsProvider*
  test_structured_metrics_provider() {
    return test_structured_metrics_provider_.get();
  }

  void ValidateAppInstallEvent(const metrics::structured::Event& event,
                               const std::string& app_url,
                               AppType app_type,
                               InstallSource install_source,
                               InstallReason install_reason) {
    cros_events::AppDiscovery_AppInstalled expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());

    expected_event.SetAppId(app_url)
        .SetAppType(static_cast<int>(app_type))
        .SetInstallSource(static_cast<int>(install_source))
        .SetInstallReason(static_cast<int>(install_reason));

    EXPECT_EQ(expected_event.metric_values(), event.metric_values());
  }

  void ValidateAppUninstallEvent(const metrics::structured::Event& event,
                                 const std::string& app_url,
                                 AppType app_type,
                                 UninstallSource uninstall_source) {
    cros_events::AppDiscovery_AppUninstall expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());

    expected_event.SetAppId(app_url)
        .SetAppType(static_cast<int>(app_type))
        .SetUninstallSource(static_cast<int>(uninstall_source));

    EXPECT_EQ(expected_event.metric_values(), event.metric_values());
  }

  void ValidateAppLaunchEvent(const metrics::structured::Event& event,
                              const std::string& app_id,
                              AppType app_type,
                              LaunchSource launch_source) {
    cros_events::AppDiscovery_AppLaunched expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());

    expected_event.SetAppId(app_id)
        .SetAppType(static_cast<int>(app_type))
        .SetLaunchSource(static_cast<int>(launch_source));

    EXPECT_EQ(expected_event.metric_values(), event.metric_values());
  }

  void ValidateAppStateEvent(const metrics::structured::Event& event,
                             const std::string& app_id,
                             AppStateChange app_state) {
    cros_events::AppDiscovery_AppStateChanged expected_event;

    EXPECT_EQ(expected_event.project_name(), event.project_name());
    EXPECT_EQ(expected_event.event_name(), event.event_name());

    expected_event.SetAppId(app_id).SetAppState(static_cast<int>(app_state));

    EXPECT_EQ(expected_event.metric_values(), event.metric_values());
  }

 private:
  std::unique_ptr<TestRecorder> test_recorder_;
  std::unique_ptr<metrics::structured::TestStructuredMetricsProvider>
      test_structured_metrics_provider_;
};

TEST_P(AppDiscoveryMetricsTest, AppInstallStateMetricsRecorded) {
  base::test::ScopedRunLoopTimeout default_timeout(FROM_HERE, base::Seconds(3));

  // Setup publisher for arc app.
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());
  FakePublisher fake_arc_apps(proxy, AppType::kArc);

  auto app_type = AppType::kArc;
  const std::string app_id = "aa";
  auto install_source = InstallSource::kPlayStore;

  // Wait for events to be recorded.
  base::RunLoop install_event_run_loop;
  auto install_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppInstallEvent(event, app_id, app_type, install_source,
                                InstallReason::kUser);
        install_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      install_record_callback);

  InstallOneApp(app_id, app_type, "publisher", Readiness::kReady,
                install_source);
  install_event_run_loop.Run();

  // Uninstall the app.
  base::RunLoop uninstall_event_run_loop;
  const auto kUninstallSource = UninstallSource::kAppList;
  auto uninstall_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppUninstallEvent(event, app_id, app_type, kUninstallSource);
        uninstall_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      uninstall_record_callback);

  proxy->UninstallSilently(app_id, kUninstallSource);
  uninstall_event_run_loop.Run();
}

TEST_P(AppDiscoveryMetricsTest, AppActivityMetricsRecorded) {
  base::test::ScopedRunLoopTimeout default_timeout(FROM_HERE, base::Seconds(3));

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  apps::AppRegistryCache& cache = proxy->AppRegistryCache();
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  // Install an ARC app to test.
  AddApp(cache, kAndroidAppId, AppType::kArc, kAndroidAppPublisherId,
         Readiness::kReady, InstallReason::kUser, InstallSource::kPlayStore,
         true /* should_notify_initialized */);

  // Simulate registering publishers for the launch interface to record metrics.
  proxy->RegisterPublishersForTesting();
  FakePublisher fake_arc_apps(proxy, AppType::kArc);

  // Create a window to simulate launching the app.
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);

  // Validate event recorded after event is recorded.
  base::RunLoop launch_event_run_loop;
  auto launch_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppLaunchEvent(event, kAndroidAppId, AppType::kArc,
                               LaunchSource::kFromChromeInternal);
        launch_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      launch_record_callback);

  EXPECT_CALL(fake_arc_apps, Launch(kAndroidAppId, ui::EF_NONE,
                                    LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(kAndroidAppId, ui::EF_NONE, LaunchSource::kFromChromeInternal,
                nullptr);
  ModifyInstance(kAndroidAppId, window.get(), apps::InstanceState::kStarted);
  launch_event_run_loop.Run();

  // Mark app as kRunning otherwise active event will not trigger since the app
  // isn't considered to be running yet.
  ModifyInstance(
      kAndroidAppId, window.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                       apps::InstanceState::kRunning));

  // Validate launch -> active event is recorded.
  base::RunLoop active_event_run_loop;
  auto active_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kActive);
        active_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      active_record_callback);

  ModifyInstance(
      kAndroidAppId, window.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kActive |
                                       apps::InstanceState::kRunning));
  active_event_run_loop.Run();

  // Validate active -> inactive event is recorded.
  base::RunLoop hidden_event_run_loop;
  auto hidden_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kInactive);
        hidden_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      hidden_record_callback);

  ModifyInstance(
      kAndroidAppId, window.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kHidden |
                                       apps::InstanceState::kRunning));
  hidden_event_run_loop.Run();

  // Validate inactive -> active is recorded.
  base::RunLoop active_event_run_loop2;
  auto active_record_callback2 = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kActive);
        active_event_run_loop2.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      active_record_callback2);

  ModifyInstance(
      kAndroidAppId, window.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kActive |
                                       apps::InstanceState::kRunning));
  active_event_run_loop2.Run();

  // Validate closed event is recorded.
  base::RunLoop closed_event_run_loop;
  auto closed_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kClosed);
        closed_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      closed_record_callback);

  ModifyInstance(kAndroidAppId, window.get(), apps::InstanceState::kDestroyed);
  closed_event_run_loop.Run();
}

TEST_P(AppDiscoveryMetricsTest, AppActivityMetricsRecordedForTwoInstances) {
  base::test::ScopedRunLoopTimeout default_timeout(FROM_HERE, base::Seconds(3));

  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile());
  apps::AppRegistryCache& cache = proxy->AppRegistryCache();
  proxy->SetAppPlatformMetricsServiceForTesting(GetAppPlatformMetricsService());

  // Install an ARC app to test.
  AddApp(cache, kAndroidAppId, AppType::kArc, kAndroidAppPublisherId,
         Readiness::kReady, InstallReason::kUser, InstallSource::kPlayStore,
         true /* should_notify_initialized */);

  // Simulate registering publishers for the launch interface to record metrics.
  proxy->RegisterPublishersForTesting();
  FakePublisher fake_arc_apps(proxy, AppType::kArc);

  // Create a window to simulate launching the app.
  auto window1 = std::make_unique<aura::Window>(nullptr);
  auto window2 = std::make_unique<aura::Window>(nullptr);
  window1->Init(ui::LAYER_NOT_DRAWN);
  window2->Init(ui::LAYER_NOT_DRAWN);

  // Validate event recorded after event is recorded.
  base::RunLoop launch_event_run_loop;
  auto launch_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppLaunchEvent(event, kAndroidAppId, AppType::kArc,
                               LaunchSource::kFromChromeInternal);
        launch_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      launch_record_callback);

  EXPECT_CALL(fake_arc_apps, Launch(kAndroidAppId, ui::EF_NONE,
                                    LaunchSource::kFromChromeInternal, _))
      .Times(1);
  proxy->Launch(kAndroidAppId, ui::EF_NONE, LaunchSource::kFromChromeInternal,
                nullptr);
  ModifyInstance(kAndroidAppId, window1.get(), apps::InstanceState::kStarted);
  ModifyInstance(kAndroidAppId, window2.get(), apps::InstanceState::kStarted);
  launch_event_run_loop.Run();

  // Mark app as kRunning otherwise active event will not trigger since the app
  // isn't considered to be running yet.
  ModifyInstance(
      kAndroidAppId, window1.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                       apps::InstanceState::kRunning));
  ModifyInstance(
      kAndroidAppId, window2.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                       apps::InstanceState::kRunning));

  // Validate launch -> active event is recorded.
  base::RunLoop active_event_run_loop;
  auto active_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kActive);
        active_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      active_record_callback);

  ModifyInstance(
      kAndroidAppId, window1.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kActive |
                                       apps::InstanceState::kRunning));
  active_event_run_loop.Run();

  // Active event should not be recorded when 2nd instance becomes active.
  auto active_record_callback2 =
      base::BindLambdaForTesting([](const metrics::structured::Event& event) {
        ADD_FAILURE() << "Should not be called!";
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      active_record_callback2);
  ModifyInstance(
      kAndroidAppId, window2.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kActive |
                                       apps::InstanceState::kRunning));

  // Inactive event is not recorded if one instance becomes inactive but other
  // instance is active.
  auto hidden_record_callback =
      base::BindLambdaForTesting([](const metrics::structured::Event& event) {
        ADD_FAILURE() << "Should not be called!";
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      hidden_record_callback);

  ModifyInstance(
      kAndroidAppId, window1.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kHidden |
                                       apps::InstanceState::kRunning));

  // Validate inactive event recorded if both instances are inactive.
  base::RunLoop inactive_event_run_loop;
  auto inactive_record_callback = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kInactive);
        inactive_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      inactive_record_callback);

  ModifyInstance(
      kAndroidAppId, window2.get(),
      static_cast<apps::InstanceState>(apps::InstanceState::kVisible |
                                       apps::InstanceState::kRunning));
  inactive_event_run_loop.Run();

  // Validate closed event is not recorded when one instance is closed.
  auto closed_record_callback =
      base::BindLambdaForTesting([](const metrics::structured::Event& event) {
        ADD_FAILURE() << "Should not be called!";
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      closed_record_callback);
  ModifyInstance(kAndroidAppId, window1.get(), apps::InstanceState::kDestroyed);

  // Validate closed event is recorded when both instances are closed.
  base::RunLoop closed_event_run_loop;
  auto closed_record_callback2 = base::BindLambdaForTesting(
      [&, this](const metrics::structured::Event& event) {
        ValidateAppStateEvent(event, kAndroidAppId, AppStateChange::kClosed);
        closed_event_run_loop.Quit();
      });
  test_structured_metrics_provider()->SetOnEventsRecordClosure(
      closed_record_callback2);

  ModifyInstance(kAndroidAppId, window2.get(), apps::InstanceState::kDestroyed);
  closed_event_run_loop.Run();
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppDiscoveryMetricsTest,
                         testing::Bool() /* IsLacrosPrimary */);

class AppPlatformMetricsServiceObserverTest
    : public AppPlatformMetricsServiceTestBase,
      public ::testing::WithParamInterface<bool> {
 protected:
  void SetUp() override {
    if (IsLacrosPrimary()) {
      feature_list_.InitWithFeatures(
          /*enabled_features=*/{ash::features::kLacrosSupport,
                                ash::features::kLacrosPrimary},
          {});
    } else {
      feature_list_.InitWithFeatures(
          {},
          /*disabled_features=*/{ash::features::kLacrosSupport,
                                 ash::features::kLacrosPrimary});
    }

    // Set up test user.
    AddRegularUser("test@test.com");
  }

  bool IsLacrosPrimary() const { return GetParam(); }

  MockObserver* observer() { return &observer_; }

 private:
  base::test::ScopedFeatureList feature_list_;

  // Mock observer for the `AppPlatformMetricsService` component. Needs to
  // outlive the lifetime of the component for testing purposes.
  MockObserver observer_;
};

TEST_P(AppPlatformMetricsServiceObserverTest,
       NotifyObserversOnAppPlatformMetricsInit) {
  MockObserver* const observer_ptr = observer();
  AppPlatformMetricsService app_platform_metrics_service(profile());
  app_platform_metrics_service.AddObserver(observer_ptr);
  EXPECT_CALL(*observer_ptr, OnAppPlatformMetricsInit(_))
      .WillOnce([&](AppPlatformMetrics* app_platform_metrics) {
        EXPECT_THAT(app_platform_metrics,
                    Eq(app_platform_metrics_service.AppPlatformMetrics()));
      });
  app_platform_metrics_service.Start(
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
      AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry());
}

TEST_P(AppPlatformMetricsServiceObserverTest,
       ShouldNotNotifyObserversOnAppPlatformMetricsInitIfUnregistered) {
  MockObserver* const observer_ptr = observer();
  AppPlatformMetricsService app_platform_metrics_service(profile());

  // Unregister registered observer before init and verify observer is not
  // notified.
  app_platform_metrics_service.AddObserver(observer_ptr);
  app_platform_metrics_service.RemoveObserver(observer_ptr);
  EXPECT_CALL(*observer_ptr, OnAppPlatformMetricsInit(_)).Times(0);
  app_platform_metrics_service.Start(
      AppServiceProxyFactory::GetForProfile(profile())->AppRegistryCache(),
      AppServiceProxyFactory::GetForProfile(profile())->InstanceRegistry());
}

TEST_P(AppPlatformMetricsServiceObserverTest,
       ShouldNotifyObserverOnDestruction) {
  MockObserver* const observer_ptr = observer();
  auto app_platform_metrics_service =
      std::make_unique<AppPlatformMetricsService>(profile());
  app_platform_metrics_service->AddObserver(observer_ptr);
  EXPECT_CALL(*observer_ptr, OnAppPlatformMetricsServiceWillBeDestroyed)
      .Times(1);
  app_platform_metrics_service.reset();
}

INSTANTIATE_TEST_SUITE_P(All,
                         AppPlatformMetricsServiceObserverTest,
                         ::testing::Bool() /* IsLacrosPrimary */);

}  // namespace apps
