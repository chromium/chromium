// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/path_service.h"
#include "base/strings/strcat.h"
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
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/metrics/content/subprocess_metrics_provider.h"
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
#include "net/cookies/cookie_constants.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;
using testing::Gt;

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostASubdomain[] = "subdomain.a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostD[] = "d.test";

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";
constexpr char kRequestOutcomeHistogram[] = "API.StorageAccess.RequestOutcome";

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
        is_storage_partitioned_(is_storage_partitioned) {}

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                            GetDisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    std::vector<base::test::FeatureRefAndParams> enabled({
        {net::features::kStorageAccessAPI,
         {
             {
                 "storage-access-api-grants-unpartitioned-storage",
                 BoolToString(permission_grants_unpartitioned_storage_),
             },
             {
                 "storage_access_api_auto_grant_within_fps",
                 "false",
             },
             {
                 "storage_access_api_auto_deny_outside_fps",
                 "false",
             },
         }},
    });
    if (is_storage_partitioned_) {
      enabled.push_back({net::features::kThirdPartyStoragePartitioning, {}});
    }
    return enabled;
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    std::vector<base::test::FeatureRef> disabled;
    if (!is_storage_partitioned_) {
      disabled.push_back(net::features::kThirdPartyStoragePartitioning);
    }
    return disabled;
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

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // TODO(fivedots): Remove this switch once Storage Foundation is enabled
    // by default.
    command_line->AppendSwitchASCII(switches::kEnableBlinkFeatures,
                                    "StorageFoundationAPI");
  }

  void SetCrossSiteCookieOnHost(const std::string& host) {
    GURL host_url = GetURL(host);
    std::string cookie = base::StrCat({"cross-site=", host});
    content::SetCookie(browser()->profile(), host_url,
                       base::StrCat({cookie, ";SameSite=None;Secure"}));
    ASSERT_THAT(content::GetCookies(browser()->profile(), host_url),
                testing::HasSubstr(cookie));
  }

  void SetPartitionedCookieInContext(const std::string& top_level_host,
                                     const std::string& embedded_host) {
    GURL host_url = GetURL(embedded_host);
    std::string cookie =
        base::StrCat({"cross-site=", embedded_host, "(partitioned)"});
    net::CookiePartitionKey partition_key =
        net::CookiePartitionKey::FromURLForTesting(GetURL(top_level_host));
    content::SetPartitionedCookie(
        browser()->profile(), host_url,
        base::StrCat({cookie, ";SameSite=None;Secure;Partitioned"}),
        partition_key);
    ASSERT_THAT(content::GetCookies(
                    browser()->profile(), host_url,
                    net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                    net::CookiePartitionKeyCollection(partition_key)),
                testing::HasSubstr(cookie));
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

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);
  SetCrossSiteCookieOnHost(kHostC);
  SetCrossSiteCookieOnHost(kHostD);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to c.test and verify that the cookie is not sent.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // Navigate nested iframe to c.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is sent:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // Navigate nested iframe to c.test and verify that the cookie is not
  // sent.
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate our top level to kHostD and verify that all requests for kHostB
  // are now blocked in that context.
  NavigateToPageWithFrame(kHostD);

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
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

  // Set a cookie on `kHostB`.
  content::SetCookie(browser()->profile(), GetURL(kHostB),
                     "thirdparty=1;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Block all cookies with a user setting for kHostB.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), ContentSetting::CONTENT_SETTING_BLOCK);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is blocked:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a cross-site page that echos the cookie header, and verify that
  // the cookie is blocked:
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Navigate iframe to a cross-site frame with a frame, and navigate _that_
  // frame to a distinct cross-site page that echos the cookie header, and
  // verify that the cookie is blocked:
  NavigateFrameTo(kHostC, "/iframe.html");
  NavigateNestedFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

// Validates that once a grant is removed access is also removed.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsDeletedAccess) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `kHostB`.
  content::SetCookie(browser()->profile(), GetURL(kHostB),
                     "thirdparty=1;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=1");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "thirdparty=1");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "thirdparty=1");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Manually delete all our grants.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->ClearSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest, OpaqueOriginRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(ExecuteScript(
      GetPrimaryMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts';"));
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       MissingSandboxTokenRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(ExecuteScript(GetPrimaryMainFrame(),
                            "document.querySelector('iframe').sandbox='allow-"
                            "scripts allow-same-origin';"));
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest, SandboxTokenResolves) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(ExecuteScript(
      GetPrimaryMainFrame(),
      "document.querySelector('iframe').sandbox='allow-scripts "
      "allow-same-origin allow-storage-access-by-user-activation';"));
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

// Validates that expiry data is transferred over IPC to the Network Service.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsExpireOverIPC) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `kHostB` and `kHostC`.
  content::SetCookie(browser()->profile(), GetURL(kHostB),
                     "thirdparty=b;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "thirdparty=b");
  content::SetCookie(browser()->profile(), GetURL(kHostC),
                     "thirdparty=c;SameSite=None;Secure");
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostC)),
            "thirdparty=c");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Manually create a pre-expired grant and ensure it doesn't grant access.
  base::Time expiration_time = base::Time::Now() - base::Minutes(5);
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostB), GetURL(kHostA), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostC), GetURL(kHostA), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW,
      {expiration_time, content_settings::SessionModel::UserSession});

  // Manually send our expired setting. This needs to be done manually because
  // normally this expired value would be filtered out before sending and time
  // cannot be properly mocked in a browser test.
  ContentSettingsForOneType settings;
  settings.push_back(ContentSettingPatternSource(
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostB)),
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false, {.expiration = expiration_time}));
  settings.emplace_back(
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostC)),
      ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
      base::Value(CONTENT_SETTING_ALLOW), "preference",
      /*incognito=*/false);

  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->SetStorageAccessGrantSettings(settings, base::DoNothing());

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(GetNestedFrameContent(), "thirdparty=c");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "thirdparty=c");
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       RsaForOriginDisabledByDefault) {
  NavigateToPageWithFrame(kHostA);
  // Ensure that the proposed extension is not available unless explicitly
  // enabled.
  EXPECT_TRUE(EvalJs(GetPrimaryMainFrame(),
                     "\"requestStorageAccessForOrigin\" in document === false")
                  .ExtractBool());
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       RequestStorageAccessTopLevelScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostASubdomain);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // Similar to the rsaFor equivalent, scoping may or may not allow access for
  // the subdomain, depending on the setting.
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       RequestStorageAccessTopLevelScopingSubDomainFirst) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostASubdomain);

  // Allow all requests for kHostB to have cookie access from subdomain.a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // Similar to the rsaFor equivalent, scoping may or may not allow access for
  // the subdomain, depending on the setting.
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIBrowserTest,
                       RequestStorageAccessEmbeddedOriginScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  // Verify that the top-level scoping does not leak to the embedded URL, whose
  // origin must be used.
  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(kHostA, "/echoheader?cookie");

  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Regardless of the top-level site or origin scoping, the embedded origin
  // should be used.
  NavigateFrameTo(kHostASubdomain, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
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
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), false);
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Allow all requests to kHostB on kHostA to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  ExpectStorage(GetFrame(), DoesPermissionGrantStorage());
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest,
                       NestedThirdPartyIFrameStorage) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), false);
  SetStorage(GetNestedFrame());
  ExpectStorage(GetNestedFrame(), true);

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  // Allow all requests to kHostB on kHostA to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetNestedFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  ExpectStorage(GetNestedFrame(), DoesPermissionGrantStorage());
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

