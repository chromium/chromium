// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "chrome/browser/apps/platform_apps/app_browsertest_util.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_navigator_params.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/embedder_support/switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_base.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/fenced_frame_test_util.h"
#include "content/public/test/prerender_test_util.h"
#include "extensions/test/extension_test_message_listener.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/page_transition_types.h"

namespace extensions {

class PlatformAppNavigationRedirectorBrowserTest
    : public PlatformAppBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override;

 protected:
  // Performs the following sequence:
  // - installs the app |handler| (a relative path under the platform_apps
  // subdirectory);
  // - navigates the current tab to the HTML page |lancher_page| (ditto);
  // - then waits for |handler| to launch and send back a |handler_ack_message|;
  // - finally checks that the resulting app window count is as expected.
  // The |launcher_page| is supposed to trigger a navigation matching one of the
  // url_handlers in the |handler|'s manifest, and thereby launch the |handler|.
  void TestNavigationInTab(const char* launcher_page,
                           const char* handler,
                           const char* handler_start_message);

  // The same as above, but does not expect the |handler| to launch. Verifies
  // that it didn't, and that the navigation has opened in a new tab instead.
  void TestMismatchingNavigationInTab(const char* launcher_page,
                                      const char* success_tab_title,
                                      const char* handler);

  // - installs the app |handler|;
  // - opens the page |xhr_opening_page| in the current tab;
  // - the page sends an XHR to a local resouce, whose URL matches one of the
  //   url_handlers in |handler|;
  // - waits until |xhr_opening_page| gets a response back and changes the tab's
  //   title to a value indicating success/failure of the XHR;
  // - verifies that no app windows have been opened, i.e. |handler| wasn't
  //   launched even though its url_handlers match the URL.
  void TestNegativeXhrInTab(const char* xhr_opening_page,
                            const char* success_tab_title,
                            const char* failure_tab_title,
                            const char* handler);

  // Performs the following sequence:
  // - installs the app |handler| (a relative path under the platform_apps
  // subdirectory);
  // - loads and launches the app |launcher| (ditto);
  // - waits for the |launcher| to launch and send back a |launcher_ack_message|
  //   (to make sure it's not the failing entity, if the test fails overall);
  // - waits for the |handler| to launch and send back a |handler_ack_message|;
  // - finally checks that the resulting app window count is as expected.
  // The |launcher| is supposed to trigger a navigation matching one of the
  // url_handlers in the |handler|'s manifest, and thereby launch the |handler|.
  void TestNavigationInApp(const char* launcher,
                           const char* launcher_done_message,
                           const char* handler,
                           const char* handler_start_message);

  // The same as above, but does not expect the |handler| to launch. Verifies
  // that it didn't, and that the navigation has opened in a new tab instead.
  void TestMismatchingNavigationInApp(const char* launcher,
                                      const char* launcher_done_message,
                                      const char* handler);

  // - installs the |handler| app;
  // - loads and launches the |launcher| app;
  // - waits until the |launcher| sends back a |launcher_done_message|;
  // - the launcher performs a navigation to a URL that mismatches the
  //   |handler|'s url_handlers;
  // - verifies that the |handler| hasn't been launched as a result of the
  //   navigation.
  void TestNegativeNavigationInApp(const char* launcher,
                                   const char* launcher_done_message,
                                   const char* handler);

  // - installs the app |handler|;
  // - navigates the current tab to the HTML page |matching_target_page| with
  //   page transition |transition|;
  // - waits for |handler| to launch and send back a |handler_start_message|;
  // - finally checks that the resulting app window count is as expected.
  void TestNavigationInBrowser(const char* matching_target_page,
                               ui::PageTransition transition,
                               const char* handler,
                               const char* handler_start_message);

  // Same as above, but does not expect |handler| to launch. This is used, e.g.
  // for form submissions, where the URL would normally match the url_handlers
  // but should not launch it.
  void TestNegativeNavigationInBrowser(const char* matching_target_page,
                                       ui::PageTransition transition,
                                       const char* success_tab_title,
                                       const char* handler);

  // Same as above, but expects the |mismatching_target_page| should not match
  // any of the |handler|'s url_handlers, and therefor not launch the app.
  void TestMismatchingNavigationInBrowser(const char* mismatching_target_page,
                                          ui::PageTransition transition,
                                          const char* success_tab_title,
                                          const char* handler);
};

