// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "url/url_constants.h"

namespace apps {
namespace {

// TODO(https://issues.chromium.org/329174385): Test more end-to-end the state
// of browser tabs rather than relying on testing callbacks, this requires
// resolving flakiness with awaiting web app launches.
class LinkCapturingNavigationThrottleBrowserTest : public InProcessBrowserTest {
 public:
  void SetUpOnMainThread() override {
#if BUILDFLAG(IS_CHROMEOS_LACROS) || \
    (BUILDFLAG(IS_LINUX) && defined(MEMORY_SANITIZER)) || BUILDFLAG(IS_MAC) ||\
    (BUILDFLAG(IS_WIN) && defined(ADDRESS_SANITIZER))
    // TODO(https://issues.chromium.org/329174385): Deflake tests in
    // lacros_chrome_browser_tests / Linux MSan Tests / Mac Tests / Win ASan
    // Tests.
    GTEST_SKIP();
#else
    InProcessBrowserTest::SetUpOnMainThread();

    ASSERT_TRUE(embedded_test_server()->Start());

    // Set up web app as link capturing target.
    start_url_ = embedded_test_server()->GetURL("/web_apps/basic.html");
    app_id_ =
        web_app::InstallWebAppFromPageAndCloseAppBrowser(browser(), start_url_);
    apps::AppReadinessWaiter(browser()->profile(), app_id_).Await();
    ASSERT_TRUE(test::EnableLinkCapturingByUser(browser()->profile(), app_id_)
                    .has_value());

    // Set up simple page browser tab with a "fill-page" CSS class available for
    // tests to use. This class makes elements fill the page making it easy to
    // click on.
    simple_url_ = embedded_test_server()->GetURL("/simple.html");
    ASSERT_TRUE(content::NavigateToURL(GetBrowserTab(), simple_url_));
    ASSERT_TRUE(content::ExecJs(GetBrowserTab(), R"(
      const style = document.createElement('style');
      style.textContent = `
        .fill-page {
          position: absolute;
          left: 0;
          top: 0;
          width: 100vw;
          height: 100vh;
        }
      `;
      document.head.append(style);
    )"));
#endif
  }

  content::WebContents* GetBrowserTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  void ClickPage() {
    content::SimulateMouseClick(GetBrowserTab(), /*modifiers=*/0,
                                blink::WebMouseEvent::Button::kLeft);
  }

 protected:
  GURL simple_url_;
  GURL start_url_;
  webapps::AppId app_id_;
  base::test::ScopedFeatureList feature_list_{
      features::kPwaNavigationCapturing};
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClick) {
  // Insert an <a href> link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_FALSE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlank) {
  // Insert an <a href target="_blank"> link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickServerRedirect) {
  // Insert an <a href> server redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        document.body.append(link);
      )",
      embedded_test_server()
          ->GetURL("/server-redirect?" + start_url_.spec())
          .spec()
          .c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_FALSE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlankServerRedirect) {
  // Insert an <a href target="_blank"> server redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      embedded_test_server()
          ->GetURL("/server-redirect?" + start_url_.spec())
          .spec()
          .c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByAHrefClickTargetBlankJavaScriptRedirect) {
  // Insert an <a href target="_blank"> JavaScript redirect link to click on.
  std::string script = base::StringPrintf(
      R"(
        const link = document.createElement('a');
        link.className = 'fill-page';
        link.href = '/navigate_to.html?%s';
        link.target = '_blank';
        document.body.append(link);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByButtonClickJavaScriptWindowOpen) {
  // Insert a window.open(url) button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          window.open('%s');
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(
    LinkCapturingNavigationThrottleBrowserTest,
    TriggeredByButtonClickJavaScriptWindowOpenAboutBlankLocationHrefUrl) {
  // Insert a window.open('about:blank').location.href = url button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          window.open('about:blank').location.href = '%s';
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByButtonClickJavaScriptLocationHrefUrl) {
  // Insert a location.href = url button to click on.
  std::string script = base::StringPrintf(
      R"(
        const button = document.createElement('button');
        button.className = 'fill-page';
        button.addEventListener('click', event => {
          location.href = '%s';
        });
        document.body.append(button);
      )",
      start_url_.spec().c_str());
  ASSERT_TRUE(content::ExecJs(GetBrowserTab(), script));

  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ClickPage();

  EXPECT_FALSE(future.Get<bool /*closed_web_contents*/>());
}

IN_PROC_BROWSER_TEST_F(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByNoClickJavaScriptWindowOpen) {
  base::test::TestFuture<bool /*closed_web_contents*/> future;
  apps::LinkCapturingNavigationThrottle::
      GetLinkCaptureLaunchCallbackForTesting() = future.GetCallback();

  ASSERT_TRUE(content::ExecJs(
      GetBrowserTab(),
      base::StringPrintf("window.open('%s')", start_url_.spec().c_str())));

  EXPECT_TRUE(future.Get<bool /*closed_web_contents*/>());
}

}  // namespace
}  // namespace apps
