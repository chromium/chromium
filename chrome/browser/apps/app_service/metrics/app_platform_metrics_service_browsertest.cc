// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include <atomic>
#include <memory>
#include <string>
#include <vector>

#include "base/json/values_util.h"
#include "base/memory/raw_ref.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/run_until.h"
#include "base/threading/sequence_local_storage_map.h"
#include "base/time/time_override.h"
#include "base/values.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/lifetime/application_lifetime.h"
#include "chrome/browser/metrics/usertype_by_devicetype_metrics_provider.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/browser_window/public/global_browser_collection.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/login/login_state/scoped_test_public_session_login_state.h"
#include "components/app_constants/constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/policy/core/common/policy_logger.h"
#include "components/prefs/pref_service.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_utils.h"
#include "services/metrics/public/cpp/ukm_recorder.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/aura/test/test_window_delegate.h"
#include "ui/aura/window.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/point.h"

namespace apps {

namespace {

constexpr char kWebAppId1[] = "web_app_id_1";
constexpr char kWebAppId2[] = "web_app_id_2";

constexpr apps::InstanceState kActiveInstanceState =
    static_cast<apps::InstanceState>(
        apps::InstanceState::kStarted | apps::InstanceState::kRunning |
        apps::InstanceState::kActive | apps::InstanceState::kVisible);
constexpr apps::InstanceState kInactiveInstanceState =
    static_cast<apps::InstanceState>(apps::InstanceState::kStarted |
                                     apps::InstanceState::kRunning);

// FakeSyncService is needed to simulate active app sync. Without it,
// ShouldRecordAppKM returns false because it thinks the user has not enabled
// app sync, and no UKM events will be recorded, causing entries.size() to be 0.
class FakeSyncService : public syncer::TestSyncService {
 public:
  syncer::DataTypeSet GetPreferredDataTypes() const override {
    syncer::DataTypeSet types;
    types.Put(syncer::DataType::APPS);
    return types;
  }
  syncer::DataTypeSet GetActiveDataTypes() const override {
    syncer::DataTypeSet types;
    types.Put(syncer::DataType::APPS);
    return types;
  }
};

std::atomic<int64_t> g_fake_time_us{0};
std::atomic<int64_t> g_fake_ticks_us{0};
std::atomic<int64_t> g_last_ticks_us{0};

base::Time GetFakeTime() {
  return base::Time::FromDeltaSinceWindowsEpoch(
      base::Microseconds(g_fake_time_us.load(std::memory_order_relaxed)));
}
base::TimeTicks GetFakeTimeTicks() {
  int64_t ticks_us = g_fake_ticks_us.load(std::memory_order_relaxed);
  int64_t last_us = g_last_ticks_us.load(std::memory_order_relaxed);
  while (ticks_us > last_us) {
    if (g_last_ticks_us.compare_exchange_weak(last_us, ticks_us,
                                              std::memory_order_relaxed)) {
      last_us = ticks_us;
      break;
    }
  }
  return base::TimeTicks() + base::Microseconds(last_us);
}

}  // namespace

class AppPlatformInputMetricsTest : public InProcessBrowserTest {
 public:
  void SetUpInProcessBrowserTestFixture() override {
    create_services_subscription_ =
        BrowserContextDependencyManager::GetInstance()
            ->RegisterCreateServicesCallbackForTesting(
                base::BindRepeating(&AppPlatformInputMetricsTest::
                                        OnWillCreateBrowserContextServices,
                                    base::Unretained(this)));
  }

  void OnWillCreateBrowserContextServices(content::BrowserContext* context) {
    SyncServiceFactory::GetInstance()->SetTestingFactory(
        context, base::BindRepeating([](content::BrowserContext* context)
                                         -> std::unique_ptr<KeyedService> {
          auto sync_service = std::make_unique<FakeSyncService>();
          sync_service->SetSignedIn(signin::ConsentLevel::kSync);
          return sync_service;
        }));
  }

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();
    test_ukm_recorder_ = std::make_unique<ukm::TestAutoSetUkmRecorder>();
  }

  void StartMetricsService() {
    profile()->GetPrefs()->SetInteger(kAppPlatformMetricsDayId, 0);
    profile()->GetPrefs()->SetDict(kAppRunningDuration, base::DictValue());
    profile()->GetPrefs()->SetDict(kAppActivatedCount, base::DictValue());

    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    // Force overwrite and re-create the service to completely bypass and
    // replace the production async posted task from
    // AppServiceProxyAsh::Initialize()!
    auto service = std::make_unique<apps::AppPlatformMetricsService>(profile());
    auto* service_ptr = service.get();
    proxy->SetAppPlatformMetricsServiceForTesting(std::move(service));

    service_ptr->Start(proxy->AppRegistryCache(), proxy->InstanceRegistry(),
                       proxy->AppCapabilityAccessCache());
  }

