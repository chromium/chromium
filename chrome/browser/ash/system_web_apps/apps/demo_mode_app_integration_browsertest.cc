// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/shell.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/files/file_util.h"
#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker_impl.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/publishers/app_publisher.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/system_web_apps/apps/chrome_demo_mode_app_delegate.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

using ::testing::_;

namespace ash {
namespace {

const char kTestHtml[] =
    "<head>"
    "  <title>Hello World!</title>"
    "</head>"
    "<body>"
    "  <h1 id=\"header\">browsertest</h1>"
    "<script src=\"test.js\" type=\"module\"></script>"
    "</body>";

const char kEmptyHtml[] = "<head></head><body></body>";

const char kFakeAppId[] = "fake_app_id";

// Base class that sets everything up for the Demo Mode SWA to run, except for
// putting the device in Demo Mode itself. This is used to verify that the app
// cannot run outside of Demo Mode.
class DemoModeAppIntegrationTestBase : public ash::SystemWebAppIntegrationTest {
 public:
  DemoModeAppIntegrationTestBase() = default;

 protected:
  void SetUpOnMainThread() override {
    ash::SystemWebAppIntegrationTest::SetUpOnMainThread();
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());
    content::WebUIConfigMap::GetInstance().RemoveConfig(
        GURL(ash::kChromeUntrustedUIDemoModeAppURL));
    auto create_controller_func = base::BindLambdaForTesting(
        [&](content::WebUI* web_ui,
            const GURL& url) -> std::unique_ptr<content::WebUIController> {
          return std::make_unique<DemoModeAppUntrustedUI>(
              web_ui, component_dir_.GetPath(),
              std::make_unique<ChromeDemoModeAppDelegate>(web_ui));
        });
    content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
        std::make_unique<ash::DemoModeAppUntrustedUIConfig>(
            create_controller_func));
  }

  base::ScopedTempDir component_dir_;
  base::HistogramTester histogram_tester_;
};

class DemoModeAppIntegrationTest : public DemoModeAppIntegrationTestBase {
 public:
  using DemoModeAppIntegrationTestBase::DemoModeAppIntegrationTestBase;
  DemoModeAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{features::kDemoModeAppLandscapeLocked},
        /*disabled_features=*/{});
  }

 protected:
  // ash::SystemWebAppIntegrationTest:
  void SetUp() override {
    // Need to set demo config before SystemWebAppManager is created.
    DemoSession::SetDemoConfigForTesting(DemoSession::DemoModeConfig::kOnline);
    DemoModeAppIntegrationTestBase::SetUp();
  }

 private:
  // Use DeviceStateMixin here as we need to set InstallAttributes early
  // enough that IsDeviceInDemoMode() returns true during SystemWebAppManager
  // creation. Device ownership also needs to be established early in startup,
  // and DeviceStateMixin also sets the owner key.
  DeviceStateMixin device_state_mixin_{
      &mixin_host_, ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE};
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Class that waits for, then asserts, that a widget has entered or exited
// fullscreen
class WidgetFullscreenWaiter : public views::WidgetObserver {
 public:
  explicit WidgetFullscreenWaiter(views::Widget* widget)
      : widget_(widget), is_fullscreen_(widget->IsFullscreen()) {
    widget_observation_.Observe(widget_.get());
  }

  void WaitThenAssert(bool is_fullscreen) {
    if (widget_->IsFullscreen() != is_fullscreen) {
      is_fullscreen_ = is_fullscreen;
      run_loop_.Run();
    }
    EXPECT_EQ(widget_->IsFullscreen(), is_fullscreen);
  }

 private:
  void OnWidgetBoundsChanged(views::Widget* widget,
                             const gfx::Rect& new_bounds) override {
    if (widget->IsFullscreen() == is_fullscreen_) {
      widget->RemoveObserver(this);
      run_loop_.Quit();
    }
  }
  const raw_ptr<views::Widget> widget_;
  bool is_fullscreen_;
  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

// Mock app publisher to test that web app launches are invoked by the LaunchApp
// Mojo API.
class MockWebAppPublisher : public apps::AppPublisher {
 public:
  explicit MockWebAppPublisher(apps::AppServiceProxy* proxy)
      : apps::AppPublisher(proxy) {
    RegisterPublisher(apps::AppType::kWeb);

    std::vector<apps::AppPtr> apps;
    auto fake_app =
        std::make_unique<apps::App>(apps::AppType::kWeb, kFakeAppId);
    fake_app->readiness = apps::Readiness::kReady;
    apps.push_back(std::move(fake_app));
    Publish(std::move(apps), apps::AppType::kWeb, true);
  }

