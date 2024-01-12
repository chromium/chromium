// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/constants/ash_features.h"
#include "ash/shell.h"
#include "ash/webui/os_feedback_ui/url_constants.h"
#include "ash/webui/sample_system_web_app_ui/url_constants.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/system_web_apps/system_web_app_manager.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/interactive_test_utils.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/display/screen.h"
#include "ui/display/test/display_manager_test_api.h"
#include "ui/gfx/geometry/rect.h"
#include "url/gurl.h"

class OSFeedbackAppIntegrationTest : public ash::SystemWebAppIntegrationTest {
 public:
  OSFeedbackAppIntegrationTest() {
    feedback_url_ = GURL(ash::kChromeUIOSFeedbackUrl);
  }

 protected:
  // Find the url of the active tab of the browser if any.
  GURL FindActiveUrl(Browser* browser) {
    if (browser) {
      return browser->tab_strip_model()->GetActiveWebContents()->GetURL();
    }
    return GURL();
  }

  Browser* FindFeedbackAppBrowser() {
    return ash::FindSystemWebAppBrowser(browser()->profile(),
                                        ash::SystemWebAppType::OS_FEEDBACK);
  }

  // Launch the Feedback SWA and wait for launching is completed.
  // Returns the browser of the Feedback SWA if exists.
  Browser* LaunchAndWait() {
    WaitForTestSystemAppInstall();

    content::TestNavigationObserver navigation_observer(feedback_url_);
    navigation_observer.StartWatchingNewWebContents();
    ui_test_utils::SendToOmniboxAndSubmit(browser(), feedback_url_.spec());
    navigation_observer.Wait();

    return FindFeedbackAppBrowser();
  }

  Browser* ExpectFeedbackAppLaunched(const GURL& old_url) {
    // browser() tab contents should be unaffected.
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    EXPECT_EQ(old_url, FindActiveUrl(browser()));

    // We now have two browsers, one for the chrome window, one for the Feedback
    // app.
    EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
    Browser* app_browser = FindFeedbackAppBrowser();
    EXPECT_TRUE(app_browser);
    EXPECT_EQ(feedback_url_, FindActiveUrl(app_browser));

    return app_browser;
  }

  void ExpectNoFeedbackAppLaunched(const GURL& old_url) {
    // browser() tab contents should be unaffected.
    EXPECT_EQ(1, browser()->tab_strip_model()->count());
    EXPECT_EQ(old_url, FindActiveUrl(browser()));

    // We now still have one browser.
    EXPECT_EQ(1u, chrome::GetTotalBrowserCount());

    EXPECT_EQ(nullptr, FindFeedbackAppBrowser());
  }

  void SendKeyPressAltShiftI(Browser* browser) {
    ASSERT_TRUE(ui_test_utils::SendKeyPressSync(
        browser, ui::VKEY_I, /* control= */ false, /* shift= */ true,
        /* alt= */ true, /* command= */ false));
  }

  GURL feedback_url_;
  base::HistogramTester histogram_tester_;
};

// This test verifies that the Feedback app is opened in a new browser window.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, NavigateToFeedback) {
  WaitForTestSystemAppInstall();
  GURL old_url = FindActiveUrl(browser());

  LaunchAndWait();
  ExpectFeedbackAppLaunched(old_url);
}

// This test verifies that the Feedback app is opened in a new browser window.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, OpenFeedbackByHotKey) {
  WaitForTestSystemAppInstall();
  GURL old_url = FindActiveUrl(browser());

  WaitForTestSystemAppInstall();

  feedback_url_ = GURL(base::StrCat(
      {ash::kChromeUIOSFeedbackUrl, "/?page_url=",
       base::EscapeQueryParamValue(old_url.spec(), /*use_plus=*/false)}));

  content::TestNavigationObserver navigation_observer(feedback_url_);
  navigation_observer.StartWatchingNewWebContents();
  // Try to press keyboard shortcut to open Feedback app.
  SendKeyPressAltShiftI(browser());
  navigation_observer.Wait();

  ExpectFeedbackAppLaunched(old_url);

  feedback_url_ = GURL(ash::kChromeUIOSFeedbackUrl);
}

// This test verifies that the Feedback app is not opened when
// UserFeedbackAllowed is set to false.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, UserFeedbackNotAllowed) {
  WaitForTestSystemAppInstall();

  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);
  GURL old_url = FindActiveUrl(browser());

  // Try to navigate to the feedback app in the browser.
  ui_test_utils::SendToOmniboxAndSubmit(browser(), feedback_url_.spec());

  ExpectNoFeedbackAppLaunched(old_url);

  // Try to press keyboard shortcut to open Feedback app.
  SendKeyPressAltShiftI(browser());

  ExpectNoFeedbackAppLaunched(old_url);
}

// Test that the Feedback App has a default bounds of 640(height)x600(width)
// and is in the center of the screen.
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest, DefaultWindowBounds) {
  display::test::DisplayManagerTestApi display_manager_test(
      ash::Shell::Get()->display_manager());
  display_manager_test.UpdateDisplay("1000x2000");

  Browser* app_browser = LaunchAndWait();
  EXPECT_TRUE(app_browser);

  gfx::Rect work_area =
      display::Screen::GetScreen()->GetDisplayForNewWindows().work_area();

  int expected_width = 600;
  int expected_height = 640;
  int x = (work_area.width() - expected_width) / 2;
  int y = (work_area.height() - expected_height) / 2;
  EXPECT_EQ(gfx::Rect(x, y, expected_width, expected_height),
            app_browser->window()->GetBounds());
}

// Test that when the policy UserFeedbackAllowed is true, the Feedback App
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
      GetManager().GetSystemApp(ash::SystemWebAppType::OS_FEEDBACK);
  EXPECT_FALSE(system_app->ShouldShowInLauncher());
  EXPECT_TRUE(system_app->ShouldShowInSearchAndShelf());
  EXPECT_FALSE(system_app->ShouldShowNewWindowMenuOption());
  EXPECT_TRUE(system_app->ShouldAllowScriptsToCloseWindows());
  EXPECT_FALSE(system_app->ShouldAllowResize());
  EXPECT_FALSE(system_app->ShouldAllowMaximize());
}

// Test that when the policy UserFeedbackAllowed is false, the Feedback App
//  1) does not show in launcher
//  2) does not show in search
IN_PROC_BROWSER_TEST_P(OSFeedbackAppIntegrationTest,
                       HideInLauncherAndSearchWhenUserFeedbackNotAllowed) {
  WaitForTestSystemAppInstall();
  browser()->profile()->GetPrefs()->SetBoolean(prefs::kUserFeedbackAllowed,
                                               false);

  // Check the correct attributes for Feedback App.
  auto* system_app =
      GetManager().GetSystemApp(ash::SystemWebAppType::OS_FEEDBACK);
  EXPECT_FALSE(system_app->ShouldShowInLauncher());
  EXPECT_FALSE(system_app->ShouldShowInSearchAndShelf());
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    OSFeedbackAppIntegrationTest);