  AppPlatformInputMetrics* app_platform_input_metrics() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    return proxy->AppPlatformMetricsService()
        ->app_platform_input_metrics_.get();
  }

  AppPlatformMetrics* app_platform_metrics() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    return proxy->AppPlatformMetrics();
  }

  void VerifyAppUsageTimeUkm(const std::string& app_id,
                             base::TimeDelta duration,
                             AppTypeName app_type_name) {
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTime", app_id, duration,
                                     app_type_name);
    VerifyAppUsageTimeUkmWithUkmName("ChromeOSApp.UsageTimeReusedSourceId",
                                     app_id, duration, app_type_name);
  }

  void VerifyAppUsageTimeUkmWithUkmName(const std::string& ukm_name,
                                        const std::string& app_id,
                                        base::TimeDelta duration,
                                        AppTypeName app_type_name) {
    const auto entries = test_ukm_recorder_->GetEntriesByName(ukm_name);
    int usage_time = 0;
    GURL expected_url = AppPlatformMetrics::GetURLForApp(profile(), app_id);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (src == nullptr || src->url() != expected_url) {
        continue;
      }
      usage_time += *(test_ukm_recorder_->GetEntryMetric(entry, "Duration"));
      test_ukm_recorder_->ExpectEntryMetric(entry, "UserDeviceMatrix", 0);
      test_ukm_recorder_->ExpectEntryMetric(entry, "AppType",
                                            (int)app_type_name);
    }
    ASSERT_EQ(usage_time, duration.InMilliseconds());
  }

  void VerifyNoAppUsageTimeUkm() {
    auto entries =
        test_ukm_recorder_->GetEntriesByName("ChromeOSApp.UsageTime");
    ASSERT_EQ(0U, entries.size());
  }

  void VerifyAppUsageTimeUkm(const std::string& app_id,
                             AppTypeName app_type_name) {
    const auto entries =
        test_ukm_recorder_->GetEntriesByName("ChromeOSApp.UsageTime");
    bool found = false;
    GURL expected_url = AppPlatformMetrics::GetURLForApp(profile(), app_id);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (src && src->url() == expected_url) {
        test_ukm_recorder_->ExpectEntryMetric(entry, "AppType",
                                              (int)app_type_name);
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }

  void VerifyInstalledAppsUkm(const std::string& app_id,
                              InstallReason install_reason,
                              InstallSource install_source,
                              InstallTime install_time) {
    const auto entries =
        test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InstalledApp");
    bool found = false;
    GURL expected_url = AppPlatformMetrics::GetURLForApp(profile(), app_id);
    for (const ukm::mojom::UkmEntry* entry : entries) {
      const ukm::UkmSource* src =
          test_ukm_recorder_->GetSourceForSourceId(entry->source_id);
      if (src && src->url() == expected_url) {
        test_ukm_recorder_->ExpectEntryMetric(entry, "InstallReason",
                                              (int)install_reason);
        test_ukm_recorder_->ExpectEntryMetric(entry, "InstallSource2",
                                              (int)install_source);
        test_ukm_recorder_->ExpectEntryMetric(entry, "InstallTime",
                                              (int)install_time);
        found = true;
        break;
      }
    }
    ASSERT_TRUE(found);
  }

  aura::Window* window() { return browser()->GetWindow()->GetNativeWindow(); }

  void CreateInputEvent(InputEventSource event_source,
                        aura::Window* target_window) {
    switch (event_source) {
      case InputEventSource::kUnknown:
        break;
      case InputEventSource::kMouse: {
        ui::MouseEvent mouse_event(ui::EventType::kMouseReleased, gfx::Point(),
                                   gfx::Point(), base::TimeTicks(), 0, 0);
        ui::Event::DispatcherApi(&mouse_event).set_target(target_window);
        app_platform_input_metrics()->OnMouseEvent(&mouse_event);
        break;
      }
      case InputEventSource::kStylus: {
        ui::TouchEvent touch_event(
            ui::EventType::kTouchReleased, gfx::Point(), base::TimeTicks(),
            ui::PointerDetails(ui::EventPointerType::kPen, 0));
        ui::Event::DispatcherApi(&touch_event).set_target(target_window);
        app_platform_input_metrics()->OnTouchEvent(&touch_event);
        break;
      }
      case InputEventSource::kTouch: {
        ui::TouchEvent touch_event(
            ui::EventType::kTouchReleased, gfx::Point(), base::TimeTicks(),
            ui::PointerDetails(ui::EventPointerType::kTouch, 0));
        ui::Event::DispatcherApi(&touch_event).set_target(target_window);
        app_platform_input_metrics()->OnTouchEvent(&touch_event);
        break;
      }
      case InputEventSource::kKeyboard: {
        ui::KeyEvent key_event(ui::EventType::kKeyReleased, ui::VKEY_MENU,
                               ui::EF_ALT_DOWN);
        ui::Event::DispatcherApi(&key_event).set_target(target_window);
        app_platform_input_metrics()->OnKeyEvent(&key_event);
        break;
      }
    }
  }

  void InstallOneApp(const std::string& app_id,
                     AppType app_type,
                     const std::string& publisher_id,
                     Readiness readiness,
                     InstallSource install_source,
                     InstallReason install_reason = InstallReason::kSystem) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = readiness;
    app->publisher_id = publisher_id;
    app->install_reason = install_reason;
    app->install_source = install_source;

    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    proxy->OnApps(std::move(apps), app_type, false);
  }

  void ModifyInstance(const std::string& app_id,
                      aura::Window* window,
                      InstanceState state) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    InstanceParams params(app_id, window);
    params.state = std::make_optional(std::make_pair(state, base::Time::Now()));
    proxy->InstanceRegistry().CreateOrUpdateInstance(std::move(params));
  }

  void VerifyUkm(int count,
                 const std::string& app_info,
                 AppTypeName app_type_name,
                 int event_count,
                 InputEventSource event_source) {
    const auto entries =
        test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
    ASSERT_EQ(count, (int)entries.size());
    const auto* entry = entries[count - 1].get();
    test_ukm_recorder_->ExpectEntrySourceHasUrl(entry, GURL(app_info));
    test_ukm_recorder_->ExpectEntryMetric(entry, "AppType", (int)app_type_name);
    test_ukm_recorder_->ExpectEntryMetric(entry, "AppInputEventCount",
                                          event_count);
    test_ukm_recorder_->ExpectEntryMetric(entry, "AppInputEventSource",
                                          (int)event_source);
  }

  void VerifyNoUkm() {
    auto entries =
        test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
    ASSERT_EQ(0U, entries.size());
  }

  std::unique_ptr<aura::Window> CreateWebAppWindow(aura::Window* parent) {
    auto window = std::make_unique<aura::Window>(nullptr);
    window->Init(ui::LAYER_NOT_DRAWN);
    if (parent) {
      parent->AddChild(window.get());
    }
    return window;
  }

  void TearDownOnMainThread() override {
    test_ukm_recorder_.reset();
    InProcessBrowserTest::TearDownOnMainThread();
  }

  Profile* profile() { return ProfileManager::GetPrimaryUserProfile(); }

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::CallbackListSubscription create_services_subscription_;

  void TriggerCheckForFiveMinutes(AppPlatformMetricsService* service) {
    service->CheckForFiveMinutes();
  }
  void TriggerCheckForNewDay(AppPlatformMetricsService* service) {
    service->CheckForNewDay();
  }
  void TriggerCheckForNoisyAppKMReportingInterval(
      AppPlatformMetricsService* service) {
    service->CheckForNoisyAppKMReportingInterval();
  }
};

