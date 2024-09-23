// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/barrier_closure.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/run_loop.h"
#include "base/strings/strcat.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/top_level_storage_access_api/top_level_storage_access_permission_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/features.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "services/network/public/cpp/network_switches.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "ui/base/window_open_disposition.h"

using content::BrowserThread;
using testing::Gt;

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kHostASubdomain[] = "subdomain.a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostD[] = "d.test";

constexpr char kRequestOutcomeHistogram[] =
    "API.TopLevelStorageAccess.RequestOutcome";

// Path for URL of custom response
constexpr char kFetchWithCredentialsPath[] = "/respondwithcookies";

constexpr char kQueryTopLevelStorageAccessPermission[] =
    "navigator.permissions.query({name: 'top-level-storage-access', "
    "requestedOrigin: $1}).then("
    "  (permission) => permission.state);";
constexpr char kVerifyHasStorageAccessPermission[] =
    "navigator.permissions.query({name: 'storage-access'}).then("
    "  (permission) => permission.name === 'storage-access' && "
    "permission.state === 'granted');";
constexpr char kRequestStorageAccess[] = "document.requestStorageAccess()";

std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kFetchWithCredentialsPath) {
    return nullptr;
  }
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  // Set the cors enabled headers.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kOrigin);
      it != request.headers.end()) {
    http_response->AddCustomHeader("Access-Control-Allow-Origin", it->second);
    http_response->AddCustomHeader("Vary", "origin");
    http_response->AddCustomHeader("Access-Control-Allow-Credentials", "true");
  }
  // Get the 'Cookie' header that was sent in the request.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
      it != request.headers.end()) {
    http_response->set_content(it->second);
  }

  return http_response;
}

class RequestStorageAccessForBaseBrowserTest : public InProcessBrowserTest {
 protected:
  RequestStorageAccessForBaseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(
        GetEnabledFeatures(),
        {content_settings::features::kActiveContentSettingExpiry});
    InProcessBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(base::BindRepeating(&HandleRequest));
    ASSERT_TRUE(https_server_.Start());
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
    content::SetCookie(
        browser()->profile(), host_url,
        base::StrCat({cookie, ";SameSite=None;Secure;Partitioned"}),
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
        &partition_key);
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
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server_.GetURL(host, "/iframe.html")));
  }

  void NavigateToNewTabWithFrame(const std::string& host) {
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), https_server_.GetURL(host, "/iframe.html"),
        WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    EXPECT_TRUE(NavigateIframeToURL(active_web_contents(), "test",
                                    https_server_.GetURL(host, path)));
  }

  std::string GetFrameContent() {
    return storage::test::GetFrameContent(GetFrame());
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    content::TestNavigationObserver load_observer(active_web_contents());
    ASSERT_TRUE(ExecJs(
        GetFrame(),
        content::JsReplace("document.body.querySelector('iframe').src = $1;",
                           https_server_.GetURL(host, path))));
    load_observer.Wait();
  }

  std::string GetNestedFrameContent() {
    return storage::test::GetFrameContent(GetNestedFrame());
  }

  content::EvalJsResult ReadCookiesViaJS(
      content::RenderFrameHost* render_frame_host) {
    return content::EvalJs(render_frame_host, "document.cookie",
                           content::EXECUTE_SCRIPT_NO_USER_GESTURE);
  }

  content::EvalJsResult QueryPermission(
      content::RenderFrameHost* render_frame_host,
      const std::string& requested_origin) {
    return content::EvalJs(
        render_frame_host,
        content::JsReplace(kQueryTopLevelStorageAccessPermission,
                           GetURL(requested_origin)));
  }

  content::RenderFrameHost* GetPrimaryMainFrame() {
    return active_web_contents()->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 0);
  }

  content::RenderFrameHost* GetNestedFrame() {
    return ChildFrameAt(GetFrame(), 0);
  }

  std::string CookiesFromFetchWithCredentials(content::RenderFrameHost* frame,
                                              const std::string& host,
                                              const bool cors_enabled) {
    return storage::test::FetchWithCredentials(
        frame, https_server_.GetURL(host, kFetchWithCredentialsPath),
        cors_enabled);
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

 private:
  content::WebContents* active_web_contents() {
    return browser()->tab_strip_model()->GetActiveWebContents();
  }

  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
};

class RequestStorageAccessForBrowserTest
    : public RequestStorageAccessForBaseBrowserTest {};