void PlatformAppNavigationRedirectorBrowserTest::SetUpCommandLine(
    base::CommandLine* command_line) {
  PlatformAppBrowserTest::SetUpCommandLine(command_line);
  command_line->AppendSwitch(embedder_support::kDisablePopupBlocking);
}

// TODO(sergeygs): Factor out common functionality from TestXyz,
// TestNegativeXyz, and TestMismatchingXyz versions.

// TODO(sergeys): Return testing::AssertionErrors from these methods to
// preserve line numbers and (if applicable) failure messages.

void PlatformAppNavigationRedirectorBrowserTest::TestNavigationInTab(
    const char* launcher_page,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener handler_listener(handler_start_message);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(base::StringPrintf(
                     "/extensions/platform_apps/%s", launcher_page))));

  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(1U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::TestMismatchingNavigationInTab(
    const char* launcher_page,
    const char* success_tab_title,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  const std::u16string success_title = base::ASCIIToUTF16(success_tab_title);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, success_title);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(base::StringPrintf(
                     "/extensions/platform_apps/%s", launcher_page))));

  ASSERT_EQ(success_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::TestNegativeXhrInTab(
    const char* launcher_page,
    const char* success_tab_title,
    const char* failure_tab_title,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  const std::u16string success_title = base::ASCIIToUTF16(success_tab_title);
  const std::u16string failure_title = base::ASCIIToUTF16(failure_tab_title);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, success_title);
  title_watcher.AlsoWaitForTitle(failure_title);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), embedded_test_server()->GetURL(base::StringPrintf(
                     "/extensions/platform_apps/%s", launcher_page))));

  ASSERT_EQ(success_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(1, browser()->tab_strip_model()->count());
  ASSERT_EQ(0U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::TestNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener handler_listener(handler_start_message);

  LoadAndLaunchPlatformApp(launcher, launcher_done_message);

  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(2U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::TestNegativeNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ui_test_utils::TabAddedWaiter tab_add(browser());

  LoadAndLaunchPlatformApp(launcher, launcher_done_message);

  tab_add.Wait();

  ASSERT_EQ(1U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::TestMismatchingNavigationInApp(
    const char* launcher,
    const char* launcher_done_message,
    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ui_test_utils::TabAddedWaiter tab_add(browser());

  LoadAndLaunchPlatformApp(launcher, launcher_done_message);

  tab_add.Wait();

  ASSERT_EQ(1U, GetAppWindowCount());
  ASSERT_EQ(2, browser()->tab_strip_model()->count());
}

void PlatformAppNavigationRedirectorBrowserTest::TestNavigationInBrowser(
    const char* matching_target_page,
    ui::PageTransition transition,
    const char* handler,
    const char* handler_start_message) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  ExtensionTestMessageListener handler_listener(handler_start_message);

  NavigateParams params(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/extensions/platform_apps/%s", matching_target_page)),
      transition);
  ui_test_utils::NavigateToURL(&params);

  ASSERT_TRUE(handler_listener.WaitUntilSatisfied());

  ASSERT_EQ(1U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::
    TestNegativeNavigationInBrowser(const char* matching_target_page,
                                    ui::PageTransition transition,
                                    const char* success_tab_title,
                                    const char* handler) {
  ASSERT_TRUE(StartEmbeddedTestServer());

  InstallPlatformApp(handler);

  const std::u16string success_title = base::ASCIIToUTF16(success_tab_title);
  content::WebContents* tab =
      browser()->tab_strip_model()->GetActiveWebContents();
  content::TitleWatcher title_watcher(tab, success_title);

  NavigateParams params(
      browser(),
      embedded_test_server()->GetURL(base::StringPrintf(
          "/extensions/platform_apps/%s", matching_target_page)),
      transition);
  ui_test_utils::NavigateToURL(&params);

  ASSERT_EQ(success_title, title_watcher.WaitAndGetTitle());
  ASSERT_EQ(0U, GetAppWindowCount());
}

void PlatformAppNavigationRedirectorBrowserTest::
    TestMismatchingNavigationInBrowser(const char* mismatching_target_page,
                                       ui::PageTransition transition,
                                       const char* success_tab_title,
                                       const char* handler) {
  TestNegativeNavigationInBrowser(mismatching_target_page, transition,
                                  success_tab_title, handler);
}

// Test that a click on a regular link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       ClickInTabIntercepted) {
  TestNavigationInTab("url_handlers/launching_pages/click_link.html",
                      "url_handlers/handlers/simple", "Handler launched");
}

// Test that a click on a target='_blank' link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       BlankClickInTabIntercepted) {
  TestNavigationInTab("url_handlers/launching_pages/click_blank_link.html",
                      "url_handlers/handlers/simple", "Handler launched");
}

// Test that a call to window.open() in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       WindowOpenInTabIntercepted) {
  TestNavigationInTab("url_handlers/launching_pages/call_window_open.html",
                      "url_handlers/handlers/simple", "Handler launched");
}

// Test that a click on a regular link in a tab launches an app that has
// matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       MismatchingClickInTabNotIntercepted) {
  TestMismatchingNavigationInTab(
      "url_handlers/launching_pages/click_mismatching_link.html",
      "Mismatching link target loaded", "url_handlers/handlers/simple");
}

// Test that a click on target='_blank' link in an app's window launches
// another app that has matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       BlankClickInAppIntercepted) {
  TestNavigationInApp("url_handlers/launchers/click_blank_link",
                      "Launcher done", "url_handlers/handlers/simple",
                      "Handler launched");
}

