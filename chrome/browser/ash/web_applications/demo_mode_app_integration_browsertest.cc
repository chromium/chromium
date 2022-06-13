// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/webui/demo_mode_app_ui/url_constants.h"
#include "base/scoped_observation.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "content/public/test/browser_test.h"
#include "ui/views/widget/widget.h"
#include "ui/views/widget/widget_observer.h"

class DemoModeAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  DemoModeAppIntegrationTest() {
    scoped_feature_list_.InitAndEnableFeature(chromeos::features::kDemoModeSWA);
  }
 private:
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
  const GURL url(ash::kChromeUIDemoModeAppURL);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      ash::SystemWebAppType::DEMO_MODE, url, "Demo Mode App"));
}

// Test that Demo Mode app starts in fullscreen from initial call to
// ToggleFullscreen() Mojo API, and subsequent call exits fullscreen
IN_PROC_BROWSER_TEST_P(DemoModeAppIntegrationTest,
                       DemoModeAppToggleFullscreen) {
  WaitForTestSystemAppInstall();
  Browser* browser;
  content::WebContents* web_contents =
      LaunchApp(ash::SystemWebAppType::DEMO_MODE, &browser);
  views::Widget* widget = views::Widget::GetWidgetForNativeWindow(
      web_contents->GetTopLevelNativeWindow());
  WidgetFullscreenWaiter(widget).WaitThenAssert(true);

  bool success = content::ExecuteScript(
      web_contents, "window.pageHandler.toggleFullscreen();");
  EXPECT_TRUE(success);
  WidgetFullscreenWaiter(widget).WaitThenAssert(false);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_GUEST_SESSION_P(
    DemoModeAppIntegrationTest);