class ManagedGuestSessionMetricsTest : public AppPlatformInputMetricsTest {
 public:
  void SetUpOnMainThread() override {
    // 1. Run base setup (InProcessBrowserTest handles its own login)
    AppPlatformInputMetricsTest::SetUpOnMainThread();
    // 2. Simulate Managed Guest Session AFTER InProcessBrowserTest setup to
    // ensure it is not overwritten!
    mgs_state_ = std::make_unique<ash::ScopedTestPublicSessionLoginState>();
    // 3. Now start the metrics service, so it sees the MGS state correctly!
    StartMetricsService();
  }

  void TearDownOnMainThread() override {
    // 1. Purge and destroy the custom testing metrics service early. This
    // invokes their destructors and safely unregisters the UkmRecorder
    // observers while MGS is still active!
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    if (proxy) {
      proxy->SetAppPlatformMetricsServiceForTesting(nullptr);
    }

    // 2. Now it is 100% safe to destroy the mock Managed Guest Session login
    // state!
    mgs_state_.reset();
    AppPlatformInputMetricsTest::TearDownOnMainThread();
  }

 protected:
  std::unique_ptr<ash::ScopedTestPublicSessionLoginState> mgs_state_;
};

IN_PROC_BROWSER_TEST_F(AppPlatformInputMetricsTest,
                       InputEventsOnBrowserWindow) {
  StartMetricsService();
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  InstallOneApp(kWebAppId1, AppType::kWeb, "https://foo.com/",
                Readiness::kReady, InstallSource::kSystem);
  InstallOneApp(kWebAppId2, AppType::kWeb, "https://foo2.com/",
                Readiness::kReady, InstallSource::kSystem);

  auto chrome_window = CreateWebAppWindow(window());
  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse, chrome_window.get());

  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();

  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm(1, std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser, /*event_count=*/1,
            InputEventSource::kMouse);

  // Create a web app tab1.
  const GURL url1 = GURL("https://foo.com");
  auto web_app_window1 = CreateWebAppWindow(window());

  // Set the web app tab1 as activated.
  ModifyInstance(kWebAppId1, web_app_window1.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse, web_app_window1.get());
  app_platform_input_metrics()->OnFiveMinutes();

  auto entries = test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(1U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 2 input metrics events are recorded.
  VerifyUkm(2, std::string("app://") + kWebAppId1, AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kMouse);

  // Create a web app tab2.
  const GURL url2 = GURL("https://foo2.com");
  auto web_app_window2 = CreateWebAppWindow(window());

  // Set the web app tab2 as activated.
  ModifyInstance(kWebAppId2, web_app_window2.get(), kActiveInstanceState);
  ModifyInstance(kWebAppId1, web_app_window1.get(), kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus, web_app_window2.get());
  CreateInputEvent(InputEventSource::kStylus, web_app_window2.get());
  app_platform_input_metrics()->OnFiveMinutes();

  entries = test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(2U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 3 input metrics events are recorded.
  VerifyUkm(3, std::string("app://") + kWebAppId2, AppTypeName::kChromeBrowser,
            /*event_count=*/2, InputEventSource::kStylus);

  // Set the web app tab2 as destroyed, and web app tab1 as activated.
  ModifyInstance(kWebAppId2, web_app_window2.get(),
                 apps::InstanceState::kDestroyed);
  ModifyInstance(kWebAppId1, web_app_window1.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kKeyboard, web_app_window1.get());
  app_platform_input_metrics()->OnFiveMinutes();

  entries = test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(3U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 4 input metrics events are recorded.
  VerifyUkm(4, std::string("app://") + kWebAppId1, AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kKeyboard);

  // Set the web app tab1 as inactivated.
  ModifyInstance(kWebAppId1, web_app_window1.get(), kInactiveInstanceState);
  CreateInputEvent(InputEventSource::kStylus, chrome_window.get());
  app_platform_input_metrics()->OnFiveMinutes();

  entries = test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent");
  ASSERT_EQ(4U, entries.size());

  app_platform_input_metrics()->OnTwoHours();
  // Verify 5 input metrics events are recorded.
  VerifyUkm(5, std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kStylus);

  // Cleanup instances to avoid crash on teardown!
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
  ModifyInstance(kWebAppId1, web_app_window1.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       InputEventUkmReportedAfter2Hours) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto chrome_window = CreateWebAppWindow(window());
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kTouch, chrome_window.get());

  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();

  // Set time passed 2 hours to record the usage time AppKM.
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm(1, std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser, /*event_count=*/1,
            InputEventSource::kTouch);

  // Cleanup
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       InputEventUkmReportedOnShutdown) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto chrome_window = CreateWebAppWindow(window());
  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);
  CreateInputEvent(InputEventSource::kTouch, chrome_window.get());

  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();

  // Simulate shutdown by notifying observers. This will now safely trigger
  // OnStartingShutdown because our inline service was correctly registered.
  ukm::UkmRecorder::Get()->NotifyStartShutdown();

  // Wait for the async UKM entry to propagate to the test recorder.
  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_ukm_recorder_->GetEntriesByName("ChromeOSApp.InputEvent")
               .size() == 1U;
  }));

  VerifyUkm(1, std::string("app://") + app_constants::kChromeAppId,
            AppTypeName::kChromeBrowser, /*event_count=*/1,
            InputEventSource::kTouch);

  // Cleanup
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       InputEventUkmForPolicyInstalledApp) {
  InstallOneApp("p", AppType::kWeb, "https://policy.com/", Readiness::kReady,
                InstallSource::kSystem, InstallReason::kPolicy);
  auto app_window = CreateWebAppWindow(window());

  // Set the browser window as activated.
  ModifyInstance("p", app_window.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse, app_window.get());
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();

  // Set time passed 2 hours to record the usage time AppKM.
  app_platform_input_metrics()->OnTwoHours();
  VerifyUkm(1, "https://policy.com/", AppTypeName::kChromeBrowser,
            /*event_count=*/1, InputEventSource::kMouse);

  // Cleanup
  ModifyInstance("p", app_window.get(), apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       DoNotReportInputEventUkmForUserInstalledApps) {
  InstallOneApp("u", AppType::kWeb, "https://user.com/", Readiness::kReady,
                InstallSource::kSystem, InstallReason::kUser);
  auto app_window = CreateWebAppWindow(window());

  // Set the browser window as activated.
  ModifyInstance("u", app_window.get(), kActiveInstanceState);
  CreateInputEvent(InputEventSource::kMouse, app_window.get());
  app_platform_input_metrics()->OnFiveMinutes();
  VerifyNoUkm();

  // Set time passed 2 hours to record the usage time AppKM.
  app_platform_input_metrics()->OnTwoHours();
  VerifyNoUkm();

  // Cleanup
  ModifyInstance("u", app_window.get(), apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       ReportsUsageTimeUkmAfter2Hours) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto chrome_window = CreateWebAppWindow(window());
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);

  app_platform_metrics()->OnFiveMinutes();
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kInactiveInstanceState);
  VerifyNoAppUsageTimeUkm();

  app_platform_metrics()->OnTwoHours();
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId,
                        AppTypeName::kChromeBrowser);

  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       UsageTimeUkmReportedOnShutdown) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto chrome_window = CreateWebAppWindow(window());
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);

  app_platform_metrics()->OnFiveMinutes();
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kInactiveInstanceState);
  VerifyNoAppUsageTimeUkm();

  ukm::UkmRecorder::Get()->NotifyStartShutdown();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_ukm_recorder_->GetEntriesByName("ChromeOSApp.UsageTime")
               .size() == 1U;
  }));

  VerifyAppUsageTimeUkm(app_constants::kChromeAppId,
                        AppTypeName::kChromeBrowser);

  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       UsageTimeUkmReportedInShortSessions) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);
  auto chrome_window = CreateWebAppWindow(window());
  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kActiveInstanceState);

  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 kInactiveInstanceState);
  VerifyNoAppUsageTimeUkm();

  ukm::UkmRecorder::Get()->NotifyStartShutdown();

  EXPECT_TRUE(base::test::RunUntil([&]() {
    return test_ukm_recorder_->GetEntriesByName("ChromeOSApp.UsageTime")
               .size() == 1U;
  }));

  VerifyAppUsageTimeUkm(app_constants::kChromeAppId,
                        AppTypeName::kChromeBrowser);

  ModifyInstance(app_constants::kChromeAppId, chrome_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       DoNotReportUsageTimeUkmForBlockedInstalledReasons) {
  for (InstallReason reason : {InstallReason::kUser}) {
    InstallOneApp("blocked_app", AppType::kWeb, "https://blocked.com/",
                  Readiness::kReady, InstallSource::kSystem, reason);
    auto app_window = CreateWebAppWindow(nullptr);
    ModifyInstance("blocked_app", app_window.get(), kActiveInstanceState);

    app_platform_metrics()->OnFiveMinutes();
    app_platform_metrics()->OnTwoHours();
    VerifyNoAppUsageTimeUkm();

    ModifyInstance("blocked_app", app_window.get(),
                   apps::InstanceState::kDestroyed);
  }
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       ReportsUsageTimeUkmForPolicyInstalledReasons) {
  InstallOneApp("policy_app", AppType::kWeb, "https://policy.com/",
                Readiness::kReady, InstallSource::kSystem,
                InstallReason::kPolicy);
  auto app_window = CreateWebAppWindow(nullptr);
  ModifyInstance("policy_app", app_window.get(), kActiveInstanceState);

  app_platform_metrics()->OnFiveMinutes();
  app_platform_metrics()->OnTwoHours();
  VerifyAppUsageTimeUkm("policy_app", AppTypeName::kWeb);

  ModifyInstance("policy_app", app_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       ReportsUsageTimeUkmForOemInstalledReasons) {
  InstallOneApp("oem_app", AppType::kWeb, "https://oem.com/", Readiness::kReady,
                InstallSource::kSystem, InstallReason::kOem);
  auto app_window = CreateWebAppWindow(nullptr);
  ModifyInstance("oem_app", app_window.get(), kActiveInstanceState);

  app_platform_metrics()->OnFiveMinutes();
  app_platform_metrics()->OnTwoHours();
  VerifyAppUsageTimeUkm("oem_app", AppTypeName::kWeb);

  ModifyInstance("oem_app", app_window.get(), apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       ReportsUsageTimeUkmForDefaultInstalledReasons) {
  InstallOneApp("default_app", AppType::kWeb, "https://default.com/",
                Readiness::kReady, InstallSource::kSystem,
                InstallReason::kDefault);
  auto app_window = CreateWebAppWindow(nullptr);
  ModifyInstance("default_app", app_window.get(), kActiveInstanceState);

  app_platform_metrics()->OnFiveMinutes();
  app_platform_metrics()->OnTwoHours();
  VerifyAppUsageTimeUkm("default_app", AppTypeName::kWeb);

  ModifyInstance("default_app", app_window.get(),
                 apps::InstanceState::kDestroyed);
}

IN_PROC_BROWSER_TEST_F(ManagedGuestSessionMetricsTest,
                       InstalledAppsUkmReportedOnlyForAllowedInstallReasons) {
  InstallOneApp("a", AppType::kWeb, "https://allowed.com/", Readiness::kReady,
                InstallSource::kSystem, InstallReason::kPolicy);

  VerifyInstalledAppsUkm("a", InstallReason::kPolicy, InstallSource::kSystem,
                         InstallTime::kRunning);
}

class AppPlatformMetricsServiceBrowserTest
    : public AppPlatformInputMetricsTest {
 public:
  using AppPlatformInputMetricsTest::VerifyAppUsageTimeUkm;
  using AppPlatformInputMetricsTest::VerifyAppUsageTimeUkmWithUkmName;

  AppPlatformMetricsServiceBrowserTest() {
    set_open_about_blank_on_browser_launch(false);
    set_exit_when_last_browser_closes(false);
    histogram_tester_ = std::make_unique<base::HistogramTester>();
  }
  ~AppPlatformMetricsServiceBrowserTest() override = default;

  void SetUpOnMainThread() override {
    base::SequenceCheckerImpl::EnableStackLogging();
    AppPlatformInputMetricsTest::SetUpOnMainThread();

    // Close the default browser created by InProcessBrowserTest.
    if (browser()) {
      CloseBrowserSynchronously(browser());
      SetBrowser(nullptr);
    }

    base::Time fake_time;
    constexpr char kFakeNowTimeString[] = "Sunday, 5 June 2022 14:30:00 CDT";
    ASSERT_TRUE(base::Time::FromString(kFakeNowTimeString, &fake_time));
    g_fake_time_us.store(fake_time.ToDeltaSinceWindowsEpoch().InMicroseconds(),
                         std::memory_order_relaxed);

    base::TimeTicks fake_ticks = base::TimeTicks::Now();
    g_fake_ticks_us.store(fake_ticks.since_origin().InMicroseconds(),
                          std::memory_order_relaxed);
    time_override_ = std::make_unique<base::subtle::ScopedTimeClockOverrides>(
        &GetFakeTime, &GetFakeTimeTicks, nullptr);
    policy::PolicyLogger::GetInstance()->ResetLoggerForTesting();
    StartMetricsService();
    histogram_tester_ = std::make_unique<base::HistogramTester>();

    content::RunAllTasksUntilIdle();
    if (auto* swa_manager = ash::SystemWebAppManager::Get(profile())) {
      swa_manager->StopBackgroundTasksForTesting();
    }
  }

  void TearDownOnMainThread() override {
    base::ThreadPoolInstance::Get()->FlushForTesting();
    time_override_.release();
    AppPlatformInputMetricsTest::TearDownOnMainThread();
  }

  ukm::TestAutoSetUkmRecorder* test_ukm_recorder() {
    return test_ukm_recorder_.get();
  }

  void InstallOneApp(const std::string& app_id,
                     AppType app_type,
                     const std::string& publisher_id,
                     Readiness readiness,
                     InstallSource install_source,
                     bool is_platform_app = false,
                     WindowMode window_mode = WindowMode::kUnknown,
                     InstallReason install_reason = InstallReason::kUser) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(profile());
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = readiness;
    app->publisher_id = publisher_id;
    app->install_reason = install_reason;
    app->install_source = install_source;
    app->is_platform_app = is_platform_app;
    app->window_mode = window_mode;

    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    proxy->OnApps(std::move(apps), app_type, false);
  }

  Browser* CreateBrowserWithAuraWindow() {
    Browser* browser = CreateBrowser(profile());
    content::WaitForLoadStop(
        browser->tab_strip_model()->GetActiveWebContents());
    return browser;
  }

  Browser* CreateBrowserWindow(
      InstallReason install_reason = InstallReason::kUser) {
    InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                  Readiness::kReady, InstallSource::kSystem,
                  /*is_platform_app=*/false, WindowMode::kUnknown,
                  install_reason);
    Browser* browser = CreateBrowserWithAuraWindow();
    SetBrowser(browser);
    EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());
    return browser;
  }

  void ModifyWebAppInstance(const std::string& app_id,
                            aura::Window* window,
                            InstanceState state) {
    ModifyInstance(app_id, window, state);
  }

  void VerifyAppUsageTimeCountHistogram(
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        expected_count);
  }

  void VerifyAppUsageTimeCountHistogram(
      base::HistogramBase::Count32 expected_count,
      AppTypeNameV2 app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        expected_count);
  }

  void VerifyAppUsageTimeHistogram(base::TimeDelta time_delta,
                                   base::HistogramBase::Count32 expected_count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppUsageTimeHistogram(base::TimeDelta time_delta,
                                   base::HistogramBase::Count32 expected_count,
                                   AppTypeNameV2 app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsUsageTimeHistogramNameForTest(app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppUsageTimeUkmWithUkmName(const std::string& ukm_name,
                                        const GURL& url,
                                        base::TimeDelta duration,
                                        AppTypeName app_type_name) {
    const auto entries = test_ukm_recorder()->GetEntriesByName(ukm_name);
    int usage_time = 0;
    for (const ukm::mojom::UkmEntry* entry : entries) {
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
    ASSERT_EQ(usage_time, duration.InMilliseconds());
  }

  void VerifyAppUsageTimeUkm(const GURL& url,
                             base::TimeDelta duration,
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

  void ResetAppPlatformMetricsService() { StartMetricsService(); }

  base::HistogramTester& histogram_tester() { return *histogram_tester_; }
  FakeSyncService* sync_service() {
    return static_cast<FakeSyncService*>(
        SyncServiceFactory::GetForProfile(profile()));
  }

  void FastForwardBy(base::TimeDelta delta) {
    content::RunAllTasksUntilIdle();
    base::TimeDelta remaining = delta;
    auto* service = AppServiceProxyFactory::GetForProfile(profile())
                        ->AppPlatformMetricsService();
    while (remaining > base::TimeDelta()) {
      base::TimeDelta time_to_five_mins =
          base::Minutes(5) - accumulated_five_minutes_time_;
      base::TimeDelta time_to_two_hours =
          base::Hours(2) - accumulated_two_hours_time_;
      base::TimeDelta step =
          std::min({remaining, time_to_five_mins, time_to_two_hours});
      g_fake_time_us.fetch_add(step.InMicroseconds(),
                               std::memory_order_relaxed);
      g_fake_ticks_us.fetch_add(step.InMicroseconds(),
                                std::memory_order_relaxed);
      remaining -= step;

      accumulated_five_minutes_time_ += step;
      accumulated_two_hours_time_ += step;

      // Pump the loop to let tasks run at this new time step.
      content::RunAllTasksUntilIdle();

      if (accumulated_five_minutes_time_ >= base::Minutes(5)) {
        TriggerCheckForNewDay(service);
        accumulated_five_minutes_time_ = base::TimeDelta();
      }

      if (accumulated_two_hours_time_ >= base::Hours(2)) {
        accumulated_two_hours_time_ = base::TimeDelta();
      }

      // Pump again in case the checks posted more tasks.
      content::RunAllTasksUntilIdle();
    }
  }

  PrefService* GetPrefService() { return profile()->GetPrefs(); }

  void VerifyAppRunningDuration(const base::TimeDelta time_delta,
                                AppTypeName app_type_name) {
    const base::DictValue& dict =
        GetPrefService()->GetDict(kAppRunningDuration);
    std::string key = GetAppTypeHistogramName(app_type_name);

    std::optional<base::TimeDelta> unreported_duration =
        base::ValueToTimeDelta(dict.FindByDottedPath(key));
    if (time_delta.is_zero()) {
      EXPECT_FALSE(unreported_duration.has_value());
      return;
    }

    ASSERT_TRUE(unreported_duration.has_value());
    EXPECT_EQ(time_delta, unreported_duration.value());
  }

  void VerifyAppRunningDurationCountHistogram(
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        expected_count);
  }

  void VerifyAppRunningDurationHistogram(
      base::TimeDelta time_delta,
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTimeBucketCount(
        AppPlatformMetrics::GetAppsRunningDurationHistogramNameForTest(
            app_type_name),
        time_delta, expected_count);
  }

  void VerifyAppRunningPercentageCountHistogram(
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
            app_type_name),
        expected_count);
  }

  void VerifyAppRunningPercentageHistogram(
      int count,
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(
        AppPlatformMetrics::GetAppsRunningPercentageHistogramNameForTest(
            app_type_name),
        count, expected_count);
  }

  void VerifyAppActivatedCount(int expected_count, AppTypeName app_type_name) {
    const base::DictValue& dict = GetPrefService()->GetDict(kAppActivatedCount);
    std::string key = GetAppTypeHistogramName(app_type_name);

    std::optional<int> activated_count = dict.FindIntByDottedPath(key);
    if (expected_count == 0) {
      EXPECT_FALSE(activated_count.has_value());
      return;
    }

    ASSERT_TRUE(activated_count.has_value());
    EXPECT_EQ(expected_count, activated_count.value());
  }

  void VerifyAppActivatedCountHistogram(
      base::HistogramBase::Count32 expected_count,
      AppTypeName app_type_name) {
    histogram_tester().ExpectTotalCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        expected_count);
  }

  void VerifyAppActivatedHistogram(int count,
                                   base::HistogramBase::Count32 expected_count,
                                   AppTypeName app_type_name) {
    histogram_tester().ExpectBucketCount(
        AppPlatformMetrics::GetAppsActivatedCountHistogramNameForTest(
            app_type_name),
        count, expected_count);
  }

 private:
  std::unique_ptr<base::HistogramTester> histogram_tester_;
  std::unique_ptr<base::subtle::ScopedTimeClockOverrides> time_override_;
  base::TimeDelta accumulated_five_minutes_time_;
  base::TimeDelta accumulated_two_hours_time_;
};

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTime DISABLED_UsageTime
#else
#define MAYBE_UsageTime UsageTime
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest, MAYBE_UsageTime) {
  // Create an ARC app window.
  std::string app_id = "aa";
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);

  FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1, AppTypeName::kArc);
  VerifyAppUsageTimeCountHistogram(/*expected_count=*/1, AppTypeNameV2::kArc);
  VerifyAppUsageTimeHistogram(base::Minutes(5),
                              /*expected_count=*/1, AppTypeName::kArc);

  FastForwardBy(base::Minutes(2));
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  Browser* browser = CreateBrowserWindow();

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  FastForwardBy(base::Minutes(3));
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

  FastForwardBy(base::Minutes(15));
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

  // Set time passed 2 hours to record the usage time AppKM.
  FastForwardBy(base::Minutes(95));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(18),
                        AppTypeName::kChromeBrowser);
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkm DISABLED_UsageTimeUkm
#else
#define MAYBE_UsageTimeUkm UsageTimeUkm
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_UsageTimeUkm) {
  Browser* browser = CreateBrowserWindow();

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  sync_service()->SetAllowedByEnterprisePolicy(false);

  // Fast forward by 2 hours and verify no usage data is reported to UKM.
  FastForwardBy(base::Hours(2));
  VerifyNoAppUsageTimeUkm();

  sync_service()->SetAllowedByEnterprisePolicy(true);

  static constexpr base::TimeDelta kAppUsageDuration = base::Hours(1);
  FastForwardBy(kAppUsageDuration);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Fast forward by 2 hours and verify usage data reported to UKM only includes
  // usage data since sync was last enabled.
  FastForwardBy(base::Hours(2));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, kAppUsageDuration,
                        AppTypeName::kChromeBrowser);
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkmReportAfterReboot \
  DISABLED_UsageTimeUkmReportAfterReboot