  MOCK_METHOD4(Launch,
               void(const std::string& app_id,
                    int32_t event_flags,
                    apps::LaunchSource launch_source,
                    apps::WindowInfoPtr window_info));

  MOCK_METHOD2(LaunchAppWithParams,
               void(apps::AppLaunchParams&& params,
                    apps::LaunchCallback callback));

  MOCK_METHOD6(LoadIcon,
               void(const std::string& app_id,
                    const apps::IconKey& icon_key,
                    apps::IconType icon_type,
                    int32_t size_hint_in_dip,
                    bool allow_placeholder_icon,
                    apps::LoadIconCallback callback));
};

// Verify that the app isn't registered by SystemWebAppManager when not in Demo
// Mode.
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTestBase, AppIsMissing) {
  WaitForTestSystemAppInstall();

  std::optional<webapps::AppId> missing_app_id =
      GetManager().GetAppIdForSystemApp(ash::SystemWebAppType::DEMO_MODE);
  ASSERT_FALSE(missing_app_id.has_value());
}

// Verify that WebUI cannot be navigated to directly from the browser when not
// in Demo Mode.
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTestBase, WebUIDoesNotLaunch) {
  ASSERT_FALSE(
      content::NavigateToURL(chrome_test_utils::GetActiveWebContents(this),
                             GURL(ash::kChromeUntrustedUIDemoModeAppIndexURL)));
}

// Test that the Demo Mode App installs and launches correctly
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest, DemoModeApp) {
  const GURL url(ash::kChromeUntrustedUIDemoModeAppIndexURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(SystemWebAppType::DEMO_MODE,
                                                  url, "ChromeOS Highlights"));
}

IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       DemoModeAppLoadComponentContent) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file_path = component_dir_.GetPath().AppendASCII("test.html");
  base::WriteFile(file_path, kTestHtml);

  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::DEMO_MODE);
  params.override_url = GURL(ash::kChromeUntrustedUIDemoModeAppURL +
                             file_path.BaseName().MaybeAsASCII());
  content::WebContents* web_contents = LaunchApp(std::move(params));

  EXPECT_EQ(
      std::string(kTestHtml),
      content::EvalJs(web_contents, R"(document.documentElement.innerHTML)",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
}

// Verify that javascript content loaded from component can invoke
// the ToggleFullscreen mojo API
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       DemoModeAppToggleFullscreenFromComponentContent) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  const std::string kTestJs =
      "import {pageHandler} from './page_handler.js'; "
      "document.addEventListener('DOMContentLoaded', () => {"
      "  pageHandler.toggleFullscreen(); "
      "});";

  base::FilePath file_path = component_dir_.GetPath().AppendASCII("test.html");
  base::WriteFile(file_path, kTestHtml);
  base::FilePath js_file_path = component_dir_.GetPath().AppendASCII("test.js");
  base::WriteFile(js_file_path, kTestJs);
  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::DEMO_MODE);
  params.override_url = GURL(ash::kChromeUntrustedUIDemoModeAppURL +
                             file_path.BaseName().MaybeAsASCII());
  content::WebContents* web_contents = LaunchApp(std::move(params));
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      web_contents->GetTopLevelNativeWindow());

  WidgetFullscreenWaiter(widget).WaitThenAssert(true);
}

