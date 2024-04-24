// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/tab_contents/navigation_metrics_recorder.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/privacy_sandbox/tracking_protection_prefs.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;

namespace {

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";
const char* kEchoCookiesWithCorsPath = "/echocookieswithcors";

bool ThirdPartyPartitionedStorageAllowedByDefault() {
  return base::FeatureList::IsEnabled(
             net::features::kThirdPartyPartitionedStorageAllowedByDefault) &&
         base::FeatureList::IsEnabled(
             net::features::kThirdPartyStoragePartitioning);
}

bool ThirdPartyPartitionedStorageAllowedByStorageAccessAPI() {
  return base::FeatureList::IsEnabled(
      net::features::kThirdPartyStoragePartitioning);
}

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 public:
  CookiePolicyBrowserTest(const CookiePolicyBrowserTest&) = delete;
  CookiePolicyBrowserTest& operator=(const CookiePolicyBrowserTest&) = delete;

  void SetBlockThirdPartyCookies() {
    browser()->profile()->GetPrefs()->SetBoolean(
        prefs::kTrackingProtection3pcdEnabled, true);
  }

 protected:
  CookiePolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  virtual std::vector<base::test::FeatureRef> EnabledFeatures() {
    return {blink::features::kWebSQLAccess};
  }

  virtual std::vector<base::test::FeatureRef> DisabledFeatures() { return {}; }

  void SetUp() override {
    // TODO(crbug.com/333756088): WebSQL is disabled everywhere by default as of
    // M119 (crbug/695592) except on Android WebView. This is enabled for
    // Android only to indirectly cover WebSQL deletions on WebView.
    feature_list_.InitWithFeatures(EnabledFeatures(), DisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  GURL GetURL(const std::string& host) {
    return https_server_.GetURL(host, "/");
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateToNewTabWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  content_settings::CookieSettings* cookie_settings() {
    return CookieSettingsFactory::GetForProfile(browser()->profile()).get();
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  std::string GetFrameContent() {
    return storage::test::GetFrameContent(GetFrame());
  }

  void SetCookieViaJS(content::RenderFrameHost* frame,
                      const std::string& cookie) {
    content::EvalJsResult result =
        EvalJs(frame, base::StrCat({"document.cookie = '", cookie, "'"}));
    ASSERT_TRUE(result.error.empty()) << result.error;
  }

  std::string GetCookieViaJS(content::RenderFrameHost* frame) {
    return EvalJs(frame, "document.cookie;").ExtractString();
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    GURL url(https_server_.GetURL(host, path));
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver load_observer(web_contents);
    ASSERT_TRUE(content::ExecJs(
        GetFrame(),
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  std::string GetNestedFrameContent() {
    return storage::test::GetFrameContent(GetNestedFrame());
  }

  content::RenderFrameHost* GetFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList feature_list_;
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  GURL url(https_server_.GetURL(kHostA, "/set-cookie?cookie1"));
  cookie_settings()->SetCookieSetting(url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  ASSERT_EQ("", content::GetCookies(browser()->profile(), url));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ("cookie1", content::GetCookies(browser()->profile(), url));
}

// Visits a page that is a redirect across domain boundary to a page that sets
// a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       AllowFirstPartyCookiesRedirect) {
  SetBlockThirdPartyCookies();

  GURL redirected_url(https_server_.GetURL(kHostB, "/set-cookie?cookie2"));

  // Redirect from a.test to b.test so it triggers third-party cookie blocking
  // if the first party for cookies URL is not changed when we follow a
  // redirect.

  ASSERT_EQ("", content::GetCookies(browser()->profile(), redirected_url));

  // This cookie can be set even if it is Lax-by-default because the redirect
  // counts as a top-level navigation and therefore the context is lax.
  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(), GURL(https_server_.GetURL(kHostA, "/server-redirect?").spec() +
                      redirected_url.spec())));

  EXPECT_EQ("cookie2",
            content::GetCookies(browser()->profile(), redirected_url));
}

// Third-Party Frame Tests
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowSetting) {
  NavigateToPageWithFrame(kHostA);
  cookie_settings()->SetCookieSetting(GetURL(kHostB),
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)), "");

  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // the cookie is set:
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo(kHostB, "/iframe.html");
  // Still need SameSite=None and Secure because the top-level is a.test so this
  // is still cross-site.
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=2;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=2");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=3;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=3");
}

// This test does the same navigations as the test above, so we can be assured
// that the cookies are actually blocked because of the
// block-third-party-cookies setting, and not just because of SameSite or
// whatever.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameBlockSetting) {
  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // the cookie is not set:
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)), "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=2;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)), "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=3;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)), "");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowReading) {
  cookie_settings()->SetCookieSetting(GetURL(kHostB),
                                      ContentSetting::CONTENT_SETTING_ALLOW);
  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echoes the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echoes the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");
}