#else
#define MAYBE_UsageTimeUkmReportAfterReboot UsageTimeUkmReportAfterReboot
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_UsageTimeUkmReportAfterReboot) {
  Browser* browser = CreateBrowserWindow();
  InstallOneApp(kWebAppId1, AppType::kWeb, "https://foo.com/",
                Readiness::kReady, InstallSource::kSystem);

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  FastForwardBy(base::Minutes(30));

  // Create a web app tab.
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(), kActiveInstanceState);

  FastForwardBy(base::Minutes(20));
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       kInactiveInstanceState);

  VerifyNoAppUsageTimeUkm();

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // AppKM is restored from the user pref and reported after 5 minutes after
  // reboot.
  ResetAppPlatformMetricsService();
  VerifyNoAppUsageTimeUkm();

  FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(30),
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, base::Minutes(20), AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  FastForwardBy(base::Minutes(10));
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Verify UKM is not reported.
  VerifyAppUsageTimeUkm(/*count=*/2);

  // Reset PlatformMetricsService to simulate the system reboot, and verify
  // only the new AppKM is reported.
  ResetAppPlatformMetricsService();
  FastForwardBy(base::Minutes(5));

  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(40),
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, base::Minutes(20), AppTypeName::kChromeBrowser);

  // Reset PlatformMetricsService to simulate the system reboot, and verify no
  // more AppKM is reported.
  ResetAppPlatformMetricsService();
  FastForwardBy(base::Minutes(5));
  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(40),
                        AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url, base::Minutes(20), AppTypeName::kChromeBrowser);
  web_app_window.reset();
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkmWithMultipleWindows \
  DISABLED_UsageTimeUkmWithMultipleWindows