// Validates that expiry data is transferred over IPC to the Network Service.
IN_PROC_BROWSER_TEST_F(RequestStorageAccessForBrowserTest,
                       ThirdPartyGrantsExpireOverIPC) {
  SetBlockThirdPartyCookies(true);

  // Set a cookie on `kHostB` and `kHostC`.
  SetCrossSiteCookieOnHost(kHostB);
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostB)),
            "cross-site=b.test");
  SetCrossSiteCookieOnHost(kHostC);
  ASSERT_EQ(content::GetCookies(browser()->profile(), GetURL(kHostC)),
            "cross-site=c.test");

  const base::TimeDelta lifetime = base::Hours(24);
  const base::Time creation_time =
      base::Time::Now() - base::Minutes(5) - lifetime;
  content_settings::ContentSettingConstraints constraints(creation_time);
  constraints.set_lifetime(lifetime);
  constraints.set_session_model(
      content_settings::mojom::SessionModel::USER_SESSION);

  // Manually create a pre-expired grant and ensure it doesn't grant access.
  // This needs to be done manually because normally this expired value would be
  // filtered out before sending and time cannot be properly mocked in a browser
  // test.
  //
  // We also do not set the setting in the browser process's
  // HostContentSettingsMap, since doing so provokes a
  // CookieManager::SetContentSettings IPC from ProfileNetworkContextService,
  // and we don't have a good way of synchronizing with that IPC such that we
  // can override the settings used by the Network Service to intentionally
  // include expired settings. (We'd need to synchronize with this IPC since the
  // browser process filters out expired settings before sending the IPC.)
  content_settings::RuleMetaData metadata;
  metadata.SetFromConstraints(constraints);
  ContentSettingsForOneType settings = {
      ContentSettingPatternSource(
          ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostB)),
          ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
          base::Value(CONTENT_SETTING_ALLOW),
          content_settings::ProviderType::kPrefProvider,
          /*incognito=*/false, metadata),
      ContentSettingPatternSource(
          ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostC)),
          ContentSettingsPattern::FromURLNoWildcard(GetURL(kHostA)),
          base::Value(CONTENT_SETTING_ALLOW),
          content_settings::ProviderType::kPrefProvider,
          /*incognito=*/false, metadata),
  };

  auto* cookie_manager = browser()
                             ->profile()
                             ->GetDefaultStoragePartition()
                             ->GetCookieManagerForBrowserProcess();

  base::RunLoop runloop;
  auto barrier = base::BarrierClosure(2, runloop.QuitClosure());
  cookie_manager->SetContentSettings(ContentSettingsType::STORAGE_ACCESS,
                                     settings, barrier);
  cookie_manager->SetContentSettings(
      ContentSettingsType::TOP_LEVEL_STORAGE_ACCESS, settings, barrier);
  runloop.Run();

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/echoheader?cookie");

  EXPECT_EQ(GetNestedFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetNestedFrame()), "");

  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostC,
                                            /*cors_enabled=*/true),
            "");

  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetNestedFrame(), kHostC,
                                            /*cors_enabled=*/true),
            "");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForBrowserTest,
                       RsaForOriginEnabledByDefault) {
  NavigateToPageWithFrame(kHostA);
  // Ensure that the proposed extension is enabled by default
  EXPECT_EQ(
      EvalJs(GetPrimaryMainFrame(), "\"requestStorageAccessFor\" in document"),
      true);
}

class RequestStorageAccessForEnabledBrowserTest
    : public RequestStorageAccessForBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<bool, bool>> {
 public:
};

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForEnabledBrowserTest,
                       SameOriginGrantedByDefault) {
  SetBlockThirdPartyCookies(true);

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

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForEnabledBrowserTest,
                       TopLevelUnrelatedOriginRejected) {
  NavigateToPageWithFrame(kHostA);

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  EXPECT_EQ(content::EvalJs(GetPrimaryMainFrame(),
                            "navigator.userActivation.isActive",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE),
            false);
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForEnabledBrowserTest,
                       TopLevelOpaqueOriginRejected) {
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(),
                                           GURL("data:,Hello%2C%20World%21")));

  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForEnabledBrowserTest,
                       RequestStorageAccessForEmbeddedOriginScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  // Verify that the top-level scoping does not leak to the embedded URL, whose
  // origin must be used.
  NavigateToPageWithFrame(kHostB);

  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Regardless of the top-level site or origin scoping, the embedded origin
  // should be used.
  NavigateFrameTo(kHostASubdomain, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostASubdomain,
                                            /*cors_enabled=*/true),
            "");
}