// Test third-party cookie blocking of features that allow to communicate
// between tabs such as SharedWorkers.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest, MultiTabTest) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Allow all requests to kHostB to access cookies.
  // Allow all requests to kHostB on kHostA to access storage.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
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

class StorageAccessAPIForOriginBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  StorageAccessAPIForOriginBrowserTest()
      : StorageAccessAPIBaseBrowserTest(std::get<0>(GetParam()),
                                        std::get<1>(GetParam())) {}

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        StorageAccessAPIBaseBrowserTest::GetEnabledFeatures();
    enabled.push_back(
        {blink::features::kStorageAccessAPIForOriginExtension, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginBrowserTest,
                       SameOriginGrantedByDefault) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  NavigateToPageWithFrame(kHostA);

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), "https://asdf.example"));
  EXPECT_FALSE(
      storage::test::RequestStorageAccessForOrigin(GetFrame(), "mattwashere"));
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), GetURL(kHostA).spec()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginBrowserTest,
                       TopLevelOpaqueOriginRejected) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:,Hello%2C%20World%21")));

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
}

// Validate that if an iframe requests access that cookies become unblocked for
// just that top-level/third-party combination.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginBrowserTest,
                       // TODO(crbug.com/1370096): Re-enable metric assertions.
                       GrantGivesCrossSiteCookieAccess) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // Repeated calls should also return true.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Also validate that an additional site C was not granted access.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginBrowserTest,
                       RequestStorageAccessForOriginTopLevelScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostASubdomain);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // Storage access grants are scoped to the embedded origin on the top-level
  // site. Accordingly, the access should be granted.
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(
    StorageAccessAPIForOriginBrowserTest,
    RequestStorageAccessForOriginTopLevelScopingWhenRequestedFromSubdomain) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostASubdomain);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // When top-level site scoping is enabled, the subdomain's grant counts for
  // the less-specific domain; otherwise, it does not.
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginBrowserTest,
                       RequestStorageAccessForOriginEmbeddedOriginScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  // Verify that the top-level scoping does not leak to the embedded URL, whose
  // origin must be used.
  NavigateToPageWithFrame(kHostB);

  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Regardless of the top-level site or origin scoping, the embedded origin
  // should be used.
  NavigateFrameTo(kHostASubdomain, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

INSTANTIATE_TEST_SUITE_P(/* no prefix */,
                         StorageAccessAPIForOriginBrowserTest,
                         testing::Combine(testing::Bool(), testing::Bool()));

// Tests to validate First-Party Set use with `requestStorageAccessForOrigin`.
class StorageAccessAPIForOriginWithFirstPartySetsBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
  StorageAccessAPIForOriginWithFirstPartySetsBrowserTest()
      : StorageAccessAPIBaseBrowserTest(false, false) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StorageAccessAPIBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{blink::features::kStorageAccessAPIForOriginExtension, {}},
            {net::features::kStorageAccessAPI,
             {
                 {
                     net::features::kStorageAccessAPIAutoGrantInFPS.name,
                     "true",
                 },
                 {
                     net::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                     "true",
                 },
                 // Setting implicit grants to a non-zero number here
                 // demonstrates that when the auto-deny param is enabled, the
                 // implicit grants param doesn't matter, since the auto-deny
                 // param takes precedence.
                 {
                     "storage-access-api-implicit-grant-limit",
                     "5",
                 },
             }}};
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIForOriginWithFirstPartySetsBrowserTest,
                       Permission_AutograntedWithinFirstPartySet) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);
  SetCrossSiteCookieOnHost(kHostC);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The request comes from `kHostA`, which is in a First-Party Set with
  // `khostB`. Note that `kHostB` would not be auto-granted access if it were
  // the requestor, because it is a service domain.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Also validate that an additional site C was not granted access.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  0 /*RequestOutcome::kGrantedByFirstPartySet*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIForOriginWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedForServiceDomain) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostB);

  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The promise should be rejected; `khostB` is a service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Re-navigate iframe to a cross-site, cookie-reading endpoint, and verify
  // that the cookie is not sent.
  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  5 /*RequestOutcome::kDeniedByPrerequisites*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIForOriginWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedForServiceDomainInIframe) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // `kHostB` cannot be granted access via `RequestStorageAccessForOrigin`,
  // because the call is not from the top-level page and because `kHostB` is a
  // service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), GetURL(kHostA).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // However, a regular `requestStorageAccess` call should be granted;
  // requesting on behalf of another domain is what is not acceptable.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the frame subsequently navigates to an endpoint on kHostB,
  // kHostB's cookies are sent, and the iframe retains storage
  // access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIForOriginWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedOutsideFirstPartySet) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostD);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // `kHostD` cannot be granted access via `RequestStorageAccessForOrigin` in
  // this configuration, because the requesting site (`kHostA`) is not in the
  // same First-Party Set as the requested site (`kHostD`).
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostD).spec()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  3 /*RequestOutcome::kDeniedByFirstPartySet*/),
              Gt(0));
}

