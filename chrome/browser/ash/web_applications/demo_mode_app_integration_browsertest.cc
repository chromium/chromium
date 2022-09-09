// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/demo_mode_app_untrusted_ui.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "ash/webui/web_applications/test/sandboxed_web_ui_test_base.h"
#include "base/files/file_util.h"
#include "base/files/scoped_temp_dir.h"
#include "base/scoped_observation.h"
#include "base/test/bind.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "content/public/browser/webui_config_map.h"
#include "content/public/test/browser_test.h"
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

const char kEmptyHtml[] = "<head></head><body></body>";

class DemoModeAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  DemoModeAppIntegrationTest() {
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
    DemoModeAppIntegrationTest);
