// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
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
#include "net/base/features.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;
using testing::Gt;

namespace {

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";

enum class TestType { kFrame, kWorker };

std::string BoolToString(bool b) {
  return b ? "true" : "false";
}

class StorageAccessAPIBaseBrowserTest : public InProcessBrowserTest {
 protected:
  StorageAccessAPIBaseBrowserTest(bool permission_grants_unpartitioned_storage,
                                  bool is_storage_partitioned)
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS),
        permission_grants_unpartitioned_storage_(
            permission_grants_unpartitioned_storage),
        is_storage_partitioned_(is_storage_partitioned) {
    std::vector<base::test::ScopedFeatureList::FeatureAndParams> enabled({
        {net::features::kStorageAccessAPI,
         {{"storage-access-api-grants-unpartitioned-storage",
           BoolToString(permission_grants_unpartitioned_storage)}}},
    });
    std::vector<base::Feature> disabled;

    if (is_storage_partitioned) {
      enabled.push_back({net::features::kThirdPartyStoragePartitioning, {}});
    } else {
      disabled.push_back(net::features::kThirdPartyStoragePartitioning);
    }

    features_.InitWithFeaturesAndParameters(enabled, disabled);
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
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

  std::string GetFrameContent() {
    return storage::test::GetFrameContent(GetFrame());
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

  std::string GetNestedFrameContent() {
    return storage::test::GetFrameContent(GetNestedFrame());
  }

  std::string ReadCookiesViaJS(content::RenderFrameHost* render_frame_host) {
    return content::EvalJs(render_frame_host, "document.cookie")
        .ExtractString();
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

  bool PermissionGrantsUnpartitionedStorage() const {
    return permission_grants_unpartitioned_storage_;
  }
  bool IsStoragePartitioned() const { return is_storage_partitioned_; }

 private:
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
  bool permission_grants_unpartitioned_storage_;
  bool is_storage_partitioned_;
};

class StorageAccessAPIBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  StorageAccessAPIBrowserTest()
      : StorageAccessAPIBaseBrowserTest(std::get<0>(GetParam()),
                                        std::get<1>(GetParam())) {}
};

// Validate that if an iframe requests access that cookies become unblocked for
// just that top-level/third-party combination.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameRequestsAccess) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server().GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("b.com")),
            "thirdparty=1");

  // Set a cookie on othersite.com.
  content::SetCookie(browser()->profile(),
                     https_server().GetURL("othersite.com", "/"),
                     "thirdparty=other;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("othersite.com")),
            "thirdparty=other");

  NavigateToPageWithFrame("a.com");

  // Allow all requests for b.com to have cookie access from a.com.
  // On the other hand, othersite.com does not have an exception set for it.
  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "thirdparty=1");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to othersite.com and verify that the cookie is not sent.
  NavigateFrameTo("othersite.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "thirdparty=1");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // Navigate nested iframe to othersite.com and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo("othersite.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=1");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "thirdparty=1");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // Navigate nested iframe to othersite.com and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo("othersite.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate our top level to d.com and verify that all requests for b.com are
  // now blocked in that context.
  NavigateToPageWithFrame("d.com");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(
      histogram_tester.GetBucketCount(
          kUseCounterHistogram,
          blink::mojom::WebFeature::kStorageAccessAPI_HasStorageAccess_Method),
      Gt(0));
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kUseCounterHistogram,
                  blink::mojom::WebFeature::
                      kStorageAccessAPI_requestStorageAccess_Method),
              Gt(0));
}

