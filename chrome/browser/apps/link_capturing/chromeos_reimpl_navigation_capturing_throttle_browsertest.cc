// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/link_capturing/chromeos_reimpl_navigation_capturing_throttle.h"

#include <string>

#include "base/strings/string_util.h"
#include "base/test/scoped_feature_list.h"
#include "build/build_config.h"
#include "chrome/browser/apps/app_service/app_registry_cache_waiter.h"
#include "chrome/browser/apps/intent_helper/preferred_apps_test_util.h"
#include "chrome/browser/apps/link_capturing/link_capturing_feature_test_support.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser_window/public/browser_window_interface.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/ui/web_applications/app_browser_controller.h"
#include "chrome/browser/ui/web_applications/test/web_app_browsertest_util.h"
#include "chrome/browser/ui/web_applications/web_app_browsertest_base.h"
#include "chrome/browser/web_applications/chromeos_web_app_experiments.h"
#include "chrome/browser/web_applications/test/web_app_install_test_utils.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/webapps/common/web_app_id.h"
#include "content/public/browser/web_contents.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/input/web_input_event.h"
#include "third_party/blink/public/common/input/web_mouse_event.h"
#include "ui/base/window_open_disposition.h"
#include "url/gurl.h"
#include "url/origin.h"

static_assert(BUILDFLAG(IS_CHROMEOS), "For ChromeOS only");