#else
#define MAYBE_UsageTimeUkmWithMultipleWindows UsageTimeUkmWithMultipleWindows
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_UsageTimeUkmWithMultipleWindows) {
  Browser* browser1 = CreateBrowserWithAuraWindow();

  // Set the browser window active.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kActiveInstanceState);
  FastForwardBy(base::Minutes(5));

  // Set the browser window inactive.
  ModifyInstance(app_constants::kChromeAppId,
                 browser1->window()->GetNativeWindow(), kInactiveInstanceState);
  FastForwardBy(base::Minutes(1));

  Browser* browser2 = CreateBrowserWithAuraWindow();
  ModifyInstance(app_constants::kChromeAppId,
                 browser2->window()->GetNativeWindow(), kActiveInstanceState);
  FastForwardBy(base::Minutes(7));

  // Close windows.
  CloseBrowserSynchronously(browser1);
  CloseBrowserSynchronously(browser2);

  VerifyNoAppUsageTimeUkm();

  // Verify UKM is reported after 2 hours.
  FastForwardBy(base::Minutes(107));
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(12),
                        AppTypeName::kChromeBrowser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser \
  DISABLED_UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser
#else
#define MAYBE_UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser \
  UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser
#endif
IN_PROC_BROWSER_TEST_F(
    AppPlatformMetricsServiceBrowserTest,
    MAYBE_UsageTimeUkmForWebAppOpenInTabWithInactivatedBrowser) {
  Browser* browser = CreateBrowserWindow();
  InstallOneApp(kWebAppId1, AppType::kWeb, "https://foo.com/",
                Readiness::kReady, InstallSource::kSystem);

  // Create a web app tab.
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Set the web app tab as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(), kActiveInstanceState);

  FastForwardBy(base::Minutes(5));
  VerifyNoAppUsageTimeUkm();

  // Set the browser window and web app tabs as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       kInactiveInstanceState);
  FastForwardBy(base::Minutes(2));

  // Set the web app tab as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(), kActiveInstanceState);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  FastForwardBy(base::Minutes(3));
  VerifyNoAppUsageTimeUkm();

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       kInactiveInstanceState);
  FastForwardBy(base::Minutes(1));

  // Set the web app tab as destroyed.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       apps::InstanceState::kDestroyed);

  // Set the browser window as destroyed.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);
  VerifyNoAppUsageTimeUkm();

  FastForwardBy(base::Minutes(109));

  // Verify the app usage time AppKM for the web app and browser window.
  VerifyAppUsageTimeUkm(/*count=*/2);
  VerifyAppUsageTimeUkm(url, base::Minutes(8), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(1),
                        AppTypeName::kChromeBrowser);
  web_app_window.reset();
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser \
  DISABLED_UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser
