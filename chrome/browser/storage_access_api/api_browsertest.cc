// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/macros.h"
#include "base/path_service.h"
#include "base/strings/stringprintf.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/network_session_configurator/common/network_switches.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_paths.h"
#include "content/public/common/content_switches.h"
#include "content/public/test/browser_task_environment.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

enum class TestType { kFrame, kWorker };

class StorageAccessAPIBrowserTest : public InProcessBrowserTest {
 protected:
  StorageAccessAPIBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {
    feature_enable_.InitAndEnableFeature(blink::features::kStorageAccessAPI);
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    ASSERT_TRUE(https_server_.Start());
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // HTTPS server only serves a valid cert for localhost, so this is needed
    // to load pages from other hosts without an error.
    command_line->AppendSwitch(switches::kIgnoreCertificateErrors);
    // TODO(fivedots): Remove this switch once Storage Foundation is enabled
    // by default.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "StorageFoundationAPI");
  }

  GURL GetURL(const std::string& host) {
    GURL url(https_server_.GetURL(host, "/"));
    return url;
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
    ASSERT_TRUE(ExecuteScript(
        GetFrame(),
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  void ExpectNestedFrameContent(const std::string& expected) {
    storage::test::ExpectFrameContent(GetNestedFrame(), expected);
  }

  content::RenderFrameHost* GetMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetMainFrame();
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

  void TestThirdPartyIFrameStorageRequestsAccess(TestType test_type) {
    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/browsing_data/site_data.html");

    ExpectStorage(test_type, GetFrame(), false);
    SetStorage(test_type, GetFrame());
    ExpectStorage(test_type, GetFrame(), true);

    SetBlockThirdPartyCookies(true);

    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), false);
    storage::test::CheckStorageAccessForFrame(GetFrame(), false);

    // Allow all requests to b.com on a.com to access storage.
    storage::test::RequestStorageAccessForFrame(GetFrame(), true);
    storage::test::CheckStorageAccessForFrame(GetFrame(), true);

    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetFrame(), true);
    storage::test::CheckStorageAccessForFrame(GetFrame(), true);
  }

  void TestNestedThirdPartyIFrameStorage(TestType test_type) {
    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/iframe.html");
    NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");

    ExpectStorage(test_type, GetNestedFrame(), false);
    SetStorage(test_type, GetNestedFrame());
    ExpectStorage(test_type, GetNestedFrame(), true);

    SetBlockThirdPartyCookies(true);

    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/iframe.html");
    NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), false);
    storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

    // Allow all requests to b.com on a.com to access storage.
    storage::test::RequestStorageAccessForFrame(GetNestedFrame(), true);
    storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);

    NavigateToPageWithFrame("a.com");
    NavigateFrameTo("b.com", "/iframe.html");
    NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
    ExpectStorage(test_type, GetNestedFrame(), true);
    storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);
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

  base::test::ScopedFeatureList feature_enable_;
};

// Validate that if an iframe requests access that cookies become unblocked for
// just that top-level/third-party combination.
// TODO(crbug.com/1090625): Flaky-failing on Linux, Mac, and Windows.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       DISABLED_ThirdPartyCookiesIFrameRequestsAccess) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL("b.com"),
                                     "thirdparty=1");

  // Set a cookie on othersite.com.
  content::SetCookie(browser()->profile(),
                     https_server_.GetURL("othersite.com", "/"),
                     "thirdparty=other;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(
      browser()->profile(), GetURL("othersite.com"), "thirdparty=other");

  NavigateToPageWithFrame("a.com");

  // Allow all requests for b.com to have cookie access from a.com.
  // On the other hand, othersite.com does not have an exception set for it.
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Our use counter should not have fired yet, so we should have 0 occurrences.
  histogram_tester.ExpectBucketCount(
      kUseCounterHistogram,
      /*kStorageAccessAPI_HasStorageAccess_Method=*/3310, 0);
  histogram_tester.ExpectBucketCount(
      kUseCounterHistogram,
      /*kStorageAccessAPI_requestStorageAccess_Method=*/3311, 0);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("thirdparty=1");
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Since the frame has navigated we should see the use counter telem appear.
  histogram_tester.ExpectBucketCount(
      kUseCounterHistogram,
      /*kStorageAccessAPI_HasStorageAccess_Method=*/3310, 1);
  histogram_tester.ExpectBucketCount(
      kUseCounterHistogram,
      /*kStorageAccessAPI_requestStorageAccess_Method=*/3311, 1);

  // Navigate iframe to othersite.com and verify that the cookie is not sent.
  NavigateFrameTo("othersite.com", "/echoheader?cookie");
  ExpectFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);
  // Navigate nested iframe to othersite.com and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo("othersite.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("thirdparty=1");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);
  // Navigate nested iframe to othersite.com and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo("othersite.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

  // Navigate our top level to d.com and verify that all requests for b.com are
  // now blocked in that context.
  NavigateToPageWithFrame("d.com");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);
}

