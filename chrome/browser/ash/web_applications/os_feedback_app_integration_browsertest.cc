// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/web_applications/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/web_applications/system_web_apps/test/system_web_app_browsertest_base.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/browser/notification_types.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class OSFeedbackAppIntegrationTest : public SystemWebAppIntegrationTest {
 public:
  OSFeedbackAppIntegrationTest() {
    scoped_feature_list_.InitWithFeatures({ash::features::kOsFeedback}, {});
  }

 protected:
  base::HistogramTester histogram_tester_;

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Test that the Feedback App installs and launches correctly by
// running some spot checks on the manifest.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, OSFeedbackAppInLauncher) {
  const GURL url(ash::kChromeUIOSFeedbackUrl);
  EXPECT_NO_FATAL_FAILURE(ExpectSystemWebAppValid(
      web_app::SystemAppType::OS_FEEDBACK, url, "Feedback"));

  histogram_tester_.ExpectBucketCount(
      "Webapp.InstallResult.System.Apps.OSFeedback",
      webapps::InstallResultCode::kSuccessOfflineOnlyInstall, 1);
}

// This test verifies that the Feedback app is opened in a new browser window.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, NavigateToFeedback) {
  WaitForTestSystemAppInstall();

  GURL main_feedback_url(ash::kChromeUIOSFeedbackUrl);
  GURL old_url = browser()->tab_strip_model()->GetActiveWebContents()->GetURL();
  {
    content::WindowedNotificationObserver observer(
        content::NOTIFICATION_LOAD_STOP,
        content::NotificationService::AllSources());
    ui_test_utils::SendToOmniboxAndSubmit(browser(), main_feedback_url.spec());
    observer.Wait();
  }

  // browser() tab contents should be unaffected.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(old_url,
            browser()->tab_strip_model()->GetActiveWebContents()->GetURL());

  // We now have two browsers, one for the chrome window, one for the Feedback
  // app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(main_feedback_url, chrome::FindLastActive()
                                   ->tab_strip_model()
                                   ->GetActiveWebContents()
                                   ->GetVisibleURL());
}

// Test that the Feedback App has a default bounds of 640(height)x600(width)
// and is in the center of the screen.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, DefaultWindowBounds) {
  display::test::DisplayManagerTestApi display_manager_test(
      ash::Shell::Get()->display_manager());
  display_manager_test.UpdateDisplay("1000x2000");

  WaitForTestSystemAppInstall();
  Browser* browser;
  LaunchApp(web_app::SystemAppType::OS_FEEDBACK, &browser);

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();

  int expected_width = 600;
  int expected_height = 640;
  int x = (work_area.width() - expected_width) / 2;
  int y = (work_area.height() - expected_height) / 2;
  EXPECT_EQ(browser->window()->GetBounds(),
            gfx::Rect(x, y, expected_width, expected_height));
}

// Test that the Feedback App
//  1) shows in launcher
//  2) shows in search
//  3) is single window
//  4) allows scripts to close the window
//  5) isn't resizable
//  6) isn't maximizable
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, FeedbackAppAttributes) {
  WaitForTestSystemAppInstall();

  // Check the correct attributes for Feedback App.
  auto* system_app =
      GetManager().GetSystemApp(web_app::SystemAppType::OS_FEEDBACK);
  EXPECT_TRUE(system_app->ShouldShowInLauncher());
  EXPECT_TRUE(system_app->ShouldShowInSearch());
  EXPECT_TRUE(system_app->ShouldReuseExistingWindow());
  EXPECT_TRUE(system_app->ShouldAllowScriptsToCloseWindows());
  EXPECT_FALSE(system_app->ShouldAllowResize());
  EXPECT_FALSE(system_app->ShouldAllowMaximize());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    OSFeedbackAppIntegrationTest);