// Verify that javascript content loaded from component can invoke
// the metricsPrivateIndividualApis extension API
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       DemoModeAppRecordMetricsFromComponentContent) {
  constexpr int kAttractLoopTimestamp = 5000, kEasyPageDuration = 6500,
                kProcessorPageDuration = 7300;
  const std::string kTestJs =
      "import {metricsService, Page, PillarButton, DetailsPage} from "
      "'./demo_mode_metrics_service.js'; "
      "document.addEventListener('DOMContentLoaded', () => {"
      "  metricsService.recordAttractLoopBreak();"
      "  metricsService.recordAttractLoopBreakTimestamp(" +
      base::ToString(kAttractLoopTimestamp) +
      ");"
      "  metricsService.recordAttractLoopBreakTimestamp(NaN);"
      "  metricsService.recordHomePageButtonClick(Page.EASY); "
      "  metricsService.recordHomePageButtonClick(Page.CHROMEOS); "
      "  metricsService.recordPageViewDuration(Page.EASY, " +
      base::ToString(kEasyPageDuration) +
      "); "
      "  metricsService.recordPageViewDuration(Page.EASY, NaN); "
      "  metricsService.recordPillarPageButtonClick(PillarButton.NEXT); "
      "  metricsService.recordNavbarButtonClick(Page.FAST); "
      "  metricsService.recordDetailsPageClicked(DetailsPage.MOBILE_GAMING); "
      "  metricsService.recordDetailsPageViewDuration(DetailsPage.PROCESSOR, " +
      base::ToString(kProcessorPageDuration) +
      "); "
      "  metricsService.recordDetailsPageViewDuration(DetailsPage.PROCESSOR, "
      "NaN); "
      "});";

  base::UserActionTester user_action_tester;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file_path = component_dir_.GetPath().AppendASCII("test.html");
  base::WriteFile(file_path, kTestHtml);
  base::FilePath js_file_path = component_dir_.GetPath().AppendASCII("test.js");
  base::WriteFile(js_file_path, kTestJs);
  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::DEMO_MODE);
  params.override_url = GURL(ash::kChromeUntrustedUIDemoModeAppURL +
                             file_path.BaseName().MaybeAsASCII());
  LaunchApp(std::move(params));

  EXPECT_EQ(user_action_tester.GetActionCount("DemoMode_AttractLoop_Break"), 1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_HomePage_Click_EasyButton"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_HomePage_Click_ChromeOSButton"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_PillarPage_Click_NextButton"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_Navbar_Click_FastButton"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_DetailsPage_Clicked_MobileGamingButton"),
            1);
  histogram_tester_.ExpectBucketCount("DemoMode.Highlights.FirstInteraction",
                                      1 /* Easy button click */, 1);
  histogram_tester_.ExpectBucketCount("DemoMode.Highlights.FirstInteraction",
                                      2 /* Fast button click */, 0);
  histogram_tester_.ExpectTimeBucketCount(
      "DemoMode.AttractLoop.Timestamp",
      base::Milliseconds(kAttractLoopTimestamp), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "DemoMode.Highlights.PageStayDuration.EasyPage",
      base::Milliseconds(kEasyPageDuration), 1);
  histogram_tester_.ExpectTimeBucketCount(
      "DemoMode.Highlights.DetailsPageStayDuration.ProcessorPage",
      base::Milliseconds(kProcessorPageDuration), 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Highlights.Error", 0 /* Invalid attract loop break timestamp */,
      1);
  histogram_tester_.ExpectBucketCount("DemoMode.Highlights.Error",
                                      1 /* Invalid page view duration */, 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.Highlights.Error", 2 /* Invalid details page view duration */,
      1);
}

// TODO(b/232945108): Change this to instead verify default resource if
// ShouldSourceFromComponent logic is changed to check if path exists
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       DemoModeAppNonexistentPathRendersEmptyPage) {
  WaitForTestSystemAppInstall();

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::DEMO_MODE);
  params.override_url =
      GURL("chrome-untrusted://demo-mode-app/nonexistent.html");
  content::WebContents* web_contents = LaunchApp(std::move(params));

  EXPECT_EQ(
      std::string(kEmptyHtml),
      content::EvalJs(web_contents, R"(document.documentElement.innerHTML)",
                      content::EXECUTE_SCRIPT_DEFAULT_OPTIONS, 1));
}