// Validate that the Storage Access API does not override any explicit user
// settings to block storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL("b.com"),
                                     "thirdparty=1");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/echoheader?cookie");

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Block all cookies with a user setting for b.com.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server_.GetURL("b.com", "/");
  cookie_settings->SetCookieSetting(url, ContentSetting::CONTENT_SETTING_BLOCK);
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  ExpectNestedFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);
}

// Validate that the Storage Access API will unblock other types of storage
// access when a grant is given and that it only applies to the top-level/third
// party pair requested on.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyIFrameStorageRequestsAccessForFrame) {
  TestThirdPartyIFrameStorageRequestsAccess(TestType::kFrame);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyIFrameStorageRequestsAccessForWorker) {
  TestThirdPartyIFrameStorageRequestsAccess(TestType::kWorker);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       NestedThirdPartyIFrameStorageForFrame) {
  TestNestedThirdPartyIFrameStorage(TestType::kFrame);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       NestedThirdPartyIFrameStorageForWorker) {
  TestNestedThirdPartyIFrameStorage(TestType::kWorker);
}

// Test third-party cookie blocking of features that allow to communicate
// between tabs such as SharedWorkers.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, MultiTabTest) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);

  // Allow all requests to b.com to access cookies.
  // Allow all requests to b.com on a.com to access storage.
  storage::test::RequestStorageAccessForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);
}

// Validates that once a grant is removed access is also removed.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsDeletedAccess) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL("b.com"),
                                     "thirdparty=1");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/echoheader?cookie");

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("thirdparty=1");
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);

  // Manually delete all our grants.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->ClearSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  NavigateFrameTo("b.com", "/echoheader?cookie");
  ExpectFrameContent("None");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, OpaqueOriginRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(
      GetMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       MissingSandboxTokenRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(GetMainFrame(),
                            "document.querySelector('iframe').sandbox='allow-"
                            "scripts allow-same-origin';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, SandboxTokenResolves) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(
      GetMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts "
      "allow-same-origin allow-storage-access-by-user-activation';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::RequestStorageAccessForFrame(GetFrame(), true);
  storage::test::CheckStorageAccessForFrame(GetFrame(), true);
}

// Validates that expiry data is transferred over IPC to the Network Service.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsExpireOverIPC) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com` and `c.com`.
  content::SetCookie(browser()->profile(), https_server_.GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL("b.com"),
                                     "thirdparty=1");
  content::SetCookie(browser()->profile(), https_server_.GetURL("c.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  storage::test::ExpectCookiesOnHost(browser()->profile(), GetURL("c.com"),
                                     "thirdparty=1");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/echoheader?cookie");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), false);

  // Manually create a pre-expired grant and ensure it doesn't grant access.
  base::Time expiration_time =
      base::Time::Now() - base::TimeDelta::FromMinutes(5);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      GetURL("b.com"), GetURL("a.com"), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});
  settings_map->SetContentSettingDefaultScope(
      GetURL("c.com"), GetURL("a.com"), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});

  // Manually send our expired setting. This needs to be done manually because
  // normally this expired value would be filtered out before sending and time
  // cannot be properly mocked in a browser test.
  ContentSettingsForOneType settings;
  settings.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(GetURL("b.com")),
      ContentSettingsPattern::FromURLNoWildcard(GetURL("a.com")),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false, expiration_time);
  settings.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(GetURL("c.com")),
      ContentSettingsPattern::FromURLNoWildcard(GetURL("a.com")),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false, base::Time());

  content::BrowserContext::GetDefaultStoragePartition(browser()->profile())
      ->GetCookieManagerForBrowserProcess()
      ->SetStorageAccessGrantSettings(settings, base::DoNothing());

  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);

  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/echoheader?cookie");
  storage::test::CheckStorageAccessForFrame(GetFrame(), false);
  storage::test::CheckStorageAccessForFrame(GetNestedFrame(), true);
  ExpectNestedFrameContent("thirdparty=1");
}

}  // namespace
