// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_service.h"

#include <memory>
#include <string>
#include <vector>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics.h"
#include "chrome/browser/apps/app_service/metrics/app_platform_metrics_utils.h"
#include "chrome/browser/sync/sync_service_factory.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/app_constants/constants.h"
#include "components/keyed_service/content/browser_context_dependency_manager.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/signin/public/base/consent_level.h"
#include "components/sync/test/test_sync_service.h"
#include "components/ukm/test_ukm_recorder.h"
#include "content/public/test/browser_test.h"
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

    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    if (!proxy->AppPlatformMetricsService()->AppPlatformMetrics()) {
      proxy->AppPlatformMetricsService()->Start(
          proxy->AppRegistryCache(), proxy->InstanceRegistry(),
          proxy->AppCapabilityAccessCache());
    }
  }

  AppPlatformInputMetrics* app_platform_input_metrics() {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    return proxy->AppPlatformMetricsService()
        ->app_platform_input_metrics_.get();
  }

  aura::Window* window() { return browser()->window()->GetNativeWindow(); }

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
                     InstallSource install_source) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
    AppPtr app = std::make_unique<App>(app_type, app_id);
    app->readiness = readiness;
    app->publisher_id = publisher_id;
    app->install_reason = InstallReason::kSystem;
    app->install_source = install_source;

    std::vector<AppPtr> apps;
    apps.push_back(std::move(app));
    proxy->OnApps(std::move(apps), app_type, false);
  }

  void ModifyInstance(const std::string& app_id,
                      aura::Window* window,
                      InstanceState state) {
    auto* proxy = AppServiceProxyFactory::GetForProfile(browser()->profile());
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

 protected:
  std::unique_ptr<ukm::TestAutoSetUkmRecorder> test_ukm_recorder_;
  base::CallbackListSubscription create_services_subscription_;
};

IN_PROC_BROWSER_TEST_F(AppPlatformInputMetricsTest,
                       InputEventsOnBrowserWindow) {
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

}  // namespace apps