namespace apps {
namespace {

using ::testing::HasSubstr;
using ::testing::Not;

constexpr char kAppHost[] = "app.site.test";
constexpr char kSameSiteOtherHost[] = "other.site.test";
constexpr char kAttackerHost[] = "attacker.example";

class ChromeOsReimplNavigationCapturingThrottleBrowserTest
    : public web_app::WebAppBrowserTestBase {
 public:
  ChromeOsReimplNavigationCapturingThrottleBrowserTest() {
    scoped_feature_list_.InitWithFeaturesAndParameters(
        apps::test::GetFeaturesToEnableLinkCapturingUX(
            apps::test::LinkCapturingFeatureVersion::kV2DefaultOn),
        {});
  }
  ~ChromeOsReimplNavigationCapturingThrottleBrowserTest() override = default;

  void SetUpOnMainThread() override {
    web_app::WebAppBrowserTestBase::SetUpOnMainThread();

    // Enable `ChromeOsWebAppExperiments` for the installed web app so that
    // in-tab link clicks to `extended_scope_` are intercepted by
    // `ChromeOsReimplNavigationCapturingThrottle` and relaunched into a
    // standalone app window via AppService.
    extended_scope_ = embedded_https_test_server().GetURL(kAppHost, "/");
    web_app::ChromeOsWebAppExperiments::SetAlwaysEnabledForTesting();
    web_app::ChromeOsWebAppExperiments::SetScopeExtensionsForTesting(
        {extended_scope_.spec().c_str()});

    const GURL start_url =
        embedded_https_test_server().GetURL(kAppHost, "/web_apps/basic.html");
    auto web_app_info =
        web_app::WebAppInstallInfo::CreateWithStartUrlForTesting(start_url);
    web_app_info->scope =
        embedded_https_test_server().GetURL(kAppHost, "/web_apps/");
    web_app_info->user_display_mode =
        web_app::mojom::UserDisplayMode::kStandalone;
    app_id_ = web_app::test::InstallWebApp(profile(), std::move(web_app_info));

    apps::AppReadinessWaiter(profile(), app_id_).Await();
    apps_util::SetSupportedLinksPreferenceAndWait(profile(), app_id_);
  }

  void TearDownOnMainThread() override {
    web_app::ChromeOsWebAppExperiments::ClearOverridesForTesting();
    web_app::WebAppBrowserTestBase::TearDownOnMainThread();
  }

 protected:
  GURL TargetUrl() const {
    // `/echoall` is inside `extended_scope_` (`https://app.site.test/`) and
    // echoes back the request headers (including `Cookie`, `Sec-Fetch-Site`,
    // and `Referer`) inside `#request-headers`.
    return embedded_https_test_server().GetURL(kAppHost, "/echoall");
  }

  GURL AttackerUrl() const {
    return embedded_https_test_server().GetURL(kAttackerHost, "/title1.html");
  }

  GURL SameSiteOutOfScopeUrl() const {
    return embedded_https_test_server().GetURL(kSameSiteOtherHost,
                                               "/title1.html");
  }

  void SetTestCookies() {
    ASSERT_TRUE(content::SetCookie(
        profile(), extended_scope_,
        "samesite_strict=1; SameSite=Strict; Secure; Path=/"));
    ASSERT_TRUE(
        content::SetCookie(profile(), extended_scope_,
                           "samesite_lax=1; SameSite=Lax; Secure; Path=/"));
  }

  std::string ExtractLowercasedRequestHeaders(
      content::WebContents* app_contents) {
    return base::ToLowerASCII(
        content::EvalJs(app_contents,
                        "document.getElementById('request-headers').innerText")
            .ExtractString());
  }

  // Navigates the active browser tab to `source_url`, injects an `<a
  // target="_self">` pointing to `link_url` with optional `rel` and
  // `referrer_policy`, simulates a real mouse click on it, waits for the
  // captured app window to finish loading `TargetUrl()`, and returns the
  // lowercased `#request-headers` text of the launched app's document.
  std::string ClickSelfLinkAndGetLaunchedAppHeaders(
      const GURL& source_url,
      const GURL& link_url,
      const std::string& rel,
      const std::string& referrer_policy = "") {
    EXPECT_TRUE(ui_test_utils::NavigateToURL(browser(), source_url));
    content::WebContents* source_contents =
        browser()->GetTabStripModel()->GetActiveWebContents();

    const std::string create_link_script = content::JsReplace(
        R"(
          (() => {
            const a = document.createElement('a');
            a.href = $1;
            a.target = '_self';
            a.rel = $2;
            a.referrerPolicy = $3;
            a.style.position = 'fixed';
            a.style.top = '0';
            a.style.left = '0';
            a.style.width = '100vw';
            a.style.height = '100vh';
            a.textContent = 'click me';
            document.body.appendChild(a);
          })();
        )",
        link_url, rel, referrer_policy);
    EXPECT_TRUE(content::ExecJs(source_contents, create_link_script));
    content::SimulateEndOfPaintHoldingOnPrimaryMainFrame(source_contents);

    ui_test_utils::BrowserCreatedObserver browser_created_observer;
    content::TestNavigationObserver nav_observer(TargetUrl());
    nav_observer.StartWatchingNewWebContents();

    content::SimulateMouseClick(source_contents,
                                blink::WebInputEvent::Modifiers::kNoModifiers,
                                blink::WebMouseEvent::Button::kLeft);

    BrowserWindowInterface* app_browser = browser_created_observer.Wait();
    nav_observer.Wait();

    EXPECT_TRUE(app_browser);
    EXPECT_NE(app_browser, browser());
    EXPECT_TRUE(
        web_app::AppBrowserController::IsForWebApp(app_browser, app_id_));
    // The source tab was navigated with user activation, so it remains on
    // `source_url` after the captured navigation is cancelled.
    EXPECT_EQ(source_contents->GetLastCommittedURL(), source_url);

    content::WebContents* app_contents =
        app_browser->GetTabStripModel()->GetActiveWebContents();
    EXPECT_EQ(app_contents->GetLastCommittedURL(), TargetUrl());

    return ExtractLowercasedRequestHeaders(app_contents);
  }

  webapps::AppId app_id_;
  GURL extended_scope_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

// Regression test for https://crbug.com/569008661: Verify that when a
// cross-site link click is intercepted by
// `ChromeOsReimplNavigationCapturingThrottle` and relaunched in a standalone
// web app window, the cross-site `initiator_origin` and sanitized `referrer`
// are preserved so `Sec-Fetch-Site` is `cross-site` and `SameSite=Strict`
// cookies are NOT attached.
IN_PROC_BROWSER_TEST_F(ChromeOsReimplNavigationCapturingThrottleBrowserTest,
                       CrossSiteLinkClickDoesNotSendSameSiteStrictCookie) {
  SetTestCookies();

  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      AttackerUrl(), TargetUrl(), /*rel=*/"");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));

  // Default referrer policy (`strict-origin-when-cross-origin`) strips the
  // cross-origin referrer URL to the initiator's origin.
  const std::string expected_origin_referer =
      url::Origin::Create(AttackerUrl()).GetURL().spec();
  EXPECT_THAT(headers, HasSubstr("referer: " + expected_origin_referer));
  EXPECT_THAT(headers, Not(HasSubstr("title1.html")));
}