// Launch the demo mode web app from the component content, and verify that
// the orientation is locked to landscape.
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       LaunchWebAppFromComponentContent) {
  base::ScopedAllowBlockingForTesting allow_blocking;

  // Configure WebUI to serve HTML/JS invoking LaunchApp Mojo API, which calls
  // DemoModeUntrustedPageHandler::LaunchApp(app_id) ->
  // ChromeDemoModeAppDelegate::LaunchApp(app_id) ->
  // AppServiceProxyBase::Launch(app_id, 0, apps::LaunchSource::kFromOtherApp)
  // in its call hierarchy.
  const std::string kTestJs = content::JsReplace(
      "import {pageHandler} from './page_handler.js'; "
      "document.addEventListener('DOMContentLoaded', () => {"
      "  pageHandler.launchApp($1); "
      "});",
      kFakeAppId);
  base::FilePath file_path = component_dir_.GetPath().AppendASCII("test.html");
  base::WriteFile(file_path, kTestHtml);
  base::FilePath js_file_path = component_dir_.GetPath().AppendASCII("test.js");
  base::WriteFile(js_file_path, kTestJs);

  Profile* profile = ProfileManager::GetActiveUserProfile();

  // Launch SWA
  WaitForTestSystemAppInstall();
  // There should be no DemoMode.AppLaunchSource recorded at the start.
  histogram_tester_.ExpectTotalCount("DemoMode.AppLaunchSource", 0);

  apps::AppLaunchParams params =
      LaunchParamsForApp(ash::SystemWebAppType::DEMO_MODE);
  params.override_url = GURL(ash::kChromeUntrustedUIDemoModeAppURL +
                             file_path.BaseName().MaybeAsASCII());
  // The launch source of the demo mode app should be kFromChromeInternal in
  // demo session.
  params.launch_source = apps::LaunchSource::kFromChromeInternal;

  // Assert that AppServiceProxy::Launch is called by using a mock AppPublisher.
  // We mock here instead of testing that an actual app is launched due to this
  // test having a parameterized crosapi variant, which would require spinning
  // up Lacros as part of test setup.
  base::RunLoop run_loop;
  MockWebAppPublisher mock_web_app_publisher(
      apps::AppServiceProxyFactory::GetForProfile(profile));
  // We expect the call hierarchy described in the comments of `kTestJs` will
  // be gone through by verifying the last function
  // Launch(app_id, 0, apps::LaunchSource::kFromOtherApp) is called.
  EXPECT_CALL(mock_web_app_publisher,
              Launch(kFakeAppId, 0, apps::LaunchSource::kFromOtherApp, _))
      .WillOnce(base::test::RunClosure(run_loop.QuitClosure()));
  // Launch the mock demo mode app.
  LaunchApp(std::move(params));
  run_loop.Run();

  // Since we launched the fake app using the LaunchApp Mojo API, we expect to
  // see one count in AppLaunchSource::kDemoModeApp, but no others.
  histogram_tester_.ExpectTotalCount("DemoMode.AppLaunchSource", 1);
  histogram_tester_.ExpectBucketCount(
      "DemoMode.AppLaunchSource", DemoSession::AppLaunchSource::kDemoModeApp,
      1);

  // Set up the display.
  display::test::DisplayManagerTestApi(Shell::Get()->display_manager())
      .SetFirstDisplayAsInternalDisplay();
  ash::ScreenOrientationController* screen_orientation_controller =
      ash::Shell::Get()->screen_orientation_controller();

  // Enable the tablet mode to allow the auto rotation and the screen
  // orientation lock.
  auto* tablet_mode_controller = Shell::Get()->tablet_mode_controller();
  tablet_mode_controller->SetEnabledForTest(true);
  EXPECT_TRUE(tablet_mode_controller->is_in_tablet_physical_state());
  EXPECT_TRUE(screen_orientation_controller->IsAutoRotationAllowed());

  // Since we locked the orientation of the app to landscape, the current
  // rotation should be the default 0 degree (kLandscapePrimary).
  EXPECT_TRUE(screen_orientation_controller->rotation_locked());
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());

  // We locked the orientation of the app, but we did not lock the orientation
  // of the device.
  EXPECT_FALSE(screen_orientation_controller->user_rotation_locked());

  // Simulate rotating device to portrait.
  screen_orientation_controller->SetLockToRotation(display::Display::ROTATE_90);

  // The app orientation is locked to landscape so remains unchanged.
  EXPECT_EQ(chromeos::OrientationType::kLandscapePrimary,
            screen_orientation_controller->GetCurrentOrientation());
  // Since we locked the device to 90 degrees, the device rotation is locked.
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());

  // Simulate rotating device to upside-down landscape.
  screen_orientation_controller->SetLockToRotation(
      display::Display::ROTATE_180);

  // The app orientation is changed to 180 degrees (kLandscapeSecondary), which
  // is still locked in landscape.
  EXPECT_EQ(chromeos::OrientationType::kLandscapeSecondary,
            screen_orientation_controller->GetCurrentOrientation());
  // Since we locked the device to 180 degrees, the device rotation is locked.
  EXPECT_TRUE(screen_orientation_controller->user_rotation_locked());
}

// Test that the Demo Mode Highlight App has a minimum window size of 800 pixels
// x 600 pixels.
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest, DemoModeAppMinWindowSize) {
  WaitForTestSystemAppInstall();
  auto* system_app = GetManager().GetSystemApp(SystemWebAppType::DEMO_MODE);
  EXPECT_EQ(system_app->GetMinimumWindowSize(), gfx::Size(800, 600));
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTestBase);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTest);

}  // namespace
}  // namespace ash