// Validate that the Storage Access API does not override any explicit user
// settings to block storage access.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server().GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("b.com")),
            "thirdparty=1");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Block all cookies with a user setting for b.com.
  auto cookie_settings =
      CookieSettingsFactory::GetForProfile(browser()->profile());
  GURL url = https_server().GetURL("b.com", "/");
  cookie_settings->SetCookieSetting(url, ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo("c.com", "/iframe.html");
  NavigateNestedFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

// Validates that once a grant is removed access is also removed.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsDeletedAccess) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com`.
  content::SetCookie(browser()->profile(), https_server().GetURL("b.com", "/"),
                     "thirdparty=1;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("b.com")),
            "thirdparty=1");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "thirdparty=1");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Manually delete all our grants.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->ClearSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  NavigateFrameTo("b.com", "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest, OpaqueOriginRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(
      GetPrimaryMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       MissingSandboxTokenRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(GetPrimaryMainFrame(),
                            "document.querySelector('iframe').sandbox='allow-"
                            "scripts allow-same-origin';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest, SandboxTokenResolves) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  ASSERT_TRUE(ExecuteScript(
      GetPrimaryMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts "
      "allow-same-origin allow-storage-access-by-user-activation';"));
  NavigateFrameTo("b.com", "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

// Validates that expiry data is transferred over IPC to the Network Service.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsExpireOverIPC) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `b.com` and `c.com`.
  content::SetCookie(browser()->profile(), https_server().GetURL("b.com", "/"),
                     "thirdparty=b;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("b.com")),
            "thirdparty=b");
  content::SetCookie(browser()->profile(), https_server().GetURL("c.com", "/"),
                     "thirdparty=c;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL("c.com")),
            "thirdparty=c");

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Manually create a pre-expired grant and ensure it doesn't grant access.
  base::Time expiration_time = base::Time::Now() - base::Minutes(5);
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

  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetStorageAccessGrantSettings(settings, base::DoNothing());

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=c");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "thirdparty=c");
}

INSTANTIATE_TEST_CASE_P(/* no prefix */,
                        StorageAccessAPIBrowserTest,
                        testing::Combine(testing::Bool(), testing::Bool()));

class StorageAccessAPIStorageBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<TestType, bool, bool>> {
 public:
  StorageAccessAPIStorageBrowserTest()
      : StorageAccessAPIBaseBrowserTest(std::get<1>(GetParam()),
                                        std::get<2>(GetParam())) {}

  void ExpectStorage(content::RenderFrameHost* frame, bool expected) {
    switch (GetTestType()) {
      case TestType::kFrame:
        storage::test::ExpectStorageForFrame(frame, /*include_cookies=*/false,
                                             expected);
        return;
      case TestType::kWorker:
        storage::test::ExpectStorageForWorker(frame, expected);
        return;
    }
  }

  void SetStorage(content::RenderFrameHost* frame) {
    switch (GetTestType()) {
      case TestType::kFrame:
        storage::test::SetStorageForFrame(frame, /*include_cookies=*/false);
        return;
      case TestType::kWorker:
        storage::test::SetStorageForWorker(frame);
        return;
    }
  }

  bool DoesPermissionGrantStorage() const {
    return IsStoragePartitioned() || PermissionGrantsUnpartitionedStorage();
  }

 private:
  TestType GetTestType() const { return std::get<0>(GetParam()); }
};

// Validate that the Storage Access API will unblock other types of storage
// access when a grant is given and that it only applies to the top-level/third
// party pair requested on.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest,
                       ThirdPartyIFrameStorageRequestsAccess) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), false);
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Allow all requests to b.com on a.com to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), DoesPermissionGrantStorage());
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest,
                       NestedThirdPartyIFrameStorage) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), false);
  SetStorage(GetNestedFrame());
  ExpectStorage(GetNestedFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Allow all requests to b.com on a.com to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetNestedFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/iframe.html");
  NavigateNestedFrameTo("c.com", "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), DoesPermissionGrantStorage());
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

// Test third-party cookie blocking of features that allow to communicate
// between tabs such as SharedWorkers.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest, MultiTabTest) {
  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Allow all requests to b.com to access cookies.
  // Allow all requests to b.com on a.com to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame("a.com");
  NavigateFrameTo("b.com", "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(),
                                            DoesPermissionGrantStorage());
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         StorageAccessAPIStorageBrowserTest,
                         testing::Combine(testing::Values(TestType::kFrame,
                                                          TestType::kWorker),
                                          testing::Bool(),
                                          testing::Bool()));

}  // namespace
