// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/metrics/user_action_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/login/demo_mode/demo_session.h"
#include "chrome/browser/ash/login/demo_mode/demo_setup_test_utils.h"
#include "chrome/browser/ash/login/test/device_state_mixin.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/test/base/chrome_test_utils.h"
#include "chromeos/constants/chromeos_features.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

const char kTestHtml[] =
    "<head>"
    "  <title>Hello World!</title>"
    "</head>"
    "<body>"
    "  <h1 id=\"header\">browsertest</h1>"
    "<script src=\"test.js\" type=\"module\"></script>"
    "</body>";

const char kTestJs[] =
    "import {pageHandler} from './page_handler.js'; "
    "document.addEventListener('DOMContentLoaded', function () {"
    " pageHandler.toggleFullscreen(); "
    "});";

/**
 * Mocks a user breaks attrack loop and enters the demo session,
 * clicks the Easy page button, stays for 10 sec, clicks Next button,
 * and clicks Fast page button.
 */
const char kTestMetricsServiceJs[] =
    "import {metricsService, Page, PillarButton} from "
    "'./demo_mode_metrics_service.js'; "
    "document.addEventListener('DOMContentLoaded', function () {"
    " metricsService.recordAttractLoopBreak();"
    " metricsService.recordHomePageButtonClick(Page.EASY); "
    " metricsService.recordPageViewDuration(Page.EASY, 10000); "
    " metricsService.recordPillarPageButtonClick(PillarButton.NEXT); "
    " metricsService.recordNavbarButtonClick(Page.FAST); "
    "});";

const char kEmptyHtml[] = "<head></head><body></body>";

// Base class that sets everything up for the Demo Mode SWA to run, except for
// putting the device in Demo Mode itself. This is used to verify that the app
// cannot run outside of Demo Mode.
class DemoModeAppIntegrationTestBase : public ash::SystemWebAppIntegrationTest {
 public:
  DemoModeAppIntegrationTestBase() {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kDemoModeSWA);
  }

 protected:
  void SetUpOnMainThread() override {
    base::ScopedAllowBlockingForTesting allow_blocking;
    ASSERT_TRUE(component_dir_.CreateUniqueTempDir());
    content::WebUIConfigMap::GetInstance().RemoveConfig(
        url::Origin::Create(GURL(ash::kChromeUntrustedUIDemoModeAppURL)));
    content::WebUIConfigMap::GetInstance().AddUntrustedWebUIConfig(
        std::make_unique<ash::DemoModeAppUntrustedUIConfig>(
            base::BindLambdaForTesting(
                [&] { return component_dir_.GetPath(); })));
  }

  base::ScopedTempDir component_dir_;
  base::test::ScopedFeatureList scoped_feature_list_;
  base::HistogramTester histogram_tester_;
};

class DemoModeAppIntegrationTest : public DemoModeAppIntegrationTestBase {
 public:
  using DemoModeAppIntegrationTestBase::DemoModeAppIntegrationTestBase;

 protected:
  // ash::SystemWebAppIntegrationTest:
  void SetUp() override {
    // Need to set demo config before SystemWebAppManager is created.
    ash::DemoSession::SetDemoConfigForTesting(
        ash::DemoSession::DemoModeConfig::kOnline);
    DemoModeAppIntegrationTestBase::SetUp();
  }

 private:
  // Use DeviceStateMixin here as we need to set InstallAttributes early
  // enough that IsDeviceInDemoMode() returns true during SystemWebAppManager
  // creation. Device ownership also needs to be established early in startup,
  // and DeviceStateMixin also sets the owner key.
  ash::DeviceStateMixin device_state_mixin_{
      &mixin_host_, ash::DeviceStateMixin::State::OOBE_COMPLETED_DEMO_MODE};
};

// Class that waits for, then asserts, that a widget has entered or exited
// fullscreen
class WidgetFullscreenWaiter : public views::WidgetObserver {
 public:
  explicit WidgetFullscreenWaiter(views::Widget* widget)
      : widget_(widget), is_fullscreen_(widget->IsFullscreen()) {
    widget_observation_.Observe(widget_);
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
  views::Widget* const widget_;
  bool is_fullscreen_;
  base::RunLoop run_loop_;
  base::ScopedObservation<views::Widget, views::WidgetObserver>
      widget_observation_{this};
};

// Verify that the app isn't registered by SystemWebAppManager when not in Demo
// Mode.
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTestBase, AppIsMissing) {
  WaitForTestSystemAppInstall();

  absl::optional<web_app::AppId> missing_app_id =
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
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::DEMO_MODE, url, "Demo Mode App"));
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
  base::UserActionTester user_action_tester;
  base::ScopedAllowBlockingForTesting allow_blocking;
  base::FilePath file_path = component_dir_.GetPath().AppendASCII("test.html");
  base::WriteFile(file_path, kTestHtml);
  base::FilePath js_file_path = component_dir_.GetPath().AppendASCII("test.js");
  base::WriteFile(js_file_path, kTestMetricsServiceJs);
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
                "DemoMode_Highlights_PillarPage_Click_NextButton"),
            1);
  EXPECT_EQ(user_action_tester.GetActionCount(
                "DemoMode_Highlights_Navbar_Click_FastButton"),
            1);
  histogram_tester_.ExpectBucketCount("DemoMode.Highlights.FirstInteraction",
                                      1 /* Easy button click */, 1);
  histogram_tester_.ExpectBucketCount("DemoMode.Highlights.FirstInteraction",
                                      2 /* Fast button click */, 0);
  histogram_tester_.ExpectTimeBucketCount(
      "DemoMode.Highlights.PageStayDuration.EasyPage", base::Seconds(10), 1);
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

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTestBase);

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTest);