// Regression test for https://crbug.com/569008661: Verify that a cross-site
// link with `rel="noreferrer"` omits the `Referer` header while still
// preserving the cross-site `initiator_origin` (`Sec-Fetch-Site: cross-site`)
// and blocking `SameSite=Strict` cookies.
IN_PROC_BROWSER_TEST_F(
    ChromeOsReimplNavigationCapturingThrottleBrowserTest,
    CrossSiteNoReferrerLinkClickStillPreservesCrossSiteInitiator) {
  SetTestCookies();

  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      AttackerUrl(), TargetUrl(), /*rel=*/"noreferrer");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));
  EXPECT_THAT(headers, Not(HasSubstr("referer:")));
}

// Regression test for https://crbug.com/569008661: Verify that a cross-site
// link click that undergoes an HTTP server redirect into the captured app scope
// (intercepted in `WillRedirectRequest()`) also preserves the cross-site
// `initiator_origin` and does not attach `SameSite=Strict` cookies.
IN_PROC_BROWSER_TEST_F(ChromeOsReimplNavigationCapturingThrottleBrowserTest,
                       CrossSiteServerRedirectDoesNotSendSameSiteStrictCookie) {
  SetTestCookies();

  const GURL redirect_url = embedded_https_test_server().GetURL(
      kAttackerHost, "/server-redirect?" + TargetUrl().spec());

  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      AttackerUrl(), redirect_url, /*rel=*/"");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));

  const std::string expected_origin_referer =
      url::Origin::Create(AttackerUrl()).GetURL().spec();
  EXPECT_THAT(headers, HasSubstr("referer: " + expected_origin_referer));
}

// Regression test for https://crbug.com/569008661: Verify that a cross-site
// script-initiated navigation without user gesture (`location.href = ...` with
// `EXECUTE_SCRIPT_NO_USER_GESTURE`), which triggers the dangling-WebContents
// `ClosePage()` branch in `ChromeOsReimplNavigationCapturingThrottle`,
// snapshots and preserves the cross-site `initiator_origin` and `referrer`
// before `ClosePage()` runs, and does not send `SameSite=Strict` cookies.
IN_PROC_BROWSER_TEST_F(
    ChromeOsReimplNavigationCapturingThrottleBrowserTest,
    CrossSiteScriptNavigationWithoutGestureDoesNotSendSameSiteStrictCookie) {
  SetTestCookies();

  // Open the attacker page in a second tab because the no-gesture script
  // navigation causes `IsEmptyDanglingWebContentsAfterLinkCapture()` to return
  // true and `ClosePage()` to close the navigating tab.
  ASSERT_TRUE(ui_test_utils::NavigateToURLWithDisposition(
      browser(), AttackerUrl(), WindowOpenDisposition::kNewForegroundTab,
      ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP));
  ASSERT_EQ(browser()->GetTabStripModel()->count(), 2);
  content::WebContents* attacker_contents =
      browser()->GetTabStripModel()->GetActiveWebContents();
  ASSERT_EQ(attacker_contents->GetLastCommittedURL(), AttackerUrl());

  ui_test_utils::BrowserCreatedObserver browser_created_observer;
  content::TestNavigationObserver nav_observer(TargetUrl());
  nav_observer.StartWatchingNewWebContents();

  ASSERT_TRUE(content::ExecJs(
      attacker_contents, content::JsReplace("location.href = $1;", TargetUrl()),
      content::EXECUTE_SCRIPT_NO_USER_GESTURE));

  BrowserWindowInterface* app_browser = browser_created_observer.Wait();
  nav_observer.Wait();

  ASSERT_TRUE(app_browser);
  EXPECT_NE(app_browser, browser());
  EXPECT_TRUE(web_app::AppBrowserController::IsForWebApp(app_browser, app_id_));

  content::WebContents* app_contents =
      app_browser->GetTabStripModel()->GetActiveWebContents();
  EXPECT_EQ(app_contents->GetLastCommittedURL(), TargetUrl());

  const std::string headers = ExtractLowercasedRequestHeaders(app_contents);

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));

  const std::string expected_origin_referer =
      url::Origin::Create(AttackerUrl()).GetURL().spec();
  EXPECT_THAT(headers, HasSubstr("referer: " + expected_origin_referer));
  EXPECT_THAT(headers, Not(HasSubstr("title1.html")));
}