// Tests to validate that, when the `requestStorageAccessForOrigin` extension is
// explicitly disabled, or if the larger Storage Access API is disabled, it does
// not leak onto the document object.
class StorageAccessAPIForOriginExplicitlyDisabledBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<bool> {
 public:
  StorageAccessAPIForOriginExplicitlyDisabledBrowserTest()
      : StorageAccessAPIBaseBrowserTest(true, true),
        enable_standard_storage_access_api_(GetParam()) {}

 protected:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    // The test should validate that either flag alone disables the API.
    // Note that enabling the extension and not the standard API means both are
    // disabled.
    if (enable_standard_storage_access_api_) {
      return {blink::features::kStorageAccessAPIForOriginExtension};
    }
    return {net::features::kStorageAccessAPI};
  }
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    // When the standard API is enabled, return the parent class's enabled
    // feature list. Otherwise, enable only the extension; this should not take
    // effect.
    if (enable_standard_storage_access_api_) {
      return StorageAccessAPIBaseBrowserTest::GetEnabledFeatures();
    }
    return {{blink::features::kStorageAccessAPIForOriginExtension, {}}};
  }

 private:
  bool enable_standard_storage_access_api_;
};

IN_PROC_BROWSER_TEST_P(StorageAccessAPIForOriginExplicitlyDisabledBrowserTest,
                       RsaForOriginNotPresentOnDocumentWhenExplicitlyDisabled) {
  NavigateToPageWithFrame(kHostA);
  // Ensure that the proposed extension is not available unless explicitly
  // enabled.
  EXPECT_TRUE(EvalJs(GetPrimaryMainFrame(),
                     "\"requestStorageAccessForOrigin\" in document === false")
                  .ExtractBool());
}

