// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

using NetworkPortalSigninWindowAshBrowserTest = InProcessBrowserTest;

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninWindowAshBrowserTest,
                       IsCaptivePortalWindow) {
  content::CreateAndLoadWebContentsObserver web_contents_observer;

  auto* portal_signin_window = NetworkPortalSigninWindow::Get();
  portal_signin_window->Show(
      GURL(captive_portal::CaptivePortalDetector::GetDefaultUrl()));
  ASSERT_TRUE(NetworkPortalSigninWindow::Get()->GetBrowserForTesting());

  web_contents_observer.Wait();

  // Showing the window should generate a DidFinishNavigation event which should
  // trigger a corresponding captive portal detection request.
  EXPECT_EQ(portal_signin_window->portal_detection_requested_for_testing(), 1);

  // The popup window sets the |is_captive_portal_popup| param which should
  // set the |CaptivePortalTabHelper::is_captive_portal_window| property.
  content::WebContents* web_contents =
      portal_signin_window->GetWebContentsForTesting();
  ASSERT_TRUE(web_contents);
  captive_portal::CaptivePortalTabHelper* helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  EXPECT_TRUE(helper->is_captive_portal_window());
}

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninWindowAshBrowserTest,
                       NavigateFromCaptivePortalSigninWindow) {
  content::CreateAndLoadWebContentsObserver web_contents_observer;

  auto* portal_signin_window = NetworkPortalSigninWindow::Get();
  portal_signin_window->Show(
      GURL(captive_portal::CaptivePortalDetector::GetDefaultUrl()));
  ASSERT_TRUE(portal_signin_window->GetBrowserForTesting());

  web_contents_observer.Wait();

  // Navigate within the captive portal signin window. The contents should be
  // opened in the same browser.
  Browser* browser = portal_signin_window->GetBrowserForTesting();
  NavigateParams params(browser, GURL("http://www.google.com"),
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  Navigate(&params);
  EXPECT_EQ(params.browser, browser);
  EXPECT_EQ(params.tabstrip_index, -1);

  // Navigate to a new tab. The contents should be opened in the same tab.
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  EXPECT_EQ(params.browser, browser);
  EXPECT_EQ(params.tabstrip_index, -1);
}

}  // namespace chromeos