// Tests to validate First-Party Set use with `requestStorageAccessFor`.
class RequestStorageAccessForWithFirstPartySetsBrowserTest
    : public RequestStorageAccessForBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RequestStorageAccessForBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }
};

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       PermissionQueryDefault) {
  NavigateToPageWithFrame(kHostA);
  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame(), kHostB), "prompt");
  // TODO(crbug.com/40256138): the `storage-access` permission seems to behave
  // similarly on self-queries. This is a counterintuitive result, however. It
  // does reflect the fact that the permission was never set, but it does not
  // reflect the fact that the `kHostA` top-level page's access to cookies on
  // `kHostA` is not actually blocked.
  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame(), kHostA), "prompt");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       PermissionQueryDoesNotShowDenied) {
  NavigateToPageWithFrame(kHostA);

  // First, get a rejection for `kHostD`, because it is not in the same
  // First-Party Set.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostD).spec()));

  // Then, validate that the rejection is not exposed via query, matching the
  // spec.
  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame(), kHostD), "prompt");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       PermissionQueryCrossSiteFrame) {
  NavigateToPageWithFrame(kHostA);

  // First, grant `kHostB` access.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame(), kHostB), "granted");

  NavigateFrameTo(kHostD, "/");

  // The cross-site frame on `kHostD` should not be able to get the state of
  // `kHostB` on `kHostA`.
  EXPECT_EQ(QueryPermission(GetFrame(), kHostB), "prompt");
}

// Validate that if a top-level document requests access that cookies become
// unblocked for just that top-level/third-party combination.
IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForWithFirstPartySetsBrowserTest,
    // TODO(crbug.com/40869547): Re-enable usage metric assertions.
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
  // The request comes from `kHostA`, which is in a First-Party Set with
  // `kHostB`. Note that `kHostB` would not be auto-granted access if it were
  // the requestor, because it is a service domain.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request from iframe does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  // Repeated calls should also return true.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request from iframe does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  // Subresource request with cors disabled does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/false),
            "");

  // Navigate iframe to a same-site, cookie-reading endpoint, and verify that
  // the cookie is not sent for a cross-site subresource request from iframe.
  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");

  // Also validate that an additional site C was not granted access.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostC,
                                            /*cors_enabled=*/true),
            "");

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  TopLevelStorageAccessRequestOutcome::kGrantedByFirstPartySet),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       Permission_NoUserGestureAfterPermissionGranted) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");

  // The request comes from `kHostA`, which is in a Related Website Set with
  // `kHostB`. Note that `kHostB` would not be auto-granted access if it were
  // the requestor, because it is a service domain.
  ASSERT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");

  // Repeated calls for the same origin should also return true, without
  // requiring a user gesture.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec(),
      /*omit_user_gesture=*/true));
}

// Validate that a user gesture is required.
IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       Permission_DeniedWithoutUserGesture) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  // The request comes from `kHostA`, which is in a First-Party Set with
  // `kHostB`. (Note that `kHostB` would not be auto-granted access if it were
  // the requestor, because it is a service domain.)
  //
  // kHostA would be autogranted access if the request has a user gesture, but
  // it doesn't.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec(),
      /*omit_user_gesture=*/true));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       AccessGranted_DoesNotConsumeUserGesture) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/");
  ASSERT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  EXPECT_EQ(content::EvalJs(GetPrimaryMainFrame(),
                            "navigator.userActivation.isActive",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE),
            true);
}

// Validate that the permission for rSAFor allows autogranting of rSA, including
// without a user gesture.
IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       Permission_AllowsRequestStorageAccessResolution) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/");
  // First, verify that executing `requestStorageAccess` without a user gesture
  // results in a rejection.
  EXPECT_FALSE(content::ExecJs(GetFrame(), kRequestStorageAccess,
                               content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  // Then invoke `requestStorageAccessFor` at the top level on behalf of
  // the frame.
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  // With the permission set, executing `requestStorageAccess` should now
  // resolve, even without a user gesture.
  EXPECT_TRUE(content::ExecJs(GetFrame(), kRequestStorageAccess,
                              content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test");

  EXPECT_EQ(content::EvalJs(GetFrame(), kVerifyHasStorageAccessPermission),
            true);

  NavigateFrameTo(kHostC, "/");
  // Verify that there was not a side effect on `kHostC`: invoking
  // `requestStorageAccess` without a user gesture should lead to rejection.
  EXPECT_FALSE(content::ExecJs(GetFrame(), kRequestStorageAccess,
                               content::EXECUTE_SCRIPT_NO_USER_GESTURE));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
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
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostA,
                                            /*cors_enabled=*/true),
            "");
  // The promise should be rejected; `kHostB` is a service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostA).spec()));

  // Re-navigate iframe to a cross-site, cookie-reading endpoint, and verify
  // that the cookie is not sent.
  NavigateFrameTo(kHostA, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostA,
                                            /*cors_enabled=*/true),
            "");

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  TopLevelStorageAccessRequestOutcome::kDeniedByPrerequisites),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedForServiceDomainInIframe) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  // `kHostB` cannot be granted access via `RequestStorageAccessFor`,
  // because the call is not from the top-level page and because `kHostB` is a
  // service domain.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetFrame(), GetURL(kHostA).spec()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
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
  // `kHostD` cannot be granted access via `RequestStorageAccessFor` in
  // this configuration, because the requesting site (`kHostA`) is not in the
  // same First-Party Set as the requested site (`kHostD`).
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostD).spec()));
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostD,
                                            /*cors_enabled=*/true),
            "");

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is not sent.
  NavigateFrameTo(kHostD, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostD,
                                            /*cors_enabled=*/true),
            "");

  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  TopLevelStorageAccessRequestOutcome::kDeniedByFirstPartySet),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       RequestStorageAccessForTopLevelScoping) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request from iframe does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  // Subresource request with cors disabled does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/false),
            "");

  NavigateToPageWithFrame(kHostASubdomain);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // Storage access grants are scoped to the embedded origin on the top-level
  // site. Accordingly, the access is be granted for subresource request.
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
}