// This test does the same navigations as the test above, so we can be assured
// that the cookies are actually blocked because of the
// block-third-party-cookies setting, and not just because of SameSite or
// whatever.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameBlockReading) {
  SetBlockThirdPartyCookies();

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echoes the cookie header, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echoes the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameExceptions) {
  SetBlockThirdPartyCookies();

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  // Set a cookie on d.test.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostD, "/"),
                     "thirdparty=other;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostD)),
            "thirdparty=other");

  // Allow all requests to b.test to have cookies.
  // On the other hand, d.test does not have an exception set for it.
  GURL url = https_server_.GetURL(kHostB, "/");
  cookie_settings()->SetCookieSetting(url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");
  // Navigate iframe to d.test and verify that the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echoes the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");
  // Navigate nested iframe to d.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echoes the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");
  // Navigate nested iframe to d.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies();

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  EXPECT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  // Allow all requests on the top frame domain a.test to have cookies.
  GURL url = https_server_.GetURL(kHostA, "/");
  cookie_settings()->SetThirdPartyCookieSetting(
      url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echoes the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echoes the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");

  // Now repeat the above with a different top frame site, which does not have
  // an exception set for it.
  NavigateToPageWithFrame(kHostD);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echoes the cookie header, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echoes the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
}

// Test third-party cookie blocking of features that allow to communicate
// between tabs such as SharedWorkers.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, MultiTabTest) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(
      GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault());

  // Allow all requests to b.test to access cookies.
  GURL a_url = https_server_.GetURL(kHostA, "/");
  GURL b_url = https_server_.GetURL(kHostB, "/");
  cookie_settings()->SetCookieSetting(b_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  // Remove ALLOW setting.
  cookie_settings()->ResetCookieSetting(b_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(
      GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault());

  // Allow all third-parties on a.test to access cookies.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
}

// Same as MultiTabTest but with a nested frame on a.test inside a b.test frame.
// The a.test frame should be treated as third-party although it matches the
// top-frame-origin.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, MultiTabNestedTest) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetNestedFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  bool expected_storage =
      ThirdPartyPartitionedStorageAllowedByDefault() ||
      ThirdPartyPartitionedStorageAllowedByStorageAccessAPI();

  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), expected_storage);

  // Allow all requests to a.test to access cookies.
  GURL a_url = https_server_.GetURL(kHostA, "/");
  cookie_settings()->SetCookieSetting(a_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);

  // Remove ALLOW setting.
  cookie_settings()->ResetCookieSetting(a_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), expected_storage);

  // Allow all third-parties on a.test to access cookies.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);
}

enum class ContextType { kFrame, kWorker };

