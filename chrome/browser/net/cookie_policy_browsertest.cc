// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
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
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;

namespace {

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";

std::string OrNone(const std::string& arg) {
  return arg.empty() ? "None" : arg;
}

enum class TestType { kFrame, kWorker };

class CookiePolicyBrowserTest : public InProcessBrowserTest {
 public:
  CookiePolicyBrowserTest(const CookiePolicyBrowserTest&) = delete;
  CookiePolicyBrowserTest& operator=(const CookiePolicyBrowserTest&) = delete;

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

 protected:
  CookiePolicyBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // Storage Foundation has to be enabled, since it is accessed from the tests
    // that use chrome/browser/net/storage_test_utils.cc.
    // TODO(fivedots): Remove this switch once Storage Foundation
    // is enabled by default.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "StorageFoundationAPI");
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

  void TestThirdPartyIFrameStorage(TestType test_type) {
    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

    ExpectStorage(test_type, GetFrame(), false);
    SetStorage(test_type, GetFrame());
    ExpectStorage(test_type, GetFrame(), true);

    SetBlockThirdPartyCookies(true);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), false);

    // Allow all requests to b.test to access storage.
    GURL a_url = https_server_.GetURL(kHostA, "/");
    GURL b_url = https_server_.GetURL(kHostB, "/");
    cookie_settings()->SetCookieSetting(b_url,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), true);

    // Remove ALLOW setting.
    cookie_settings()->ResetCookieSetting(b_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings()->SetThirdPartyCookieSetting(
        a_url, ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), true);
  }

  void TestNestedThirdPartyIFrameStorage(TestType test_type) {
    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

    ExpectStorage(test_type, GetNestedFrame(), false);
    SetStorage(test_type, GetNestedFrame());
    ExpectStorage(test_type, GetNestedFrame(), true);

    SetBlockThirdPartyCookies(true);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all requests to b.test to access storage.
    GURL a_url = https_server_.GetURL(kHostA, "/");
    GURL c_url = https_server_.GetURL(kHostC, "/");
    cookie_settings()->SetCookieSetting(c_url,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);

    // Remove ALLOW setting.
    cookie_settings()->ResetCookieSetting(c_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings()->SetThirdPartyCookieSetting(
        a_url, ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);
  }

  void TestNestedFirstPartyIFrameStorage(TestType test_type) {
    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");

    ExpectStorage(test_type, GetNestedFrame(), false);
    SetStorage(test_type, GetNestedFrame());
    ExpectStorage(test_type, GetNestedFrame(), true);

    SetBlockThirdPartyCookies(true);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all requests to b.test to access storage.
    GURL a_url = https_server_.GetURL(kHostA, "/");
    cookie_settings()->SetCookieSetting(a_url,
                                        ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);

    // Remove ALLOW setting.
    cookie_settings()->ResetCookieSetting(a_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings()->SetThirdPartyCookieSetting(
        a_url, ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);
  }

  net::test_server::EmbeddedTestServer https_server_;

 private:
  void ExpectStorage(TestType test_type,
                     content::RenderFrameHost* frame,
                     bool expected) {
    switch (test_type) {
      case TestType::kFrame:
        storage::test::ExpectStorageForFrame(frame, /*include_cookies=*/true,
                                             expected);
        return;
      case TestType::kWorker:
        storage::test::ExpectStorageForWorker(frame, expected);
        return;
    }
  }

  void SetStorage(TestType test_type, content::RenderFrameHost* frame) {
    switch (test_type) {
      case TestType::kFrame:
        storage::test::SetStorageForFrame(frame, /*include_cookies=*/true);
        return;
      case TestType::kWorker:
        storage::test::SetStorageForWorker(frame);
        return;
    }
  }
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  SetBlockThirdPartyCookies(false);

  GURL url(https_server_.GetURL(kHostA, "/set-cookie?cookie1"));

  ASSERT_EQ("", content::GetCookies(browser()->profile(), url));

  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_EQ("cookie1", content::GetCookies(browser()->profile(), url));
}

// Visits a page that is a redirect across domain boundary to a page that sets
// a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       AllowFirstPartyCookiesRedirect) {
  SetBlockThirdPartyCookies(true);

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
  SetBlockThirdPartyCookies(false);

  NavigateToPageWithFrame(kHostA);

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
  SetBlockThirdPartyCookies(true);

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
  SetBlockThirdPartyCookies(false);

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
  SetBlockThirdPartyCookies(true);

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
  SetBlockThirdPartyCookies(true);

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
  SetBlockThirdPartyCookies(true);

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

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyIFrameStorageForFrame) {
  TestThirdPartyIFrameStorage(TestType::kFrame);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyIFrameStorageForWorker) {
  TestThirdPartyIFrameStorage(TestType::kWorker);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       NestedThirdPartyIFrameStorageForFrame) {
  TestNestedThirdPartyIFrameStorage(TestType::kFrame);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       NestedThirdPartyIFrameStorageForWorker) {
  TestNestedThirdPartyIFrameStorage(TestType::kWorker);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       NestedFirstPartyIFrameStorageForFrame) {
  TestNestedFirstPartyIFrameStorage(TestType::kFrame);
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       NestedFirstPartyIFrameStorageForWorker) {
  TestNestedFirstPartyIFrameStorage(TestType::kWorker);
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

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);

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
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);

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

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), false);

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
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), false);

  // Allow all third-parties on a.test to access cookies.
  cookie_settings()->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);
}