// Test that a call to window.open() in the app's foreground page launches
// another app that has matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       WindowOpenInAppIntercepted) {
  TestNavigationInApp("url_handlers/launchers/call_window_open",
                      "Launcher done", "url_handlers/handlers/simple",
                      "Handler launched");
}

// Test that an app with url_handlers does not intercept a mismatching
// click on a target='_blank' link in another app's window.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       MismatchingWindowOpenInAppNotIntercepted) {
  TestMismatchingNavigationInApp(
      "url_handlers/launchers/call_mismatching_window_open", "Launcher done",
      "url_handlers/handlers/simple");
}

// Test that a webview in an app can be navigated to a URL without interception
// even when there are other (or the same) apps that have matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       WebviewNavigationNotIntercepted) {
  // The launcher clicks on a link, which gets intercepted and launches the
  // handler. The handler also redirects an embedded webview to the URL. The
  // webview should just navigate without creating an endless loop of
  // navigate-intercept-launch sequences with multiplying handler's windows.
  // There should be 2 windows only: launcher's and handler's.
  TestNavigationInApp(
      "url_handlers/launchers/click_blank_link", "Launcher done",
      "url_handlers/handlers/navigate_webview_to_url", "Handler launched");
}

// Test that a webview in an app can be navigated to a URL without interception
// even when there are other (or the same) apps that have matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       MismatchingBlankClickInAppNotIntercepted) {
  // The launcher clicks on a link, which gets intercepted and launches the
  // handler. The handler also redirects an embedded webview to the URL. The
  // webview should just navigate without creating an endless loop of
  // navigate-intercept-launch sequences with multiplying handler's windows.
  // There should be 2 windows only: launcher's and handler's.
  TestMismatchingNavigationInApp(
      "url_handlers/launchers/click_mismatching_blank_link", "Launcher done",
      "url_handlers/handlers/simple");
}

// Test that a URL entry in the omnibar launches an app that has matching
// url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       EntryInOmnibarIntercepted) {
  TestNavigationInBrowser("url_handlers/common/target.html",
                          ui::PAGE_TRANSITION_TYPED,
                          "url_handlers/handlers/simple", "Handler launched");
}

// Test that an app with url_handlers does not intercept a mismatching
// URL entry in the omnibar.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       MismatchingEntryInOmnibarNotIntercepted) {
  TestMismatchingNavigationInBrowser(
      "url_handlers/common/mismatching_target.html", ui::PAGE_TRANSITION_TYPED,
      "Mismatching link target loaded", "url_handlers/handlers/simple");
}

// Test that a form submission in a page is never subject to interception
// by apps even with matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       FormSubmissionInTabNotIntercepted) {
  TestMismatchingNavigationInTab(
      "url_handlers/launching_pages/submit_form.html", "Link target loaded",
      "url_handlers/handlers/simple");
}

// Test that a form submission in a page is never subject to interception
// by apps even with matching url_handlers.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorBrowserTest,
                       XhrInTabNotIntercepted) {
  TestNegativeXhrInTab("url_handlers/xhr_downloader/main.html", "XHR succeeded",
                       "XHR failed", "url_handlers/handlers/steal_xhr_target");
}

