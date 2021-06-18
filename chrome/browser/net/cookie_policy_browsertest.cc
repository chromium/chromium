// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;

namespace {

const char* kHostA = "a.test";
const char* kHostB = "b.test";
const char* kHostC = "c.test";
const char* kHostD = "d.test";

enum class TestType { kFrame, kWorker };

class CookiePolicyBrowserTest : public InProcessBrowserTest {
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

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

  void NavigateToPageWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURL(browser(), main_url);
  }

  void NavigateToNewTabWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    GURL page = https_server_.GetURL(host, path);
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "test", page));
  }

  void ExpectFrameContent(const std::string& expected) {
    storage::test::ExpectFrameContent(GetFrame(), expected);
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

  void ExpectNestedFrameContent(const std::string& expected) {
    storage::test::ExpectFrameContent(GetNestedFrame(), expected);
  }

  content::RenderFrameHost* GetFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return ChildFrameAt(web_contents->GetMainFrame(), 0);
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
    auto cookie_settings =
        CookieSettingsFactory::GetForProfile(browser()->profile());
    GURL a_url = https_server_.GetURL(kHostA, "/");
    GURL b_url = https_server_.GetURL(kHostB, "/");
    cookie_settings->SetCookieSetting(b_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), true);

    // Remove ALLOW setting.
    cookie_settings->ResetCookieSetting(b_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings->SetThirdPartyCookieSetting(
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
    auto cookie_settings =
        CookieSettingsFactory::GetForProfile(browser()->profile());
    GURL a_url = https_server_.GetURL(kHostA, "/");
    GURL c_url = https_server_.GetURL(kHostC, "/");
    cookie_settings->SetCookieSetting(c_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);

    // Remove ALLOW setting.
    cookie_settings->ResetCookieSetting(c_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings->SetThirdPartyCookieSetting(
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
    auto cookie_settings =
        CookieSettingsFactory::GetForProfile(browser()->profile());
    GURL a_url = https_server_.GetURL(kHostA, "/");
    cookie_settings->SetCookieSetting(a_url,
                                      ContentSetting::CONTENT_SETTING_ALLOW);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);

    // Remove ALLOW setting.
    cookie_settings->ResetCookieSetting(a_url);

    NavigateToPageWithFrame(kHostA);
    NavigateFrameTo(kHostB, "/iframe.html");
    NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);

    // Allow all third-parties on a.test to access storage.
    cookie_settings->SetThirdPartyCookieSetting(
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
        storage::test::ExpectStorageForFrame(frame, expected);
        return;
      case TestType::kWorker:
        storage::test::ExpectStorageForWorker(frame, expected);
        return;
    }
  }

  void SetStorage(TestType test_type, content::RenderFrameHost* frame) {
    switch (test_type) {
      case TestType::kFrame:
        storage::test::SetStorageForFrame(frame);
        return;
      case TestType::kWorker:
        storage::test::SetStorageForWorker(frame);
        return;
    }
  }

  DISALLOW_COPY_AND_ASSIGN(CookiePolicyBrowserTest);
};

// Visits a page that sets a first-party cookie.
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest, AllowFirstPartyCookies) {
  SetBlockThirdPartyCookies(false);

  GURL url(https_server_.GetURL(kHostA, "/set-cookie?cookie1"));

  ASSERT_EQ("", content::GetCookies(browser()->profile(), url));

  ui_test_utils::NavigateToURL(browser(), url);

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
  ui_test_utils::NavigateToURL(
      browser(), GURL(https_server_.GetURL(kHostA, "/server-redirect?").spec() +
                      redirected_url.spec()));

  EXPECT_EQ("cookie2",
            content::GetCookies(browser()->profile(), redirected_url));
}

// Third-Party Frame Tests
IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowSetting) {
  SetBlockThirdPartyCookies(false);

  NavigateToPageWithFrame(kHostA);

  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB), "");

  // Navigate iframe to a cross-site, cookie-setting endpoint, and verify that
  // the cookie is set:
  NavigateFrameTo(kHostB, "/set-cookie?thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo(kHostB, "/iframe.html");
  // Still need SameSite=None and Secure because the top-level is a.test so this
  // is still cross-site.
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=2;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=2");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is set:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=3;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
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
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB), "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=2;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB), "");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site, cookie-setting endpoint, and verify that the cookie
  // is not set:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB,
                        "/set-cookie?thirdparty=3;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB), "");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameAllowReading) {
  SetBlockThirdPartyCookies(false);

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=1");

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  ExpectFrameContent("thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");
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
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=1");

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  ExpectFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("None");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=1");

  // Set a cookie on d.test.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostD, "/"),
                     "thirdparty=other;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostD),
                                     "thirdparty=other");

  // Allow all requests to b.test to have cookies.
  // On the other hand, d.test does not have an exception set for it.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server_.GetURL(kHostB, "/");
  cookie_settings->SetCookieSetting(url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  ExpectFrameContent("thirdparty=1");
  // Navigate iframe to d.test and verify that the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  ExpectFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");
  // Navigate nested iframe to d.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostD, "/echoheader?cookie");
  ExpectNestedFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");
  // Navigate nested iframe to d.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostD, "/echoheader?cookie");
  ExpectNestedFrameContent("None");
}

IN_PROC_BROWSER_TEST_F(CookiePolicyBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.test`.
  content::SetCookie(browser()->profile(), https_server_.GetURL(kHostB, "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL(kHostB),
                                     "thirdparty=1");

  // Allow all requests on the top frame domain a.test to have cookies.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server_.GetURL(kHostA, "/");
  cookie_settings->SetThirdPartyCookieSetting(
      url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  ExpectFrameContent("thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");

  // Now repeat the above with a different top frame site, which does not have
  // an exception set for it.
  NavigateToPageWithFrame(kHostD);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  ExpectFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is not sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("None");

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is not sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  ExpectNestedFrameContent("None");
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
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL a_url = https_server_.GetURL(kHostA, "/");
  GURL b_url = https_server_.GetURL(kHostB, "/");
  cookie_settings->SetCookieSetting(b_url,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);

  // Remove ALLOW setting.
  cookie_settings->ResetCookieSetting(b_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);

  // Allow all third-parties on a.test to access cookies.
  cookie_settings->SetThirdPartyCookieSetting(
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
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL a_url = https_server_.GetURL(kHostA, "/");
  cookie_settings->SetCookieSetting(a_url,
                                    ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);

  // Remove ALLOW setting.
  cookie_settings->ResetCookieSetting(a_url);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), false);

  // Allow all third-parties on a.test to access cookies.
  cookie_settings->SetThirdPartyCookieSetting(
      a_url, ContentSetting::CONTENT_SETTING_ALLOW);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetNestedFrame(), true);
}

}  // namespace