class CookiePolicyStorageBrowserTest
    : public CookiePolicyBrowserTest,
      public testing::WithParamInterface<ContextType> {
 protected:
  void ExpectStorage(content::RenderFrameHost* frame,
                     bool expected_storage,
                     bool expected_cookie) {
    switch (ContextType()) {
      case ContextType::kFrame:
        storage::test::ExpectStorageForFrame(frame, expected_storage);
        EXPECT_EQ(expected_cookie && !Is3pcd(),
                  content::EvalJs(frame, "hasCookie()"));
        return;
      case ContextType::kWorker:
        storage::test::ExpectStorageForWorker(frame, expected_storage);
        return;
    }
  }

  void SetStorage(content::RenderFrameHost* frame) {
    switch (ContextType()) {
      case ContextType::kFrame:
        storage::test::SetStorageForFrame(frame, /*include_cookies=*/!Is3pcd());
        return;
      case ContextType::kWorker:
        storage::test::SetStorageForWorker(frame);
        return;
    }
  }

  bool Is3pcd() {
    return base::FeatureList::IsEnabled(
        content_settings::features::kTrackingProtection3pcd);
  }

  ContextType ContextType() const { return GetParam(); }
};

IN_PROC_BROWSER_TEST_P(CookiePolicyStorageBrowserTest,
                       ThirdPartyIFrameStorage) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), /*expected_storage=*/false,
                /*expected_cookie=*/false);
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault(),
                /*expected_cookie=*/false);

  // Allow all requests to b.test to access storage.
  GURL a_url = https_server_.GetURL(kHostA, "/");
  GURL b_url = https_server_.GetURL(kHostB, "/");
  cookie_settings()->SetCookieSetting(b_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  // Remove ALLOW setting.
  cookie_settings()->ResetCookieSetting(b_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault(),
                /*expected_cookie=*/false);

  // Allow all third-parties on a.test to access storage.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);
}

IN_PROC_BROWSER_TEST_P(CookiePolicyStorageBrowserTest,
                       NestedThirdPartyIFrameStorage) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), /*expected_storage=*/false,
                /*expected_cookie=*/false);
  SetStorage(GetNestedFrame());
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(),
                ThirdPartyPartitionedStorageAllowedByDefault(),
                /*expected_cookie=*/false);

  // Allow all requests to b.test to access storage.
  GURL a_url = https_server_.GetURL(kHostA, "/");
  GURL c_url = https_server_.GetURL(kHostC, "/");
  cookie_settings()->SetCookieSetting(c_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  // Remove ALLOW setting.
  cookie_settings()->ResetCookieSetting(c_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(),
                ThirdPartyPartitionedStorageAllowedByDefault(),
                /*expected_cookie=*/false);

  // Allow all third-parties on a.test to access storage.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);
}

class ThirdPartyPartitionedStorageAccessibilityTest
    : public CookiePolicyBrowserTest,
      public testing::WithParamInterface<std::tuple<ContextType, bool>> {
 public:
  void ExpectStorage(content::RenderFrameHost* frame, bool expected_storage) {
    switch (ContextType()) {
      case ContextType::kFrame:
        storage::test::ExpectStorageForFrame(frame, expected_storage);
        return;
      case ContextType::kWorker:
        storage::test::ExpectStorageForWorker(frame, expected_storage);
        return;
    }
  }

  void SetStorage(content::RenderFrameHost* frame) {
    switch (ContextType()) {
      case ContextType::kFrame:
        storage::test::SetStorageForFrame(frame, /*include_cookies=*/false);
        return;
      case ContextType::kWorker:
        storage::test::SetStorageForWorker(frame);
        return;
    }
  }

  bool StoragePartitioningEnabled() const { return std::get<1>(GetParam()); }

 protected:
  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    if (StoragePartitioningEnabled()) {
      return {};
    }
    return {net::features::kThirdPartyStoragePartitioning,
            content_settings::features::kTrackingProtection3pcd};
  }

  ContextType ContextType() const { return std::get<0>(GetParam()); }

 private:
  base::test::ScopedFeatureList feature_list_;
};

// When third-party cookies are disabled, third-party storage should only be
// accessible when storage partitioning is enabled.
IN_PROC_BROWSER_TEST_P(ThirdPartyPartitionedStorageAccessibilityTest, Basic) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  SetBlockThirdPartyCookies();
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), StoragePartitioningEnabled());
}