#else
#define MAYBE_UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser \
  UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser
#endif
IN_PROC_BROWSER_TEST_F(
    AppPlatformMetricsServiceBrowserTest,
    MAYBE_UsageTimeUkmForWebAppOpenInTabWithActivatedBrowser) {
  Browser* browser = CreateBrowserWindow();
  InstallOneApp(kWebAppId1, AppType::kWeb, "https://foo.com/",
                Readiness::kReady, InstallSource::kSystem);

  // Create a web app tab.
  const GURL url = GURL("https://foo.com");
  auto web_app_window =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(), kActiveInstanceState);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  FastForwardBy(base::Minutes(5));
  VerifyNoAppUsageTimeUkm();

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       kInactiveInstanceState);
  FastForwardBy(base::Minutes(3));

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);
  VerifyNoAppUsageTimeUkm();
  FastForwardBy(base::Minutes(112));

  // Verify the app usage time AppKM.
  VerifyAppUsageTimeUkm(/*count=*/2);
  VerifyAppUsageTimeUkm(url, base::Minutes(5), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(3),
                        AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  // Set the web app tab as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(), kActiveInstanceState);
  FastForwardBy(base::Minutes(2));

  // Set the browser window as inactivated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Verify no more app usage time AppKM is recorded.
  VerifyAppUsageTimeUkm(/*count=*/2);

  // Set the web app tab as inactivated.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       kInactiveInstanceState);

  FastForwardBy(base::Minutes(118));

  // Verify only the web app UKM is reported.
  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(url, base::Minutes(7), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(3),
                        AppTypeName::kChromeBrowser);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);
  FastForwardBy(base::Minutes(1));

  // Set the browser window as destroyed.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  // Set the web app tab as destroyed.
  ModifyWebAppInstance(kWebAppId1, web_app_window.get(),
                       apps::InstanceState::kDestroyed);

  // Verify no more app usage time AppKM is recorded.
  VerifyAppUsageTimeUkm(/*count=*/3);

  FastForwardBy(base::Minutes(119));

  VerifyAppUsageTimeUkm(/*count=*/4);
  VerifyAppUsageTimeUkm(url, base::Minutes(7), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(4),
                        AppTypeName::kChromeBrowser);

  web_app_window.reset();
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_UsageTimeUkmForMultipleWebAppOpenInTab \
  DISABLED_UsageTimeUkmForMultipleWebAppOpenInTab