class PlatformAppNavigationRedirectorPrerenderingBrowserTest
    : public PlatformAppNavigationRedirectorBrowserTest {
 public:
  PlatformAppNavigationRedirectorPrerenderingBrowserTest()
      : prerender_helper_(base::BindRepeating(
            &PlatformAppNavigationRedirectorPrerenderingBrowserTest::
                GetWebContents,
            base::Unretained(this))) {}
  ~PlatformAppNavigationRedirectorPrerenderingBrowserTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::PrerenderTestHelper& prerender_helper() {
    return prerender_helper_;
  }

 private:
  content::test::PrerenderTestHelper prerender_helper_;
};

// Test that prerendering doesn't launch an app but aborts the navigation.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorPrerenderingBrowserTest,
                       DoNotLaunchAppInPrerendering) {
  ASSERT_TRUE(StartEmbeddedTestServer());
  const char* handler = "url_handlers/handlers/simple";
  InstallPlatformApp(handler);

  const auto initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  EXPECT_EQ(initial_url, GetWebContents()->GetLastCommittedURL());

  const auto prerender_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/url_handlers/common/target.html");

  // Loading an app URL in prerendering cancels prerendering.
  prerender_helper().AddPrerenderAsync(prerender_url);
  content::test::PrerenderHostObserver host_observer(*GetWebContents(),
                                                     prerender_url);
  // Wait until PrerenderHost is destroyed by canceling prerendering.
  host_observer.WaitForDestroyed();

  // The primary page doesn't have any change.
  EXPECT_EQ(initial_url, GetWebContents()->GetLastCommittedURL());
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(0U, GetAppWindowCount());
}

class PlatformAppNavigationRedirectorFencedFrameBrowserTest
    : public PlatformAppNavigationRedirectorBrowserTest {
 public:
  PlatformAppNavigationRedirectorFencedFrameBrowserTest() = default;
  ~PlatformAppNavigationRedirectorFencedFrameBrowserTest() override = default;

  content::WebContents* GetWebContents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  content::test::FencedFrameTestHelper& fenced_frame_test_helper() {
    return fenced_frame_test_helper_;
  }

 private:
  content::test::FencedFrameTestHelper fenced_frame_test_helper_;
};

// Test that PlatformAppNavigationRedirector doesn't apply to fenced frames.
IN_PROC_BROWSER_TEST_F(PlatformAppNavigationRedirectorFencedFrameBrowserTest,
                       DoNotLaunchAppInFencedFrames) {
  // Set a response for a fenced frame.
  embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
      [](const net::test_server::HttpRequest& request)
          -> std::unique_ptr<net::test_server::HttpResponse> {
        if (request.GetURL().ExtractFileName() != "target.html") {
          return {};
        }
        auto response = std::make_unique<net::test_server::BasicHttpResponse>();
        response->set_content_type("text/html");
        response->AddCustomHeader("Supports-Loading-Mode", "fenced-frame");
        return response;
      }));

  ASSERT_TRUE(StartEmbeddedTestServer());
  const char* handler = "url_handlers/handlers/simple";
  InstallPlatformApp(handler);

  const GURL initial_url = embedded_test_server()->GetURL("/empty.html");
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), initial_url));
  EXPECT_EQ(initial_url, GetWebContents()->GetLastCommittedURL());

  // Load an app URL in a fenced frame.
  const GURL fenced_frame_url = embedded_test_server()->GetURL(
      "/extensions/platform_apps/url_handlers/common/target.html");
  content::RenderFrameHost* fenced_frame_rfh =
      fenced_frame_test_helper().CreateFencedFrame(
          GetWebContents()->GetPrimaryMainFrame(), fenced_frame_url);

  // Ensure that a fenced frame doesn't launch an app and the page is opened
  // in a fenced frame.
  EXPECT_EQ(1, browser()->tab_strip_model()->count());
  EXPECT_EQ(fenced_frame_rfh->GetLastCommittedURL(), fenced_frame_url);
  EXPECT_EQ(fenced_frame_rfh->GetParentOrOuterDocument(),
            GetWebContents()->GetPrimaryMainFrame());
  EXPECT_EQ(0U, GetAppWindowCount());
}

}  // namespace extensions