// Partitioned third-party storage shouldn't be accessible when cookies are
// disabled by a user setting, even if 3p cookies are also disabled by
// default.
IN_PROC_BROWSER_TEST_P(ThirdPartyPartitionedStorageAccessibilityTest,
                       UserSetting) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  GURL b_url = https_server_.GetURL(kHostB, "/");
  cookie_settings()->SetCookieSetting(b_url,
                                      ContentSetting::CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), false);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), false);
}

// This "using" is to give the shared worker tests a different name in
// order to allow us to instantiate different parameters for them.
//
// This is because Shared worker tests don't depend on the ContextType, so we
// can just arbitrarily choose any single value. We don't want to use the same
// parameters as ThirdPartyPartitionedStorageAccessibilityTest as that would
// mean the shared worker tests would run twice as many times as necessary.
using ThirdPartyPartitionedStorageAccessibilitySharedWorkerTest =
    ThirdPartyPartitionedStorageAccessibilityTest;

IN_PROC_BROWSER_TEST_P(
    ThirdPartyPartitionedStorageAccessibilitySharedWorkerTest,
    Basic) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  // Create a second tab to test shared worker between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  SetBlockThirdPartyCookies();
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(),
                                            StoragePartitioningEnabled());
}

IN_PROC_BROWSER_TEST_P(
    ThirdPartyPartitionedStorageAccessibilitySharedWorkerTest,
    UserSetting) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  // Create a second tab to test shared worker between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  GURL b_url = https_server_.GetURL(kHostB, "/");
  cookie_settings()->SetCookieSetting(b_url,
                                      ContentSetting::CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
}

class ThirdPartyPartitionedStorageAccessibilityCanBeDisabledTest
    : public ThirdPartyPartitionedStorageAccessibilityTest {
 protected:
  std::vector<base::test::FeatureRef> DisabledFeatures() override {
    return {net::features::kThirdPartyPartitionedStorageAllowedByDefault,
            content_settings::features::kTrackingProtection3pcd};
  }
};

// Tests that even if partitioned third-party storage would otherwise be
// accessible, we can disable it with
// kThirdPartyPartitionedStorageAllowedByDefault.
IN_PROC_BROWSER_TEST_P(
    ThirdPartyPartitionedStorageAccessibilityCanBeDisabledTest,
    Basic) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  SetBlockThirdPartyCookies();
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  // Third-party storage is now always inaccessible.
  ExpectStorage(GetFrame(), false);
}

IN_PROC_BROWSER_TEST_P(CookiePolicyStorageBrowserTest,
                       NestedFirstPartyIFrameStorage) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), /*expected_storage=*/false,
                /*expected_cookie=*/false);
  SetStorage(GetNestedFrame());
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  SetBlockThirdPartyCookies();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  bool expected_storage =
      ThirdPartyPartitionedStorageAllowedByDefault() ||
      ThirdPartyPartitionedStorageAllowedByStorageAccessAPI();

  ExpectStorage(GetNestedFrame(), expected_storage,
                /*expected_cookie=*/false);

  // Allow all requests to b.test to access storage.
  GURL a_url = https_server_.GetURL(kHostA, "/");
  cookie_settings()->SetCookieSetting(a_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);

  // Remove ALLOW setting.
  cookie_settings()->ResetCookieSetting(a_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), expected_storage,
                /*expected_cookie=*/false);

  // Allow all third-parties on a.test to access storage.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), /*expected_storage=*/true,
                /*expected_cookie=*/true);
}

std::unique_ptr<net::test_server::HttpResponse>
HandleEchoCookiesWithCorsRequest(const net::test_server::HttpRequest& request) {
  if (request.relative_url != kEchoCookiesWithCorsPath) {
    return nullptr;
  }

  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::string content;

  // Get the 'Cookie' header that was sent in the request.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
      it != request.headers.end()) {
    content = it->second;
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  // Set the cors enabled headers.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kOrigin);
      it != request.headers.end()) {
    http_response->AddCustomHeader("Access-Control-Allow-Headers",
                                   "credentials");
    http_response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
    http_response->AddCustomHeader("Access-Control-Allow-Methods", "GET");
    http_response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
  }
  http_response->AddCustomHeader("Vary", "Origin");
  http_response->set_content(content);

  return http_response;
}

