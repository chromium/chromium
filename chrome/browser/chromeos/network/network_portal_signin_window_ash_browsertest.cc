// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/scoped_feature_list.h"
#include "chrome/browser/ash/login/test/guest_session_mixin.h"
#include "chrome/browser/chromeos/network/network_portal_signin_window.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/navigator/browser_navigator.h"
#include "chrome/browser/ui/navigator/browser_navigator_params.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/mixin_based_in_process_browser_test.h"
#include "components/captive_portal/content/captive_portal_tab_helper.h"
#include "components/captive_portal/core/captive_portal_detector.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/controllable_http_response.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace chromeos {

class NetworkPortalSigninWindowAshBrowserTest : public InProcessBrowserTest {
 public:
  NetworkPortalSigninWindowAshBrowserTest() {
    // TODO(crbug.com/452061489): Fix the tests that fail when WebUI Omnibox is
    // enabled and then remove this.
    webui_omnibox_feature_list_.InitFromCommandLine(
        "", "WebUIOmniboxPopup,WebUIOmniboxAimPopup");
  }

 protected:
  base::test::ScopedFeatureList webui_omnibox_feature_list_;
};

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
  BrowserWindowInterface* browser =
      portal_signin_window->GetBrowserForTesting();
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

class NetworkPortalSigninWindowAshGuestBrowserTest
    : public MixinBasedInProcessBrowserTest {
 public:
  NetworkPortalSigninWindowAshGuestBrowserTest() {
    // TODO(crbug.com/452061489): Fix the tests that fail when WebUI Omnibox is
    // enabled and then remove this.
    webui_omnibox_feature_list_.InitFromCommandLine(
        "", "WebUIOmniboxPopup,WebUIOmniboxAimPopup");
  }

 protected:
  ash::GuestSessionMixin guest_session_{&mixin_host_};
  base::test::ScopedFeatureList webui_omnibox_feature_list_;
};

IN_PROC_BROWSER_TEST_F(NetworkPortalSigninWindowAshGuestBrowserTest,
                       NavigateFromCaptivePortalSigninWindow) {
  // In a Guest session the active user profile is itself off the record and is
  // used directly as the captive portal signin profile.
  ASSERT_TRUE(ProfileManager::GetActiveUserProfile()->IsOffTheRecord());

  content::CreateAndLoadWebContentsObserver web_contents_observer;

  auto* portal_signin_window = NetworkPortalSigninWindow::Get();
  portal_signin_window->Show(
      GURL(captive_portal::CaptivePortalDetector::GetDefaultUrl()));
  BrowserWindowInterface* browser =
      portal_signin_window->GetBrowserForTesting();
  ASSERT_TRUE(browser);

  web_contents_observer.Wait();

  content::WebContents* web_contents =
      portal_signin_window->GetWebContentsForTesting();
  ASSERT_TRUE(web_contents);
  captive_portal::CaptivePortalTabHelper* helper =
      captive_portal::CaptivePortalTabHelper::FromWebContents(web_contents);
  ASSERT_TRUE(helper);
  EXPECT_TRUE(helper->is_captive_portal_window());

  // Navigate to a new tab from the captive portal signin window. The contents
  // should be opened in the same tab even though the signin window shares its
  // profile with the Guest browsing session.
  NavigateParams params(browser, GURL("http://www.google.com"),
                        ui::PageTransition::PAGE_TRANSITION_LINK);
  params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&params);
  EXPECT_EQ(params.browser, browser);
  EXPECT_EQ(params.tabstrip_index, -1);

  // Same, but with an explicit source WebContents (e.g. `window.open`).
  NavigateParams source_params(browser, GURL("http://www.google.com"),
                               ui::PageTransition::PAGE_TRANSITION_LINK);
  source_params.source_contents = web_contents;
  source_params.disposition = WindowOpenDisposition::NEW_FOREGROUND_TAB;
  Navigate(&source_params);
  EXPECT_EQ(source_params.browser, browser);
  EXPECT_EQ(source_params.tabstrip_index, -1);
}

// Tests that a link opened in a new tab from a sandboxed subframe within a
// captive portal sign-in window respects the iframe's sandbox restrictions.
// Specifically, because the iframe is sandboxed against top-level navigation
// (kTopNavigation), captive portal navigation restrictions must not rewrite the
// navigation to CURRENT_TAB (which would allow a sandbox escape). Instead, the
// browser falls back to NEW_POPUP and opens a new popup window, preserving the
// current URL of the captive portal sign-in window.
IN_PROC_BROWSER_TEST_F(NetworkPortalSigninWindowAshBrowserTest,
                       NavigateNewTabFromSandboxedSubframe) {
  net::test_server::ControllableHttpResponse embedder_response(
      embedded_test_server(), "/embedder.html");
  net::test_server::ControllableHttpResponse iframe_response(
      embedded_test_server(), "/iframe.html");
  net::test_server::ControllableHttpResponse target_response(
      embedded_test_server(), "/target.html");
  ASSERT_TRUE(embedded_test_server()->Start());

  auto* portal_signin_window = NetworkPortalSigninWindow::Get();
  GURL start_url = embedded_test_server()->GetURL("/embedder.html");

  portal_signin_window->Show(start_url);

  embedder_response.WaitForRequest();
  embedder_response.Send(
      net::HTTP_OK, "text/html",
      "<html><body>"
      "<iframe id='iframe' sandbox='allow-scripts allow-popups' "
      "src='/iframe.html'></iframe>"
      "</body></html>");
  embedder_response.Done();

  iframe_response.WaitForRequest();
  iframe_response.Send(
      net::HTTP_OK, "text/html",
      "<html><body>"
      "<a id='link' href='/target.html' target='_blank'>Click me</a>"
      "</body></html>");
  iframe_response.Done();

  content::WebContents* web_contents =
      portal_signin_window->GetWebContentsForTesting();
  ASSERT_TRUE(web_contents);

  content::WaitForLoadStop(web_contents);
  EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

  content::RenderFrameHost* iframe_rfh = content::ChildFrameAt(web_contents, 0);
  ASSERT_TRUE(iframe_rfh);
  EXPECT_EQ(iframe_rfh->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/iframe.html"));

  content::WebContentsAddedObserver web_contents_added_observer;

  EXPECT_TRUE(
      content::ExecJs(iframe_rfh, "document.getElementById('link').click();"));

  target_response.WaitForRequest();
  target_response.Send(net::HTTP_OK, "text/html",
                       "<html><body>Target</body></html>");
  target_response.Done();

  content::WebContents* new_contents =
      web_contents_added_observer.GetWebContents();
  ASSERT_TRUE(new_contents);

  EXPECT_EQ(web_contents->GetLastCommittedURL(), start_url);

  content::WaitForLoadStop(new_contents);
  EXPECT_EQ(new_contents->GetLastCommittedURL(),
            embedded_test_server()->GetURL("/target.html"));
}

}  // namespace chromeos