INSTANTIATE_TEST_SUITE_P(
    /* no prefix */,
    StorageAccessAPIForOriginExplicitlyDisabledBrowserTest,
    testing::Bool());

class StorageAccessAPIWithFirstPartySetsBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  StorageAccessAPIWithFirstPartySetsBrowserTest()
      : StorageAccessAPIBaseBrowserTest(false, false) {}

  void SetUpCommandLine(base::CommandLine* command_line) override {
    StorageAccessAPIBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseFirstPartySet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostB, R"("])",
                      R"(, "serviceSites": ["https://)", kHostD, R"("]})"}));
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {net::features::kStorageAccessAPI,
         {
             {
                 net::features::kStorageAccessAPIAutoGrantInFPS.name,
                 "true",
             },
             {
                 net::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                 "true",
             },
             // Setting implicit grants to a non-zero number here demonstrates
             // that when the auto-deny param is enabled, the implicit grants
             // param doesn't matter, since the auto-deny param takes
             // precedence.
             {
                 "storage-access-api-implicit-grant-limit",
                 "5",
             },
         }},
    };
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_AutograntedWithinFirstPartySet) {
  base::HistogramTester histogram_tester;
  // Note: kHostA and kHostB are considered same-party due to the use of
  // `network::switches::kUseFirstPartySet`.
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // kHostB starts without access:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostB can request storage access, and it is granted:
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the frame subsequently navigates to an endpoint on kHostB,
  // kHostB's cookies are sent, and the iframe retains storage
  // access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  0 /*RequestOutcome::kGrantedByFirstPartySet*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedForServiceDomain) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  SetCrossSiteCookieOnHost(kHostA);

  NavigateToPageWithFrame(kHostD);

  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // The promise should be rejected; `khostD` is a service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  5 /*RequestOutcome::kDeniedByPrerequisites*/),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedOutsideFirstPartySet) {
  base::HistogramTester histogram_tester;
  // Note: kHostA and kHostC are considered cross-party, since kHostA's set does
  // not include kHostC.
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostC);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to kHostC and verify that the cookie is not sent.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostC cannot request storage access.
  EXPECT_FALSE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  3 /*RequestOutcome::kDeniedByFirstPartySet*/),
              Gt(0));
}

class StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest()
      : StorageAccessAPIBaseBrowserTest(false, false) {}

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {net::features::kStorageAccessAPI,
         {
             {
                 net::features::kStorageAccessAPIAutoGrantInFPS.name,
                 "true",
             },
             {
                 net::features::kStorageAccessAPIAutoDenyOutsideFPS.name,
                 "false",
             },
             {
                 "storage-access-api-implicit-grant-limit",
                 "5",
             },
         }},
    };
  }
};

IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest,
    ImplicitGrants) {
  // When auto-deny is disabled (but auto-grant is enabled), implicit grants
  // still work.

  // Note: kHostA and kHostC are considered cross-party, since kHostA's set does
  // not include kHostC.
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostC);

  NavigateToPageWithFrame(kHostA);

  // Navigate iframe to kHostC and verify that the cookie is not sent.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostC can request storage access, due to implicit grants.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostB);

  // Navigate iframe to kHostC and verify that the cookie is not sent.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostC can request storage access here too, again due to
  // implicit grants.
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

class StorageAccessAPIWithCHIPSBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  StorageAccessAPIWithCHIPSBrowserTest()
      : StorageAccessAPIBaseBrowserTest(
            /*permission_grants_unpartitioned_storage=*/false,
            /*is_storage_partitioned=*/false) {}

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> enabled =
        StorageAccessAPIBaseBrowserTest::GetEnabledFeatures();
    enabled.push_back({net::features::kPartitionedCookies, {}});
    enabled.push_back(
        {blink::features::kStorageAccessAPIForOriginExtension, {}});
    return enabled;
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithCHIPSBrowserTest,
                       RequestStorageAccess_CoexistsWithCHIPS) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);
  SetPartitionedCookieInContext(/*top_level_host=*/kHostA,
                                /*embedded_host=*/kHostB);

  NavigateToPageWithFrame(kHostA);

  // kHostB starts without unpartitioned cookies:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test(partitioned)");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostB can request storage access, and it is granted (by an implicit
  // grant):
  EXPECT_TRUE(storage::test::RequestStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the frame subsequently navigates to an endpoint on kHostB, kHostB's
  // unpartitioned and partitioned cookies are sent, and the iframe retains
  // storage access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(),
            "cross-site=b.test; cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()),
            "cross-site=b.test; cross-site=b.test(partitioned)");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithCHIPSBrowserTest,
                       RequestStorageAccessForOrigin_CoexistsWithCHIPS) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);
  SetPartitionedCookieInContext(/*top_level_host=*/kHostA,
                                /*embedded_host=*/kHostB);

  NavigateToPageWithFrame(kHostA);

  // kHostB starts without unpartitioned cookies:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test(partitioned)");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostA can request storage access on behalf of kHostB, and it is granted
  // (by an implicit grant):
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the frame subsequently navigates to an endpoint on kHostB, kHostB's
  // unpartitioned and partitioned cookies are sent, and the iframe retains
  // storage access.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(),
            "cross-site=b.test; cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()),
            "cross-site=b.test; cross-site=b.test(partitioned)");
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

}  // namespace