// Regression test for https://crbug.com/569008661: Verify that a same-site (but
// out-of-scope) link click that is captured into the web app window preserves
// the same-site initiator origin (`Sec-Fetch-Site: same-site`) and still sends
// both `SameSite=Lax` and `SameSite=Strict` cookies (no over-blocking).
IN_PROC_BROWSER_TEST_F(ChromeOsReimplNavigationCapturingThrottleBrowserTest,
                       SameSiteLinkClickSendsSameSiteStrictCookie) {
  SetTestCookies();

  // `other.site.test` is same-site with `app.site.test` (registrable domain
  // `site.test`), but outside the app's manifest scope and ChromeOS extended
  // scope (`https://app.site.test/`), so clicking a link to `TargetUrl()` is
  // captured into a new app window.
  const GURL same_site_source_url = SameSiteOutOfScopeUrl();
  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      same_site_source_url, TargetUrl(), /*rel=*/"");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: same-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, HasSubstr("samesite_strict=1"));

  const std::string expected_origin_referer =
      url::Origin::Create(same_site_source_url).GetURL().spec();
  EXPECT_THAT(headers, HasSubstr("referer: " + expected_origin_referer));
  EXPECT_THAT(headers, Not(HasSubstr("title1.html")));
}

// Regression test for https://crbug.com/569008661: Verify that a cross-site
// link click with `referrerPolicy="unsafe-url"` transfers the referrer policy
// across the captured app launch and preserves the full `.../title1.html`
// referrer URL.
IN_PROC_BROWSER_TEST_F(
    ChromeOsReimplNavigationCapturingThrottleBrowserTest,
    CrossSiteLinkClickWithUnsafeUrlReferrerPolicyPreservesFullReferrer) {
  SetTestCookies();

  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      AttackerUrl(), TargetUrl(), /*rel=*/"",
      /*referrer_policy=*/"unsafe-url");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));
  EXPECT_THAT(headers, HasSubstr("referer: " + AttackerUrl().spec()));
}

// Regression test for https://crbug.com/569008661: Verify that when a
// navigation starts on a same-site page (`other.site.test`), redirects through
// a cross-site host (`attacker.example`), and lands in the captured app scope
// (`app.site.test`), the relaunched navigation downgrades `initiator_origin` to
// an opaque origin so `Sec-Fetch-Site` is `cross-site` and `SameSite=Strict`
// cookies are not sent.
IN_PROC_BROWSER_TEST_F(
    ChromeOsReimplNavigationCapturingThrottleBrowserTest,
    SameSiteRedirectThroughCrossSiteDowngradesInitiatorToCrossSite) {
  SetTestCookies();

  const GURL cross_site_redirect_url = embedded_https_test_server().GetURL(
      kAttackerHost, "/server-redirect?" + TargetUrl().spec());

  const std::string headers = ClickSelfLinkAndGetLaunchedAppHeaders(
      SameSiteOutOfScopeUrl(), cross_site_redirect_url, /*rel=*/"");

  EXPECT_THAT(headers, HasSubstr("sec-fetch-site: cross-site"));
  EXPECT_THAT(headers, HasSubstr("samesite_lax=1"));
  EXPECT_THAT(headers, Not(HasSubstr("samesite_strict=1")));
}

}  // namespace
}  // namespace apps