// The tuple<bool, bool> is used as <sameparty-considered-first-party flag is
// enabled, block-third-party-cookies setting is enabled>.
class SamePartyIsFirstPartyCookiePolicyBrowserTest
    : public CookiePolicyBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  SamePartyIsFirstPartyCookiePolicyBrowserTest() {
    if (sameparty_considered_first_party()) {
      feature_list_.InitAndEnableFeature(
          net::features::kSamePartyCookiesConsideredFirstParty);
    } else {
      feature_list_.Init();
    }
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    CookiePolicyBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StringPrintf("https://%s,https://%s,https://%s", kHostA, kHostB,
                           kHostC));
  }

  std::string AllCookies() const { return "thirdparty=1; firstparty=1"; }

  std::string ExpectedSamePartyCookies() const {
    // Assumes a cross-site context.
    if (block_third_party_cookies()) {
      if (sameparty_considered_first_party())
        return "firstparty=1";

      return "";
    }

    return "thirdparty=1; firstparty=1";
  }

  std::string ExpectedCrossPartyCookies() const {
    // Assumes a cross-site context.
    if (block_third_party_cookies())
      return "";

    return "thirdparty=1";
  }

  bool sameparty_considered_first_party() const {
    return std::get<0>(GetParam());
  }
  bool block_third_party_cookies() const { return std::get<1>(GetParam()); }

  std::vector<std::string> CookieLines() {
    return {
        "thirdparty=1;SameSite=None;Secure",
        "firstparty=1;SameParty;Secure",
    };
  }

 private:
  base::test::ScopedFeatureList feature_list_;
};

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest,
                       Write_HTTP) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Navigate iframe to a cross-site, same-party, cookie-setting endpoint, and
  // verify that only the appropriate cookies are set.
  // A(B)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, base::StrCat({
                              "/set-cookie?",
                              base::JoinString(CookieLines(), "&"),
                          }));
  EXPECT_EQ(ExpectedSamePartyCookies(),
            content::GetCookies(browser()->profile(), GetURL(kHostB)));
}

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest,
                       Read_HTTP) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Set cookies on `b.test`.
  for (const std::string& cookie_line : CookieLines()) {
    ASSERT_TRUE(content::SetCookie(
        browser()->profile(), https_server_.GetURL(kHostB, "/"), cookie_line));
  }

  // Navigate iframe to a cross-site, same-party cookie-reading endpoint, and
  // verify that only the appropriate cookies are sent.
  // A(B)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), OrNone(ExpectedSamePartyCookies()));

  // Navigate iframe to a cross-site, same-party frame with a frame, and
  // navigate _that_ frame to a cross-site page that echoes the cookie header,
  // and verify that only the appropriate cookies are sent.
  // A(C(B))
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), OrNone(ExpectedSamePartyCookies()));

  // Navigate iframe to a cross-site, cross-party frame with a frame, and
  // navigate _that_ frame to a distinct cross-site page that echoes the cookie
  // header, and verify that only the appropriate cookies are sent.
  // A(D(B))
  NavigateFrameTo(kHostD, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), OrNone(ExpectedCrossPartyCookies()));

  // Navigate to a page with a cross-party iframe, and verify cookie access.
  // D(B)
  NavigateToPageWithFrame(kHostD);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), OrNone(ExpectedCrossPartyCookies()));

  // Navigate to a cross-party page that embeds an iframe and a nested iframe
  // (that is same-party to the other iframe), and verify cookie access.
  // D(A(B))
  NavigateFrameTo(kHostA, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), OrNone(ExpectedCrossPartyCookies()));
}

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest, Write_JS) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Navigate iframe to a cross-site, same-party endpoint, set some cookies
  // via JS, and verify that only the appropriate cookies are actually set.
  // A(B)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");
  for (const std::string& cookie_line : CookieLines()) {
    SetCookieViaJS(GetFrame(), cookie_line);
  }
  EXPECT_EQ(ExpectedSamePartyCookies(),
            content::GetCookies(browser()->profile(), GetURL(kHostB)));
}

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest, Read_JS) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Set cookies on `b.test`.
  for (const std::string& cookie_line : CookieLines()) {
    ASSERT_TRUE(content::SetCookie(
        browser()->profile(), https_server_.GetURL(kHostB, "/"), cookie_line));
  }

  // Navigate iframe to a cross-site, same-party endpoint, read cookies via JS,
  // and verify that only the appropriate cookies are read.
  // A(B)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");
  EXPECT_EQ(ExpectedSamePartyCookies(), GetCookieViaJS(GetFrame()));

  // Navigate iframe to a cross-site, same-party frame with a frame,
  // navigate _that_ frame to a cross-site page, and read cookies via JS;
  // verify that only the appropriate cookies are read.
  // A(C(B))
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/empty.html");
  EXPECT_EQ(ExpectedSamePartyCookies(), GetCookieViaJS(GetNestedFrame()));

  // Navigate iframe to a cross-site, cross-party frame with a frame,
  // navigate _that_ frame to a distinct cross-site page, and read cookies via
  // JS; verify that only the appropriate cookies are read.
  // A(D(B))
  NavigateFrameTo(kHostD, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/empty.html");
  EXPECT_EQ(ExpectedCrossPartyCookies(), GetCookieViaJS(GetNestedFrame()));

  // Navigate to a page with a cross-party iframe, and verify cookie access.
  // D(B)
  NavigateToPageWithFrame(kHostD);
  NavigateFrameTo(kHostB, "/empty.html");
  EXPECT_EQ(ExpectedCrossPartyCookies(), GetCookieViaJS(GetFrame()));

  // Navigate to a cross-party page that embeds an iframe and a nested iframe
  // (that is same-party to the other iframe), and verify cookie access.
  // D(A(B))
  NavigateFrameTo(kHostA, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/empty.html");
  EXPECT_EQ(ExpectedCrossPartyCookies(), GetCookieViaJS(GetNestedFrame()));
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, ThirdPartyCookieHistogram) {
  base::HistogramTester tester;
  const char* histogram =
      "Navigation.MainFrame.ThirdPartyCookieBlockingEnabled";
  NavigateToPageWithFrame(kHostA);
  tester.ExpectTotalCount(histogram, 1);
  tester.ExpectBucketCount(histogram,
                           ThirdPartyCookieBlockState::kCookiesAllowed, 1);

  SetBlockThirdPartyCookies(true);
  NavigateToPageWithFrame(kHostA);
  tester.ExpectTotalCount(histogram, 2);
  tester.ExpectBucketCount(
      histogram, ThirdPartyCookieBlockState::kThirdPartyCookiesBlocked, 1);

  GURL url = https_server_.GetURL(kHostA, "/");
  cookie_settings()->SetThirdPartyCookieSetting(url, CONTENT_SETTING_ALLOW);
  NavigateToPageWithFrame(kHostA);
  tester.ExpectTotalCount(histogram, 3);
  tester.ExpectBucketCount(
      histogram,
      ThirdPartyCookieBlockState::kThirdPartyCookieBlockingDisabledForSite, 1);
}

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest,
                       Redirect_HTTP) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Set cookies on `a.test`.
  for (const std::string& cookie_line : CookieLines()) {
    ASSERT_TRUE(content::SetCookie(
        browser()->profile(), https_server_.GetURL(kHostA, "/"), cookie_line));
  }

  // Navigate iframe to a cross-site, cross-party page. Then redirect to a
  // same-site (same-party) cookie-reading endpoint, and verify that the
  // appropriate cookies are sent.
  // A(D -> A)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(
      kHostD, base::StrCat({
                  "/server-redirect?",
                  base::EscapeQueryParamValue(
                      https_server_.GetURL(kHostA, "/echoheader?cookie").spec(),
                      /*use_plus=*/false),
              }));
  EXPECT_EQ(GetFrameContent(), AllCookies());
}

IN_PROC_BROWSER_TEST_P(SamePartyIsFirstPartyCookiePolicyBrowserTest,
                       Redirect_JS) {
  SetBlockThirdPartyCookies(block_third_party_cookies());
  // Set cookies on `a.test`.
  for (const std::string& cookie_line : CookieLines()) {
    ASSERT_TRUE(content::SetCookie(
        browser()->profile(), https_server_.GetURL(kHostA, "/"), cookie_line));
  }

  // Navigate iframe to a cross-site, cross-party page. Then redirect to a
  // same-site (same-party) cookie-reading endpoint, and verify that the
  // appropriate cookies are sent.
  // A(D -> A)
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(
      kHostD, base::StrCat({
                  "/script_redirect.html?",
                  base::EscapeQueryParamValue(
                      https_server_.GetURL(kHostA, "/echoheader?cookie").spec(),
                      /*use_plus=*/false),
              }));
  EXPECT_EQ(GetFrameContent(), AllCookies());
}

INSTANTIATE_TEST_SUITE_P(FlagAndSettings,
                         SamePartyIsFirstPartyCookiePolicyBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

}  // namespace
