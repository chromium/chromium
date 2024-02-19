// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_browsertest_base.h"
#include "chrome/browser/ash/system_web_apps/test_support/system_web_app_integration_test.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/browser/ui/views/frame/browser_view.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"

using CameraAppIntegrationTest = ash::SystemWebAppIntegrationTest;

IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, MainUrlNavigation) {
  WaitForTestSystemAppInstall();

  GURL main_camera_app_url("chrome://camera-app/views/main.html");
  content::TestNavigationObserver navigation_observer(main_camera_app_url);
  navigation_observer.StartWatchingNewWebContents();
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  ui_test_utils::SendToOmniboxAndSubmit(browser(), main_camera_app_url.spec());
  navigation_observer.Wait();

  // We now have two browsers, one for the chrome window, one for the Camera
  // app.
  EXPECT_EQ(2u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(main_camera_app_url, chrome::FindLastActive()
                                     ->tab_strip_model()
                                     ->GetActiveWebContents()
                                     ->GetVisibleURL());
}

IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, OtherPageUrlNavigation) {
  WaitForTestSystemAppInstall();

  // TODO(crbug.com/980846): Change it to test page once the corresponding CL is
  // merged.
  GURL other_page_camera_app_url("chrome://camera-app/js/main.js");
  ASSERT_EQ(1u, chrome::GetTotalBrowserCount());

  ui_test_utils::SendToOmniboxAndSubmit(browser(),
                                        other_page_camera_app_url.spec());

  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  EXPECT_EQ(
      other_page_camera_app_url,
      browser()->tab_strip_model()->GetActiveWebContents()->GetVisibleURL());
}

// CCAAPI verifies whether the private JavaScript APIs CCA (Chrome camera app)
// relies on work as expected. The APIs under testing are not owned by CCA team.
// This test prevents changes to those APIs' implementations from silently
// breaking CCA.
// Contacts: chromeos-camera-eng@google.com, wtlee@chromium.org
// Ported this test from Tast: jamescook@chromium.org
// Bug Component: 978428
// Bug Component: ChromeOS > Platform > Technologies > Camera > App & Framework
IN_PROC_BROWSER_TEST_P(CameraAppIntegrationTest, CCAAPI) {
  WaitForTestSystemAppInstall();

  // Load the test page.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL("chrome://camera-app/test/test.html")));
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  ASSERT_TRUE(web_contents);

  // Assert that the window.FileSystemHandle API exists.
  auto result =
      content::EvalJs(web_contents, "window.FileSystemHandle !== undefined");
  bool has_window_file_system_handle = result.ExtractBool();
  EXPECT_TRUE(has_window_file_system_handle);

  // Load the untrusted script loader.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      GURL("chrome://camera-app/views/untrusted_script_loader.html")));
  EXPECT_EQ(1u, chrome::GetTotalBrowserCount());
  web_contents = browser()->tab_strip_model()->GetActiveWebContents();

  // Assert that the window.launchQueue API exists.
  auto result2 =
      content::EvalJs(web_contents, "window.launchQueue !== undefined");
  bool has_window_launch_queue = result2.ExtractBool();
  EXPECT_TRUE(has_window_launch_queue);
}

INSTANTIATE_SYSTEM_WEB_APP_MANAGER_TEST_SUITE_REGULAR_PROFILE_P(
    CameraAppIntegrationTest);