#else
#define MAYBE_UsageTimeUkmForMultipleWebAppOpenInTab \
  UsageTimeUkmForMultipleWebAppOpenInTab
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_UsageTimeUkmForMultipleWebAppOpenInTab) {
  Browser* browser = CreateBrowserWindow();
  InstallOneApp(kWebAppId1, AppType::kWeb, "https://foo.com/",
                Readiness::kReady, InstallSource::kSystem);
  InstallOneApp(kWebAppId2, AppType::kWeb, "https://foo2.com/",
                Readiness::kReady, InstallSource::kSystem);

  // Create web app tabs.
  const GURL url1 = GURL("https://foo.com");
  auto web_app_window1 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());
  const GURL url2 = GURL("https://foo2.com");
  auto web_app_window2 =
      CreateWebAppWindow(browser->window()->GetNativeWindow());

  // Set the web app tab 1 as activated.
  ModifyWebAppInstance(kWebAppId1, web_app_window1.get(), kActiveInstanceState);
  ModifyWebAppInstance(kWebAppId2, web_app_window2.get(),
                       kInactiveInstanceState);

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kActiveInstanceState);

  FastForwardBy(base::Minutes(5));

  // Set the web app tab 2 as activated.
  ModifyWebAppInstance(kWebAppId2, web_app_window2.get(), kActiveInstanceState);
  ModifyWebAppInstance(kWebAppId1, web_app_window1.get(),
                       kInactiveInstanceState);
  FastForwardBy(base::Minutes(4));

  // Set the web app tabs as inactivated.
  ModifyWebAppInstance(kWebAppId1, web_app_window1.get(),
                       kInactiveInstanceState);
  ModifyWebAppInstance(kWebAppId2, web_app_window2.get(),
                       kInactiveInstanceState);

  FastForwardBy(base::Minutes(3));
  VerifyNoAppUsageTimeUkm();

  // Set the browser window as activated.
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(), kInactiveInstanceState);

  // Destroy the browser windows, and web app tabs.
  ModifyWebAppInstance(kWebAppId1, web_app_window1.get(),
                       apps::InstanceState::kDestroyed);
  ModifyWebAppInstance(kWebAppId2, web_app_window2.get(),
                       apps::InstanceState::kDestroyed);
  ModifyInstance(app_constants::kChromeAppId,
                 browser->window()->GetNativeWindow(),
                 apps::InstanceState::kDestroyed);

  FastForwardBy(base::Minutes(108));

  // Verify the app usage time AppKM for the web apps and browser window.
  VerifyAppUsageTimeUkm(/*count=*/3);
  VerifyAppUsageTimeUkm(url1, base::Minutes(5), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(url2, base::Minutes(4), AppTypeName::kChromeBrowser);
  VerifyAppUsageTimeUkm(app_constants::kChromeAppId, base::Minutes(3),
                        AppTypeName::kChromeBrowser);

  web_app_window1.reset();
  web_app_window2.reset();
  CloseBrowserSynchronously(browser);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_BrowserWindow DISABLED_BrowserWindow
#else
#define MAYBE_BrowserWindow BrowserWindow
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_BrowserWindow) {
  InstallOneApp(app_constants::kChromeAppId, AppType::kChromeApp, "Chrome",
                Readiness::kReady, InstallSource::kSystem);

  // Expect no Browsers at the beginning.
  EXPECT_EQ(0U, GlobalBrowserCollection::GetInstance()->GetSize());
  Browser* browser1 = CreateBrowserWithAuraWindow();

  EXPECT_EQ(1U, GlobalBrowserCollection::GetInstance()->GetSize());

  // browser1 is active by default.
  // Note: expected_count is 2 because in a real Ash/Aura environment, creating
  // a window triggers two natural focus activation transitions (first when the
  // window is initialized and mapped, and again once focus settles on the
  // default tab's web contents). Both of these activation updates increment
  // the metrics count.
  FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/2, AppTypeName::kChromeBrowser);

  FastForwardBy(base::Minutes(20));
  // Set the browser window running in the background by minimizing it.
  browser1->window()->Minimize();

  FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Minutes(30), AppTypeName::kChromeBrowser);

  // Test multiple browsers.
  Browser* browser2 = CreateBrowserWithAuraWindow();
  EXPECT_EQ(2U, GlobalBrowserCollection::GetInstance()->GetSize());

  // browser2 is active by default.
  // Note: expected_count is 4 because launching browser2 triggers two more
  // focus activations during its real startup lifecycle, adding 2 to the
  // previous count of 2.
  FastForwardBy(base::Minutes(10));
  VerifyAppActivatedCount(/*expected_count=*/4, AppTypeName::kChromeBrowser);

  FastForwardBy(base::Minutes(20));
  CloseBrowserSynchronously(browser2);

  FastForwardBy(base::Minutes(10));
  VerifyAppRunningDuration(base::Hours(1), AppTypeName::kChromeBrowser);

  // Test date change.
  FastForwardBy(base::Days(1));
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
  VerifyAppActivatedHistogram(/*count=*/4, /*expected_count=*/1,
                              AppTypeName::kChromeBrowser);

  CloseBrowserSynchronously(browser1);
}