class ThirdPartyCookiePhaseoutPolicyStorageBrowserTest
    : public CookiePolicyBrowserTest {
 public:
  void SetUpOnMainThread() override {
    https_server()->RegisterRequestHandler(
        base::BindRepeating(&HandleEchoCookiesWithCorsRequest));
    CookiePolicyBrowserTest::SetUpOnMainThread();
  }

 protected:
  net::EmbeddedTestServer* https_server() { return &https_server_; }

  std::vector<base::test::FeatureRef> EnabledFeatures() override {
    return {blink::features::kWebSQLAccess,
            net::features::kForceThirdPartyCookieBlocking,
            net::features::kThirdPartyStoragePartitioning};
  }
};

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiePhaseoutPolicyStorageBrowserTest,
                       ForceThirdPartyCookieBlocking) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

  // Test that we can access storage. This feature's impact on cookies is tested
  // separately from this file.
  storage::test::SetStorageForFrame(GetNestedFrame(),
                                    /*include_cookies=*/false);
  storage::test::ExpectStorageForFrame(GetNestedFrame(),
                                       /*expected=*/true);
}

IN_PROC_BROWSER_TEST_F(ThirdPartyCookiePhaseoutPolicyStorageBrowserTest,
                       SandboxedTopLevelFrame) {
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL(
          kHostA,
          "/set-header?Content-Security-Policy: sandbox allow-scripts")));

  content::WebContents* web_contents =
      browser()->tab_strip_model()->GetActiveWebContents();
  constexpr char script[] = R"JS(
      fetch($1, {credentials: 'include', mode: 'cors'}).then(
          (result) => result.text());
  )JS";
  GURL url = https_server()->GetURL(kHostB, kEchoCookiesWithCorsPath);
  EXPECT_THAT(EvalJs(web_contents->GetPrimaryMainFrame(),
                     content::JsReplace(script, url))
                  .ExtractString(),
              net::CookieStringIs(testing::IsEmpty()));

  // Adding an explicit content setting should re-enable SameSite=None cookies
  // on sandboxed pages.
  auto* map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  map->SetContentSettingDefaultScope(
      https_server()->GetURL(kHostB, "/"), https_server()->GetURL(kHostA, "/"),
      ContentSettingsType::COOKIES, CONTENT_SETTING_ALLOW);

  ASSERT_TRUE(ui_test_utils::NavigateToURL(
      browser(),
      https_server()->GetURL(
          kHostA,
          "/set-header?Content-Security-Policy: sandbox allow-scripts")));

  EXPECT_THAT(EvalJs(web_contents->GetPrimaryMainFrame(),
                     content::JsReplace(script, url))
                  .ExtractString(),
              net::CookieStringIs(
                  testing::UnorderedElementsAre(testing::Key("thirdparty"))));
}

INSTANTIATE_TEST_SUITE_P(,
                         CookiePolicyStorageBrowserTest,
                         testing::Values(ContextType::kFrame,
                                         ContextType::kWorker));

INSTANTIATE_TEST_SUITE_P(,
                         ThirdPartyPartitionedStorageAccessibilityTest,
                         testing::Combine(testing::Values(ContextType::kFrame,
                                                          ContextType::kWorker),
                                          testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ,
    ThirdPartyPartitionedStorageAccessibilitySharedWorkerTest,
    testing::Combine(testing::Values(ContextType::kFrame), testing::Bool()));

INSTANTIATE_TEST_SUITE_P(
    ,
    ThirdPartyPartitionedStorageAccessibilityCanBeDisabledTest,
    testing::Combine(testing::Values(ContextType::kFrame, ContextType::kWorker),
                     testing::Values(true)));

}  // namespace
