// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/link_capturing_navigation_throttle.h"

#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/web_applications/os_integration/os_integration_manager.h"
#include "chrome/browser/web_applications/test/os_integration_test_override_impl.h"
#include "chrome/browser/web_applications/test/web_app_test_observers.h"
#include "chrome/browser/web_applications/web_app_tab_helper.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_features.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/switches.h"
#include "url/url_constants.h"

namespace apps {
namespace {

enum class NavCaptureVersion { kV1, kV2 };

std::string NavCaptureVersionToString(
    const testing::TestParamInfo<NavCaptureVersion>& version) {
  switch (version.param) {
    case NavCaptureVersion::kV1:
      return "V1";
    case NavCaptureVersion::kV2:
      return "V2";
  }
}

// TODO(https://issues.chromium.org/329174385): Test more end-to-end the state
// of browser tabs rather than relying on testing callbacks, this requires
// resolving flakiness with awaiting web app launches.
class LinkCapturingNavigationThrottleBrowserTest
    : public InProcessBrowserTest,
      public testing::WithParamInterface<NavCaptureVersion> {
 public:
  LinkCapturingNavigationThrottleBrowserTest() {
    feature_list_.InitWithFeaturesAndParameters(
        /*enabled_features=*/{base::test::FeatureRefAndParams(
            features::kPwaNavigationCapturing,
            {{"link_capturing_state",
              IsV1() ? "on_by_default" : "reimpl_default_on"}})},
        /*disabled_features=*/{
            blink::features::kDropInputEventsBeforeFirstPaint});
  }

  bool IsV1() { return GetParam() == NavCaptureVersion::kV1; }

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

  void SetUpDefaultCommandLine(base::CommandLine* command_line) override {
    InProcessBrowserTest::SetUpDefaultCommandLine(command_line);
    command_line->AppendSwitch(blink::switches::kAllowPreCommitInput);
  }

  content::WebContents* GetBrowserTab() {
    return browser()->tab_strip_model()->GetWebContentsAt(0);
  }

  content::WebContents* ClickPageAndWaitForStartUrlLoad() {
    test::NavigationCommittedForUrlObserver load_observer(start_url_);
    content::SimulateMouseClick(GetBrowserTab(), /*modifiers=*/0,
                                blink::WebMouseEvent::Button::kLeft);
    load_observer.Wait();
    return load_observer.web_contents();
  }

 protected:
  GURL simple_url_;
  GURL start_url_;
  webapps::AppId app_id_;
  base::test::ScopedFeatureList feature_list_;
  web_app::OsIntegrationTestOverrideBlockingRegistration faked_os_integration_;
};

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  EXPECT_TRUE(web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                  ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();

  bool expected_open_in_pwa_window = IsV1();
  ASSERT_TRUE(loaded_contents);
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  EXPECT_TRUE(web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                  ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
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

  content::WebContents* loaded_contents = ClickPageAndWaitForStartUrlLoad();
  ASSERT_TRUE(loaded_contents);

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(expected_open_in_pwa_window,
            web_app::WebAppTabHelper::FromWebContents(loaded_contents)
                ->is_in_app_window());
}

IN_PROC_BROWSER_TEST_P(LinkCapturingNavigationThrottleBrowserTest,
                       TriggeredByNoClickJavaScriptWindowOpen) {
  ui_test_utils::UrlLoadObserver load_observer(start_url_);
  ASSERT_TRUE(content::ExecJs(
      GetBrowserTab(),
      base::StringPrintf("window.open('%s')", start_url_.spec().c_str())));
  load_observer.Wait();
  ASSERT_TRUE(load_observer.web_contents());

  bool expected_open_in_pwa_window = IsV1();
  EXPECT_EQ(
      expected_open_in_pwa_window,
      web_app::WebAppTabHelper::FromWebContents(load_observer.web_contents())
          ->is_in_app_window());
}

INSTANTIATE_TEST_SUITE_P(,
                         LinkCapturingNavigationThrottleBrowserTest,
                         testing::Values(NavCaptureVersion::kV1,
                                         NavCaptureVersion::kV2),
                         NavCaptureVersionToString);

}  // namespace
}  // namespace apps