// TODO(crbug.com/521490538): Fix memory leaks and re-enable the tests.
#if defined(LEAK_SANITIZER)
#define MAYBE_AppRunningPercentage DISABLED_AppRunningPercentage
#else
#define MAYBE_AppRunningPercentage AppRunningPercentage
#endif
IN_PROC_BROWSER_TEST_F(AppPlatformMetricsServiceBrowserTest,
                       MAYBE_AppRunningPercentage) {
  Browser* browser = CreateBrowserWindow();

  // Test one Chrome browser.
  // browser is active by default.
  FastForwardBy(base::Hours(1));

  // Set the browser window running in the background by minimizing it.
  browser->window()->Minimize();

  // Test one ARC app.
  std::string app_id = "aa";
  InstallOneApp(app_id, AppType::kArc, "com.google.AA", Readiness::kReady,
                InstallSource::kPlayStore);
  auto window = std::make_unique<aura::Window>(nullptr);
  window->Init(ui::LAYER_NOT_DRAWN);
  ModifyInstance(app_id, window.get(), apps::InstanceState::kActive);
  FastForwardBy(base::Hours(1));

  // Inactive ARC app.
  ModifyInstance(app_id, window.get(), kInactiveInstanceState);

  // Test date change.
  FastForwardBy(base::Days(1));

  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kChromeBrowser);
  VerifyAppRunningPercentageHistogram(50,
                                      /*expected_count=*/1,
                                      AppTypeName::kChromeBrowser);

  VerifyAppRunningPercentageCountHistogram(/*expected_count=*/1,
                                           AppTypeName::kArc);
  VerifyAppRunningPercentageHistogram(50,
                                      /*expected_count=*/1, AppTypeName::kArc);

  CloseBrowserSynchronously(browser);
}

}  // namespace apps