IN_PROC_BROWSER_TEST_F(
    RequestStorageAccessForWithFirstPartySetsBrowserTest,
    RequestStorageAccessForTopLevelScopingWhenRequestedFromSubdomain) {
  SetBlockThirdPartyCookies(true);

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  NavigateToPageWithFrame(kHostASubdomain);

  // Allow all requests for kHostB to have cookie access from a.test.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));

  // Navigate iframe to a cross-site, cookie-reading endpoint, and verify that
  // the cookie is sent for the cors-enabled subresource request.
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request from iframe does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
  // Subresource request with cors disabled does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/false),
            "");

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // When top-level site scoping is enabled, the subdomain's grant counts for
  // the less-specific domain; otherwise, it does not.
  EXPECT_EQ(GetFrameContent(), "None");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test");
  // Subresource request from iframe does not have cookie access.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
}
IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       RequestExplicitlyDeniedResourceInFirstPartySet) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  // Set cross-site cookies on all hosts.
  SetCrossSiteCookieOnHost(kHostA);
  SetCrossSiteCookieOnHost(kHostB);

  // Block cookies at origin in browser settings
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(GetURL(kHostB), GetURL(kHostB),
                                      ContentSettingsType::COOKIES,
                                      CONTENT_SETTING_BLOCK);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/empty.html");

  // Attempt to request storage access for kHostB from kHostA.
  EXPECT_FALSE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  EXPECT_THAT(histogram_tester.GetBucketCount(
                  kRequestOutcomeHistogram,
                  TopLevelStorageAccessRequestOutcome::kDeniedByCookieSettings),
              Gt(0));

  // Verify that no cookies were sent.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "");
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       PRE_PermissionGrantsResetAfterRestart) {
  SetBlockThirdPartyCookies(true);
  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  ASSERT_EQ("granted", QueryPermission(GetPrimaryMainFrame(), kHostB));
}

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithFirstPartySetsBrowserTest,
                       PermissionGrantsResetAfterRestart) {
  SetBlockThirdPartyCookies(true);
  NavigateToPageWithFrame(kHostA);
  EXPECT_EQ("prompt", QueryPermission(GetPrimaryMainFrame(), kHostB));
}

class RequestStorageAccessForWithCHIPSBrowserTest
    : public RequestStorageAccessForBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    RequestStorageAccessForBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostC, R"("])",
                      R"(, "serviceSites": ["https://)", kHostB, R"("]})"}));
  }
};

IN_PROC_BROWSER_TEST_F(RequestStorageAccessForWithCHIPSBrowserTest,
                       RequestStorageAccessFor_CoexistsWithCHIPS) {
  SetBlockThirdPartyCookies(true);

  SetCrossSiteCookieOnHost(kHostB);
  SetPartitionedCookieInContext(/*top_level_host=*/kHostA,
                                /*embedded_host=*/kHostB);

  NavigateToPageWithFrame(kHostA);

  // kHostB starts without unpartitioned cookies:
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_EQ(GetFrameContent(), "cross-site=b.test(partitioned)");
  EXPECT_EQ(ReadCookiesViaJS(GetFrame()), "cross-site=b.test(partitioned)");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test(partitioned)");
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test(partitioned)");

  // kHostA can request storage access on behalf of kHostB, and it is granted
  // (by an implicit grant):
  EXPECT_TRUE(storage::test::RequestStorageAccessForOrigin(
      GetPrimaryMainFrame(), GetURL(kHostB).spec()));
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  // When the top-level frame makes a subresource request to an endpoint on
  // kHostB, kHostB's unpartitioned and partitioned cookies are sent.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetPrimaryMainFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test; cross-site=b.test(partitioned)");
  // When the frame makes a subresource request to an endpoint on kHostB,
  // only kHostB's partitioned cookies are sent.
  EXPECT_EQ(CookiesFromFetchWithCredentials(GetFrame(), kHostB,
                                            /*cors_enabled=*/true),
            "cross-site=b.test(partitioned)");
}

}  // namespace
