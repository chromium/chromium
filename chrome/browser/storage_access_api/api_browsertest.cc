// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <memory>
#include <string_view>

#include "base/containers/adapters.h"
#include "base/containers/map_util.h"
#include "base/containers/span.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/path_service.h"
#include "base/strings/escape.h"
#include "base/strings/strcat.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/content_settings/cookie_settings_factory.h"
#include "chrome/browser/content_settings/host_content_settings_map_factory.h"
#include "chrome/browser/net/storage_test_utils.h"
#include "chrome/browser/policy/policy_test_utils.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/storage_access_api/storage_access_grant_permission_context.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/tabs/tab_strip_model.h"
#include "chrome/browser/webid/federated_identity_permission_context.h"
#include "chrome/browser/webid/federated_identity_permission_context_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/content_settings/browser/page_specific_content_settings.h"
#include "components/content_settings/core/browser/cookie_settings.h"
#include "components/content_settings/core/browser/host_content_settings_map.h"
#include "components/content_settings/core/common/content_settings.h"
#include "components/content_settings/core/common/content_settings_types.h"
#include "components/content_settings/core/common/features.h"
#include "components/content_settings/core/common/pref_names.h"
#include "components/permissions/features.h"
#include "components/permissions/permission_request_manager.h"
#include "components/permissions/request_type.h"
#include "components/permissions/test/mock_permission_prompt_factory.h"
#include "components/permissions/test/permission_request_observer.h"
#include "components/policy/policy_constants.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/navigation_handle.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/common/content_features.h"
#include "content/public/common/content_paths.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "content/public/test/url_loader_interceptor.h"
#include "net/base/features.h"
#include "net/base/schemeful_site.h"
#include "net/cookies/canonical_cookie_test_helpers.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_request_headers.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/spawned_test_server/spawned_test_server.h"
#include "net/test/test_data_directory.h"
#include "services/network/public/cpp/features.h"
#include "services/network/public/cpp/network_switches.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom-forward.h"
#include "third_party/blink/public/mojom/use_counter/metrics/web_feature.mojom.h"
#include "ui/base/window_open_disposition.h"
#include "url/origin.h"

using content::BrowserThread;
using testing::Contains;
using testing::Gt;
using testing::IsEmpty;
using testing::Pair;
using testing::UnorderedElementsAre;

namespace {

constexpr char kHostA[] = "a.test";
constexpr char kOriginA[] = "https://a.test";
constexpr char kOriginB[] = "https://b.test";
constexpr char kUrlA[] = "https://a.test/random.path";
constexpr char kHostASubdomain[] = "subdomain.a.test";
constexpr char kHostB[] = "b.test";
constexpr char kHostBSubdomain[] = "subdomain.b.test";
constexpr char kHostBSubdomain2[] = "subdomain2.b.test";
constexpr char kHostC[] = "c.test";
constexpr char kHostD[] = "d.test";

constexpr char kUseCounterHistogram[] = "Blink.UseCounter.Features";
constexpr char kRequestOutcomeHistogram[] = "API.StorageAccess.RequestOutcome";
constexpr char kGrantIsImplicitHistogram[] =
    "API.StorageAccess.GrantIsImplicit";

// Path for URL of custom response
const char* kEchoCookiesWithCorsPath = "/echocookieswithcors";
constexpr char kRetryPath[] = "/retry-with-storage-access";

constexpr char kQueryStorageAccessPermission[] =
    "navigator.permissions.query({name: 'storage-access'}).then("
    "  (permission) => permission.state);";

constexpr char kHeaderNotProvidedSentinel[] = "HEADER_NOT_PROVIDED";

enum class TestType { kFrame, kWorker };

// Helpers to express expected
std::pair<std::string, std::string> CookieBundle(const std::string& cookies) {
  DCHECK_NE(cookies, "None");
  DCHECK_NE(cookies, "");
  return {cookies, cookies};
}

std::tuple<std::string, std::string, std::string> CookieBundleWithContent(
    const std::string& cookies) {
  DCHECK_NE(cookies, "None");
  DCHECK_NE(cookies, "");
  return {cookies, cookies, cookies};
}

constexpr std::pair<const char*, const char*> kNoCookies =
    std::make_pair("",     // cookie string via `document.cookie`
                   "None"  // cookie string via `echoheader?cookie`
    );

constexpr std::tuple<const char*, const char*, const char*>
    kNoCookiesWithContent =
        std::make_tuple("",      // cookie string via `document.cookie`
                        "None",  // cookie string via `echoheader?cookie`
                        "None"   // cookie string via frame content (also via
                                 // `echoheader?cookie`)
        );

// Executes the inner_matcher on the string arg after it's transformed into a
// vector of pairs of strings. This assumes that the argument is a string, whose
// value is a '\n' delimited list of name/value pairs (delimited by ':').
MATCHER_P(HeadersAre, inner_matcher, "") {
  return testing::ExplainMatchResult(net::WhenKVSplit('\n', ':', inner_matcher),
                                     arg, result_listener);
}

void SetCORSHeaders(const net::test_server::HttpRequest& request,
                    net::test_server::BasicHttpResponse& response) {
  if (auto it = request.headers.find(net::HttpRequestHeaders::kOrigin);
      it != request.headers.end()) {
    response.AddCustomHeader("Access-Control-Allow-Origin", it->second);
    response.AddCustomHeader("Vary", "origin");
    response.AddCustomHeader("Access-Control-Allow-Credentials", "true");
  }
}

// Responds to a request to /echocookieswithcors with the cookies that were sent
// with the request. We can't use the default handler /echoheader?Cookie here,
// because it doesn't send the appropriate Access-Control-Allow-Origin and
// Access-Control-Allow-Credentials headers (which are required for this to
// work for cross-origin requests in the tests).
std::unique_ptr<net::test_server::HttpResponse>
HandleEchoCookiesWithCorsRequest(const net::test_server::HttpRequest& request) {
  if (request.relative_url != kEchoCookiesWithCorsPath) {
    return nullptr;
  }
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  std::string content = "None";
  // Get the 'Cookie' header that was sent in the request.
  if (auto it = request.headers.find(net::HttpRequestHeaders::kCookie);
      it != request.headers.end()) {
    content = it->second;
  }

  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  SetCORSHeaders(request, *http_response);
  http_response->set_content(content);

  return http_response;
}

std::unique_ptr<net::test_server::HttpResponse> HandleRetryRequest(
    int& fetch_count,
    std::string_view allowed_origin,
    const net::test_server::HttpRequest& request) {
  if (request.relative_url != kRetryPath) {
    return nullptr;
  }

  fetch_count++;
  auto http_response = std::make_unique<net::test_server::BasicHttpResponse>();
  http_response->set_code(net::HTTP_OK);
  http_response->set_content_type("text/plain");
  http_response->AddCustomHeader(
      "Activate-Storage-Access",
      base::StrCat({"retry; allowed-origin=", allowed_origin}));
  SetCORSHeaders(request, *http_response);

  std::optional<std::string> storage_access_header =
      base::OptionalFromPtr(base::FindOrNull(
          request.headers, net::HttpRequestHeaders::kSecFetchStorageAccess));
  if (storage_access_header == "inactive") {
    std::optional<std::string> origin_header = base::OptionalFromPtr(
        base::FindOrNull(request.headers, net::HttpRequestHeaders::kOrigin));
    CHECK(origin_header);
    if (allowed_origin != "*" && origin_header) {
      std::string trimmed_allowed_origin;
      base::TrimString(allowed_origin, "\"", &trimmed_allowed_origin);
      EXPECT_EQ(trimmed_allowed_origin, origin_header);
    }
  }

  auto serialize_header_name_and_value =
      [&](std::string_view header_name) -> std::string {
    std::string value =
        base::OptionalFromPtr(base::FindOrNull(request.headers, header_name))
            .value_or(kHeaderNotProvidedSentinel);
    return base::JoinString({header_name, value}, ":");
  };

  http_response->set_content(base::JoinString(
      {
          serialize_header_name_and_value(net::HttpRequestHeaders::kCookie),
          serialize_header_name_and_value(
              net::HttpRequestHeaders::kSecFetchStorageAccess),
      },
      "\n"));

  return http_response;
}

std::string QueryPermission(content::RenderFrameHost* render_frame_host) {
  return content::EvalJs(render_frame_host, kQueryStorageAccessPermission)
      .ExtractString();
}

bool ThirdPartyPartitionedStorageAllowedByDefault() {
  return base::FeatureList::IsEnabled(
             net::features::kThirdPartyPartitionedStorageAllowedByDefault) &&
         base::FeatureList::IsEnabled(
             net::features::kThirdPartyStoragePartitioning);
}

std::string CookieAttributes(std::string_view domain) {
  return base::StrCat({";SameSite=None;Secure;Domain=", domain, ";Path=/"});
}

std::vector<base::test::FeatureRefAndParams> GetEnabledFeaturesForStorage(
    bool is_storage_partitioned) {
  std::vector<base::test::FeatureRefAndParams> enabled;
  if (is_storage_partitioned) {
    enabled.push_back({net::features::kThirdPartyStoragePartitioning, {}});
  }
  // WebSQL is disabled by default as of M119 (crbug/695592). Enable feature
  // in tests during deprecation trial and enterprise policy support.
  enabled.push_back({blink::features::kWebSQLAccess, {}});
  return enabled;
}

std::vector<base::test::FeatureRef> GetDisabledFeaturesForStorage(
    bool is_storage_partitioned) {
  std::vector<base::test::FeatureRef> disabled;
  if (!is_storage_partitioned) {
    disabled.push_back(net::features::kThirdPartyStoragePartitioning);
  }
  return disabled;
}

class StorageAccessAPIBaseBrowserTest : public policy::PolicyTest {
 protected:
  StorageAccessAPIBaseBrowserTest()
      : https_server_(net::EmbeddedTestServer::TYPE_HTTPS) {}

  void SetUp() override {
    features_.InitWithFeaturesAndParameters(GetEnabledFeatures(),
                                            GetDisabledFeatures());
    InProcessBrowserTest::SetUp();
  }

  virtual std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() {
    return {};
  }

  virtual std::vector<base::test::FeatureRef> GetDisabledFeatures() {
    return {};
  }

  void SetUpOnMainThread() override {
    host_resolver()->AddRule("*", "127.0.0.1");
    base::FilePath path;
    base::PathService::Get(content::DIR_TEST_DATA, &path);
    https_server_.SetSSLConfig(net::EmbeddedTestServer::CERT_TEST_NAMES);
    https_server_.ServeFilesFromDirectory(path);
    https_server_.ServeFilesFromSourceDirectory(
        net::GetWebSocketTestDataDirectory());
    https_server_.AddDefaultHandlers(GetChromeTestDataDir());
    https_server_.RegisterRequestHandler(
        base::BindRepeating(&HandleEchoCookiesWithCorsRequest));
    https_server_.RegisterRequestHandler(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          return HandleRetryRequest(retry_path_fetch_count_,
                                    retry_allowed_origin_, request);
        }));
    https_server_.RegisterRequestMonitor(base::BindLambdaForTesting(
        [&](const net::test_server::HttpRequest& request) {
          most_recent_request_headers_ = request.headers;
        }));
    ASSERT_TRUE(https_server_.Start());

    // All the sites used during these tests should have a cookie.
    SetCrossSiteCookieOnDomain(kHostA);
    SetCrossSiteCookieOnDomain(kHostB);
    SetCrossSiteCookieOnDomain(kHostC);
    SetCrossSiteCookieOnDomain(kHostD);

    prompt_factory_ = MakePromptFactory(browser());

    // Don't respond to the prompt at all, by default. This forces any test that
    // assumes a particular response to the prompt to explicitly declare that
    // assumption by setting up the auto-response themselves.
    prompt_factory_->set_response_type(
        permissions::PermissionRequestManager::NONE);

    // Most of these tests invoke document.requestStorageAccess from a kHostB
    // iframe. We pre-seed that site with user interaction, to avoid being
    // blocked by the top-level user interaction heuristic.
    EnsureUserInteractionOn(kHostB);
  }

  std::unique_ptr<permissions::MockPermissionPromptFactory> MakePromptFactory(
      Browser* browser_ptr) {
    CHECK(browser_ptr);
    return std::make_unique<permissions::MockPermissionPromptFactory>(
        permissions::PermissionRequestManager::FromWebContents(
            browser_ptr->tab_strip_model()->GetActiveWebContents()));
  }

  void TearDownOnMainThread() override { prompt_factory_.reset(); }

  void SetCrossSiteCookieOnDomain(const std::string& domain) {
    GURL domain_url = GetURL(domain);
    std::string cookie = base::StrCat({"cross-site=", domain});
    content::SetCookie(browser()->profile(), domain_url,
                       base::StrCat({cookie, CookieAttributes(domain)}));
    ASSERT_THAT(content::GetCookies(browser()->profile(), domain_url),
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
        base::StrCat({cookie, CookieAttributes(/*domain=*/embedded_host),
                      ";Partitioned"}),
        net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
        &partition_key);
    ASSERT_THAT(content::GetCookies(
                    browser()->profile(), host_url,
                    net::CookieOptions::SameSiteCookieContext::MakeInclusive(),
                    net::CookiePartitionKeyCollection(partition_key)),
                testing::HasSubstr(cookie));
  }

  void BlockAllCookiesOnHost(const std::string& host) {
    CookieSettingsFactory::GetForProfile(browser()->profile())
        ->SetCookieSetting(GetURL(host), ContentSetting::CONTENT_SETTING_BLOCK);
  }

  GURL GetURL(std::string_view host, std::string_view path = "/") {
    return https_server_.GetURL(host, path);
  }

  void SetBlockThirdPartyCookies(bool value) {
    browser()->profile()->GetPrefs()->SetInteger(
        prefs::kCookieControlsMode,
        static_cast<int>(
            value ? content_settings::CookieControlsMode::kBlockThirdParty
                  : content_settings::CookieControlsMode::kOff));
  }

  void NavigateToPage(const std::string& host, const std::string& path) {
    GURL main_url(https_server_.GetURL(host, path));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateToPageWithFrame(const std::string& host,
                               Browser* browser_ptr = nullptr) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser_ptr ? browser_ptr : browser(), main_url));
  }

  void NavigateToNewTabWithFrame(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/iframe.html"));
    ui_test_utils::NavigateToURLWithDisposition(
        browser(), main_url, WindowOpenDisposition::NEW_FOREGROUND_TAB,
        ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  }

  void NavigateFrameTo(const std::string& host, const std::string& path) {
    NavigateFrameTo(https_server_.GetURL(host, path));
  }

  void NavigateFrameTo(const GURL& url,
                       Browser* browser_ptr = nullptr,
                       const std::string& iframe_id = "test") {
    content::WebContents* web_contents = (browser_ptr ? browser_ptr : browser())
                                             ->tab_strip_model()
                                             ->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, iframe_id, url));
  }

  void NavigateNestedFrameTo(const std::string& host, const std::string& path) {
    NavigateNestedFrameTo(https_server_.GetURL(host, path));
  }

  // Navigates the innermost frame to the given URL. (The web_contents is
  // assumed to be showing a page containing an iframe that contains another
  // iframe.) The navigation's initiator is the middle iframe (not the leaf).
  void NavigateNestedFrameTo(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    content::TestNavigationObserver load_observer(web_contents);
    ASSERT_TRUE(ExecJs(
        GetFrame(),
        base::StringPrintf("document.body.querySelector('iframe').src = '%s';",
                           url.spec().c_str())));
    load_observer.Wait();
  }

  void NavigateToPageWithTwoFrames(const std::string& host) {
    GURL main_url(https_server_.GetURL(host, "/two_iframes_blank.html"));
    ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), main_url));
  }

  void NavigateFirstFrameTo(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "iframe1", url));
  }

  void NavigateSecondFrameTo(const GURL& url) {
    content::WebContents* web_contents =
        browser()->tab_strip_model()->GetActiveWebContents();
    EXPECT_TRUE(NavigateIframeToURL(web_contents, "iframe2", url));
  }

  GURL EchoCookiesURL(const std::string& host) {
    return https_server().GetURL(host, "/echoheader?cookie");
  }

  GURL RedirectViaHosts(const std::vector<std::string>& hosts,
                        const GURL& destination) {
    GURL url = destination;

    for (const auto& host : base::Reversed(hosts)) {
      url = https_server().GetURL(
          host, base::StrCat(
                    {"/server-redirect?", base::EscapeQueryParamValue(
                                              url.spec(), /*use_plus=*/true)}));
    }
    return url;
  }

  std::string CookiesFromFetch(content::RenderFrameHost* render_frame_host,
                               std::string_view subresource_host) {
    return ContentFromFetch(render_frame_host, subresource_host,
                            kEchoCookiesWithCorsPath);
  }

  std::string ContentFromFetch(content::RenderFrameHost* render_frame_host,
                               std::string_view subresource_host,
                               std::string_view path) {
    return storage::test::FetchWithCredentials(
        render_frame_host, https_server_.GetURL(subresource_host, path),
        /*cors_enabled=*/true);
  }

  // Reads cookies via `document.cookie` in the provided RFH, and via a
  // subresource request from the provided RFH to the given host. These are
  // bundled together to ensure that tests always check both, and that they're
  // consistent.
  std::pair<std::string, std::string> ReadCookies(
      content::RenderFrameHost* render_frame_host,
      const std::string& subresource_host) {
    return {
        content::EvalJs(render_frame_host, "document.cookie",
                        content::EXECUTE_SCRIPT_NO_USER_GESTURE)
            .ExtractString(),
        CookiesFromFetch(render_frame_host, subresource_host),
    };
  }

  // Reads cookies via `document.cookie` in the provided RFH, and via a
  // subresource request from the provided RFH to the given host, and also
  // includes the content of the provided RFH. This is most useful to check that
  // the cookies accessible during navigation (via `echoheader?cookie`), during
  // load (via `echoheader?cookie` for subresources), and via script execution
  // (via `document.cookie`) are consistent with each other.
  std::tuple<std::string, std::string, std::string> ReadCookiesAndContent(
      content::RenderFrameHost* render_frame_host,
      const std::string& subresource_host) {
    auto [js_cookies, subresource_cookies] =
        ReadCookies(render_frame_host, subresource_host);
    return {
        js_cookies,
        subresource_cookies,
        storage::test::GetFrameContent(render_frame_host),
    };
  }

  content::RenderFrameHost* GetPrimaryMainFrame(
      Browser* browser_ptr = nullptr) {
    content::WebContents* web_contents = (browser_ptr ? browser_ptr : browser())
                                             ->tab_strip_model()
                                             ->GetActiveWebContents();
    return web_contents->GetPrimaryMainFrame();
  }

  content::RenderFrameHost* GetFrame(Browser* browser_ptr = nullptr) {
    return ChildFrameAt(GetPrimaryMainFrame(browser_ptr), 0);
  }

  content::RenderFrameHost* GetNestedFrame(Browser* browser_ptr = nullptr) {
    return ChildFrameAt(GetFrame(browser_ptr), 0);
  }

  content::RenderFrameHost* GetFirstFrame() { return GetFrame(); }

  content::RenderFrameHost* GetSecondFrame() {
    return ChildFrameAt(GetPrimaryMainFrame(), 1);
  }

  void EnsureUserInteractionOn(std::string_view host,
                               Browser* browser_ptr = nullptr) {
    if (browser_ptr == nullptr) {
      browser_ptr = browser();
    }
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser_ptr, https_server_.GetURL(host, "/empty.html")));
    // ExecJs runs with a synthetic user interaction (by default), which is all
    // we need, so our script is a no-op.
    ASSERT_TRUE(content::ExecJs(
        browser_ptr->tab_strip_model()->GetActiveWebContents(), ""));
  }

  void OpenConnectToPage(content::RenderFrameHost* frame,
                         const net::SpawnedTestServer& wss_server) {
    std::string query = base::StrCat(
        {"url=", wss_server.GetURL(kHostB, "echo-request-headers").spec()});
    GURL::Replacements replacements;
    replacements.SetQueryStr(query);

    ASSERT_TRUE(content::NavigateToURLFromRenderer(
        frame, https_server()
                   .GetURL(kHostB, "/connect_to.html")
                   .ReplaceComponents(replacements)));
  }

  net::test_server::EmbeddedTestServer& https_server() { return https_server_; }

  permissions::MockPermissionPromptFactory* prompt_factory() {
    return prompt_factory_.get();
  }

  content_settings::PageSpecificContentSettings* content_settings() {
    return content_settings::PageSpecificContentSettings::GetForFrame(
        GetPrimaryMainFrame());
  }

  net::test_server::HttpRequest::HeaderMap MostRecentRequestHeaders() {
    return most_recent_request_headers_;
  }

  void SetRetryAllowedOriginFromHost(std::string_view host) {
    set_retry_allowed_origin(base::StrCat({
        "\"",
        url::Origin::Create(GetURL(host)).Serialize(),
        "\"",
    }));
  }

  void set_retry_allowed_origin(std::string_view allowed_origin) {
    retry_allowed_origin_ = allowed_origin;
  }

  int retry_path_fetch_count_ = 0;

 private:
  net::test_server::EmbeddedTestServer https_server_;
  base::test::ScopedFeatureList features_;
  std::unique_ptr<permissions::MockPermissionPromptFactory> prompt_factory_;
  net::test_server::HttpRequest::HeaderMap most_recent_request_headers_;
  std::string retry_allowed_origin_ = "";
};

// Test fixture for core Storage Access API functionality, guaranteed by spec.
// This fixture should use the minimal set of features/params.
class StorageAccessAPIBrowserTest : public StorageAccessAPIBaseBrowserTest {};

// Test fixture for tests whose behavior could be affected by Origin Isolation
// (kOriginKeyedProcessesByDefault). This is a parameterised version of
// StorageAccessAPIBrowserTest that runs each test with Origin Isolation
// enabled and disabled.
class StorageAccessAPIOriginIsolationBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public ::testing::WithParamInterface<bool> {
 public:
  StorageAccessAPIOriginIsolationBrowserTest() = default;
  ~StorageAccessAPIOriginIsolationBrowserTest() = default;

  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    if (GetParam()) {
      return {
          {features::kOriginKeyedProcessesByDefault, {}},
      };
    }
    return {};
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    if (!GetParam()) {
      return {features::kOriginKeyedProcessesByDefault};
    }
    return {};
  }
};

// Check default values for permissions.query on storage-access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, PermissionQueryDefault) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame()), "granted");
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");
}

// Check default values for permissions.query on storage-access when 3p cookie
// is allowed.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       PermissionQueryDefault_AllowCrossSiteCookie) {
  SetBlockThirdPartyCookies(false);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_EQ(QueryPermission(GetPrimaryMainFrame()), "granted");
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");
}

// Test that permissions.query changes to "granted" when a storage access
// request was successful.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, PermissionQueryGranted) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_THAT(content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());

  // Grant initial permission.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "granted");

  EXPECT_THAT(
      content_settings()->GetTwoSiteRequests(
          ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(Pair(net::SchemefulSite(GURL(kOriginB)), true)));

  // Ensure that after a navigation the permission state is preserved.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "granted");

  EXPECT_THAT(content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());

  // And the permission is regranted without prompt.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::NONE);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_THAT(
      content_settings()->GetTwoSiteRequests(
          ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(Pair(net::SchemefulSite(GURL(kOriginB)), true)));
}

// Test that permissions.query changes to "denied" when a storage access
// request was denied.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, PermissionQueryDenied) {
  SetBlockThirdPartyCookies(true);
  EnsureUserInteractionOn(kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_THAT(content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());

  // Deny initial permission.
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");

  EXPECT_THAT(
      content_settings()->GetTwoSiteRequests(
          ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(Pair(net::SchemefulSite(GURL(kOriginB)), false)));

  // Ensure that after a navigation the permission state is preserved.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/echoheader?cookie");
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");

  EXPECT_THAT(content_settings()->GetTwoSiteRequests(
                  ContentSettingsType::STORAGE_ACCESS),
              IsEmpty());

  // And the permission is denied without prompt.
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::NONE);
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_THAT(
      content_settings()->GetTwoSiteRequests(
          ContentSettingsType::STORAGE_ACCESS),
      UnorderedElementsAre(Pair(net::SchemefulSite(GURL(kOriginB)), false)));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, PermissionQueryCrossSite) {
  SetBlockThirdPartyCookies(true);

  EnsureUserInteractionOn(kHostA);

  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(kHostA, "/echoheader?cookie");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Grant initial permission.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(QueryPermission(GetFrame()), "granted");

  // Ensure that the scope of the permission grant is for the entire site.
  NavigateFrameTo(kHostASubdomain, "/echoheader?cookie");
  EXPECT_EQ(QueryPermission(GetFrame()), "granted");

  // The permission should not be available cross-site.
  NavigateFrameTo(kHostC, "/echoheader?cookie");
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       Permission_Denied_WithoutInteraction) {
  base::HistogramTester histogram_tester;
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::NONE);

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()",
                               content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(
      histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                      RequestOutcome::kDeniedByPrerequisites),
      Gt(0));
}

// Validate that a cross-site iframe can bypass third-party cookie blocking via
// the Storage Access API.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameRequestsAccess_CrossSiteIframe) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       AccessGranted_DoesNotConsumeUserInteraction) {
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  ASSERT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_EQ(content::EvalJs(GetFrame(), "navigator.userActivation.isActive",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE),
            true);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       AccessGranted_NoSubsequentUserInteraction) {
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  ASSERT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  ASSERT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->ResetCounts();

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      GetFrame(), /*omit_user_gesture=*/true));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 0);
}

// Validate that if an iframe obtains access, then cookies become unblocked for
// just that top-level/third-party combination and are still blocked for other
// combinations.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameRequestsAccess_CrossSiteIframe_UnrelatedSites) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateFrameTo(EchoCookiesURL(kHostC));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostC), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

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

// Validate that a nested A(B(B)) iframe can obtain cookie access, and that that
// access is not shared with the "middle" B iframe.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameRequestsAccess_NestedCrossSiteIframe_InnerRequestsAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostB),
            CookieBundle("cross-site=b.test"));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
}

// Validate that in a A(B) frame tree, the iframe can make credentialed
// same-site requests, even if the requests are cross-origin.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameRequestsAccess_CrossOriginFetch) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostBSubdomain));

  ASSERT_EQ(ReadCookies(GetFrame(), kHostBSubdomain), kNoCookies);
  ASSERT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_EQ(CookiesFromFetch(GetFrame(), kHostBSubdomain2),
            "cross-site=b.test");
}

// Validate that in a A(B(B)) frame tree, the middle B iframe can obtain access,
// and that access is not shared with the leaf B iframe.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameRequestsAccess_NestedCrossSiteIframe_MiddleRequestsAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostB), kNoCookies);

  // Subresource request from the cross-site iframe to an end point that's
  // same-origin with the top-level does not enable cookie access.
  EXPECT_EQ(CookiesFromFetch(GetFrame(), kHostA), "None");
  EXPECT_EQ(CookiesFromFetch(GetNestedFrame(), kHostA), "None");
}

// Validate that in a A(B(C)) frame tree, the C leaf iframe can obtain cookie
// access.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameRequestsAccess_NestedCrossSiteIframe_DistinctSites) {
  SetBlockThirdPartyCookies(true);
  EnsureUserInteractionOn(kHostC);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostC));

  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostC),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostC),
            CookieBundle("cross-site=c.test"));
}

// Validate that cross-site sibling iframes cannot take advantage of each
// other's granted permission.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesCrossSiteSiblingIFrameRequestsAccess) {
  EnsureUserInteractionOn(kHostC);

  NavigateToPageWithTwoFrames(kHostA);
  NavigateFirstFrameTo(EchoCookiesURL(kHostB));
  NavigateSecondFrameTo(EchoCookiesURL(kHostC));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), CONTENT_SETTING_ALLOW);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostC), CONTENT_SETTING_ALLOW);

  // Verify that both same-origin subresource request and cross-origin
  // subresource request can access cookies for the kHostB iframe.
  ASSERT_EQ(CookiesFromFetch(GetFirstFrame(), kHostB), "cross-site=b.test");
  ASSERT_EQ(CookiesFromFetch(GetFirstFrame(), kHostC), "cross-site=c.test");

  // Verify that both same-origin subresource request and cross-origin
  // subresource request can access cookies for the kHostC iframe.
  ASSERT_EQ(CookiesFromFetch(GetSecondFrame(), kHostC), "cross-site=c.test");
  ASSERT_EQ(CookiesFromFetch(GetSecondFrame(), kHostB), "cross-site=b.test");

  SetBlockThirdPartyCookies(true);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostB));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostC));
  // Navigate the first iframe to kHostB and grant Storage Access.
  NavigateFirstFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFirstFrame(), kHostB),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFirstFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetFirstFrame()));
  EXPECT_EQ(ReadCookies(GetFirstFrame(), kHostB),
            CookieBundle("cross-site=b.test"));

  // Navigate the second iframe to kHostC and grant Storage Access.
  NavigateSecondFrameTo(EchoCookiesURL(kHostC));
  EXPECT_EQ(ReadCookiesAndContent(GetSecondFrame(), kHostC),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetSecondFrame()));
  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetSecondFrame()));
  EXPECT_EQ(ReadCookies(GetSecondFrame(), kHostC),
            CookieBundle("cross-site=c.test"));

  // Verify same-origin subresource request has cookie access whereas the
  // cross-origin subresource request does not for the kHostB iframe.
  EXPECT_EQ(CookiesFromFetch(GetFirstFrame(), kHostB), "cross-site=b.test");
  EXPECT_EQ(CookiesFromFetch(GetFirstFrame(), kHostC), "None");

  // Verify same-origin subresource request has cookie access whereas the
  // cross-origin subresource request does not for the kHostC iframe.
  EXPECT_EQ(CookiesFromFetch(GetSecondFrame(), kHostC), "cross-site=c.test");
  EXPECT_EQ(CookiesFromFetch(GetSecondFrame(), kHostB), "None");
}

// Validate that the Storage Access API does not override any explicit user
// settings to block storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyCookiesIFrameThirdPartyExceptions) {
  SetBlockThirdPartyCookies(true);
  BlockAllCookiesOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(content::EvalJs(GetFrame(), "navigator.userActivation.isActive",
                            content::EXECUTE_SCRIPT_NO_USER_GESTURE),
            false);

  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
}

// Validate that user settings take precedence for the leaf in a A(B(B)) frame
// tree.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameThirdPartyExceptions_NestedSameSite) {
  SetBlockThirdPartyCookies(true);
  BlockAllCookiesOnHost(kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(
      content::ExecJs(GetNestedFrame(), "document.requestStorageAccess()"));

  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostB), kNoCookies);
}

// Validate that user settings take precedence for the leaf in a A(B(C)) frame
// tree.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameThirdPartyExceptions_NestedCrossSite) {
  EnsureUserInteractionOn(kHostC);

  SetBlockThirdPartyCookies(true);
  BlockAllCookiesOnHost(kHostC);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostC));

  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostC),
            kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(
      content::ExecJs(GetNestedFrame(), "document.requestStorageAccess()"));

  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostC), kNoCookies);
}

// Validate that user settings take precedence for the leaf in a A(B(A)) frame
// tree.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameThirdPartyExceptions_CrossSiteAncestorChain) {
  EnsureUserInteractionOn(kHostA);
  SetBlockThirdPartyCookies(true);
  BlockAllCookiesOnHost(kHostA);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostA));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetPrimaryMainFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(
      content::ExecJs(GetNestedFrame(), "document.requestStorageAccess()"));

  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostA),
            kNoCookiesWithContent);
}

// Validate that user settings take precedence for the leaf in a A(A) frame
// tree.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    ThirdPartyCookiesIFrameThirdPartyExceptions_SameSiteAncestorChain) {
  EnsureUserInteractionOn(kHostA);
  SetBlockThirdPartyCookies(true);
  BlockAllCookiesOnHost(kHostA);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostA));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetPrimaryMainFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostA), kNoCookiesWithContent);
}

// Validates that once a grant is removed access is also removed.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ThirdPartyGrantsDeletedAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));

  // Manually delete all our grants.
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  settings_map->ClearSettingsForOneType(ContentSettingsType::STORAGE_ACCESS);

  // Try to ensure that the pref observer is triggered and the updated settings
  // are propagated to the network service.
  base::RunLoop().RunUntilIdle();
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // Verify cookie cannot be accessed.
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);

  // Access change should be reflected immediately.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

// Validates that if the user explicitly blocks cookies, cookie access is
// blocked even with the existing grant.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       ExplicitUserSettingsBlockThirdPartyGrantsAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));

  BlockAllCookiesOnHost(kHostB);
  // Try to ensure that the pref observer is triggered and the updated settings
  // are propagated to the network service.
  base::RunLoop().RunUntilIdle();
  browser()
      ->profile()
      ->GetDefaultStoragePartition()
      ->FlushNetworkInterfaceForTesting();

  // Access change should be reflected immediately.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // TODO(crbug.com/40266432): should EXPECT_FALSE here since user
  // explicitly blocks cookies for hostB
  EXPECT_TRUE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
}

// Validate that if the iframe's origin is opaque, it cannot obtain storage
// access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, OpaqueOriginRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(
      ExecJs(GetPrimaryMainFrame(),
             "document.querySelector('iframe').sandbox='allow-scripts';"));
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

// Validate that if the iframe is sandboxed and allows scripts but is missing
// the Storage Access sandbox tag, the iframe cannot obtain storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       MissingSandboxTokenRejects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(ExecJs(GetPrimaryMainFrame(),
                     "document.querySelector('iframe').sandbox='allow-"
                     "scripts allow-same-origin';"));
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
}

// Validate that if the iframe is sandboxed and has the Storage Access sandbox
// tag, the iframe can obtain storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, SandboxTokenResolves) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  ASSERT_TRUE(
      ExecJs(GetPrimaryMainFrame(),
             "document.querySelector('iframe').sandbox='allow-scripts "
             "allow-same-origin allow-storage-access-by-user-activation';"));
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));
}

// Validates that expired grants don't get reused.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, ThirdPartyGrantsExpiry) {
  base::HistogramTester histogram_tester;
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostC));

  // Manually create a pre-expired grant and ensure it doesn't grant access for
  // HostB.
  const base::TimeDelta lifetime = base::Days(30);
  const base::Time creation_time =
      base::Time::Now() - base::Minutes(5) - lifetime;
  HostContentSettingsMap* settings_map =
      HostContentSettingsMapFactory::GetForProfile(browser()->profile());
  content_settings::ContentSettingConstraints constraints(creation_time);
  constraints.set_lifetime(lifetime);
  constraints.set_session_model(
      content_settings::mojom::SessionModel::USER_SESSION);
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostB), GetURL(kHostA), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW, constraints);
  settings_map->SetContentSettingDefaultScope(
      GetURL(kHostC), GetURL(kHostA), ContentSettingsType::STORAGE_ACCESS,
      CONTENT_SETTING_ALLOW);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // The iframe should request for new grant since the existing one is expired.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Validate that only one permission was newly granted.
  histogram_tester.ExpectUniqueSample(kRequestOutcomeHistogram,
                                      RequestOutcome::kGrantedByUser, 1);

  // The nested iframe reuses the existing grant without prompting.
  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostC),
            CookieBundle("cross-site=c.test"));

  histogram_tester.ExpectTotalCount(kRequestOutcomeHistogram, 2);
  histogram_tester.ExpectBucketCount(
      kRequestOutcomeHistogram, RequestOutcome::kReusedPreviousDecision, 1);

  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostC));
  // Only when the initiator is the frame that's been navigated can inherit
  // per-frame storage access.
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostC),
            kNoCookiesWithContent);
}

// Validate that if an iframe navigates itself to a same-origin endpoint, and
// that navigation does not include any cross-origin redirects, the new document
// can inherit storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       Navigation_SelfInitiated_SameOrigin_Preserves) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(
      content::NavigateToURLFromRenderer(GetFrame(), EchoCookiesURL(kHostB)));

  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB),
            CookieBundleWithContent("cross-site=b.test"));
}

// Validate that if an iframe is navigated (by a cross-site initiator) to a
// same-origin endpoint, and that navigation does not include any cross-origin
// redirects, the new document cannot inherit storage access.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    Navigation_NonSelfInitiated_SameOriginDestination_CrossSiteInitiator) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The navigation for this frame does not include cookies, since the initiator
  // is cross-site from the destination, and the initiator did not have storage
  // access.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
}

// Validate that if an iframe is navigated (by a same-site initiator) to a
// same-origin endpoint (even if the navigation does not include any
// cross-origin redirects), the new document cannot inherit storage access.
IN_PROC_BROWSER_TEST_P(
    StorageAccessAPIOriginIsolationBrowserTest,
    Navigation_NonSelfInitiated_SameOriginDestination_SameSiteInitiator) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostBSubdomain));

  ASSERT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  ASSERT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateNestedFrameTo(EchoCookiesURL(kHostBSubdomain));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // The navigation itself carried cookies due to the initiator's storage
  // access, but the new document did not inherit storage access, since the
  // navigation was not self-initiated.
  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            std::make_tuple("", "None", "cross-site=b.test"));
}

// Validate that if an iframe is navigated (by a same-site initiator) to a
// same-origin endpoint (even if the navigation does not include any
// cross-origin redirects, and the navigated frame has obtained storage access
// already), the new document cannot inherit storage access.
IN_PROC_BROWSER_TEST_P(
    StorageAccessAPIOriginIsolationBrowserTest,
    Navigation_NonSelfInitiated_SameOriginDestination_SameSiteInitiator_TargetHasStorageAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(EchoCookiesURL(kHostBSubdomain));

  ASSERT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  ASSERT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));

  NavigateNestedFrameTo(EchoCookiesURL(kHostBSubdomain));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  // The navigation itself carried cookies due to the initiator's storage
  // access, but the new document did not inherit storage access, since the
  // navigation was not self-initiated.
  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            std::make_tuple("", "None", "cross-site=b.test"));
}

// Validate that if an iframe navigates itself to a same-site cross-origin
// endpoint, and that navigation does not include any cross-origin redirects,
// the new document cannot inherit storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       Navigation_SelfInitiated_SameSiteCrossOrigin) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      GetFrame(), EchoCookiesURL(kHostBSubdomain)));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The navigation itself carried cookies from the previous document's storage
  // access, but the new document did not inherit storage access.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB),
            std::make_tuple("", "None", "cross-site=b.test"));
}

// Validate that if an iframe navigates itself to a cross-site endpoint, and
// that navigation does not include any cross-origin redirects, the new document
// cannot inherit storage access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       Navigation_SelfInitiated_CrossSite) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(
      content::NavigateToURLFromRenderer(GetFrame(), EchoCookiesURL(kHostC)));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostC), kNoCookiesWithContent);
}

// Validate that if an iframe navigates itself to a same-origin endpoint, but
// that navigation include a cross-origin redirect, the new document
// cannot inherit storage access.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    Navigation_SelfInitiated_SameOrigin_CrossOriginRedirect) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  GURL dest = EchoCookiesURL(kHostB);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      GetFrame(),
      /*url=*/
      RedirectViaHosts({kHostBSubdomain}, dest),
      /*expected_commit_url=*/dest));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The navigation itself carried cookies from the previous document's storage
  // access, but the new document did not inherit storage access.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostBSubdomain),
            std::make_tuple("", "None", "cross-site=b.test"));
}

// Validate that if an iframe navigates itself to a same-origin endpoint, and
// that navigation includes a cross-origin redirect (even if there's a
// subsequent same-origin redirect), the new document cannot inherit storage
// access.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIBrowserTest,
    Navigation_SelfInitiated_SameOrigin_CrossSiteAndSameSiteRedirects) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  GURL dest = EchoCookiesURL(kHostB);
  EXPECT_TRUE(content::NavigateToURLFromRenderer(
      GetFrame(),
      /*url=*/
      RedirectViaHosts({kHostBSubdomain, kHostB}, dest),
      /*expected_commit_url=*/dest));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  // The navigation itself carried cookies from the previous document's storage
  // access, but the new document did not inherit storage access.
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostBSubdomain),
            std::make_tuple("", "None", "cross-site=b.test"));
}

// Validate that in a A(A) frame tree, the inner A iframe can obtain cookie
// access by default.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       EmbeddedSameOriginCookieAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostA));

  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostA),
            CookieBundleWithContent("cross-site=a.test"));
}

// Validate that in a A(sub.A) frame tree, the inner A iframe can obtain cookie
// access by default.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       EmbeddedSameSiteCookieAccess) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostASubdomain));

  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostA),
            CookieBundleWithContent("cross-site=a.test"));
}

// Validate that in a A(B(A)) frame tree, the inner A iframe can obtain cookie
// access after requesting access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       NestedSameOriginCookieAccess_CrossSiteAncestorChain) {
  base::HistogramTester histogram_tester;
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostA, "/empty.html");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostA), kNoCookies);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostA),
            CookieBundle("cross-site=a.test"));
  histogram_tester.ExpectTotalCount(kRequestOutcomeHistogram, 1);
  histogram_tester.ExpectBucketCount(kRequestOutcomeHistogram,
                                     RequestOutcome::kAllowedBySameSite, 1);
}

// Validate that in a A(B(sub.A)) frame tree, the inner iframe can obtain cookie
// access after requesting access.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       NestedSameSiteCookieAccess_CrossSiteAncestorChain) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostASubdomain, "/empty.html");

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostA), kNoCookies);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostASubdomain),
            CookieBundle("cross-site=a.test"));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       DedicatedWorker_InheritsStorageAccessFromDocument) {
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  // Get storage access and then do a self-initiated same-site navigation to the
  // fetch_from_worker.html, so that fetch_from_worker.html has storage access
  // upon load (and therefore has storage access when it creates the Worker).
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  ASSERT_TRUE(content::NavigateToURLFromRenderer(
      GetFrame(),
      https_server().GetURL(
          kHostB,
          "/workers/fetch_from_worker.html?script=fetch_from_worker.js")));
  ASSERT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // When the worker's parent document has storage access at the time the worker
  // is created, the worker should inherit that access and be able to use it.
  EXPECT_EQ(
      content::EvalJs(GetFrame(), "fetch_from_worker('/echoheader?cookie');"),
      "cross-site=b.test");
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       WebsocketRequestsUseStorageAccessGrants) {
  net::SpawnedTestServer wss_server(
      net::SpawnedTestServer::TYPE_WSS,
      net::SpawnedTestServer::SSLOptions(
          net::SpawnedTestServer::SSLOptions::CERT_TEST_NAMES),
      net::GetWebSocketTestDataDirectory());
  ASSERT_TRUE(wss_server.Start());

  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  content::DOMMessageQueue message_queue(
      browser()->tab_strip_model()->GetActiveWebContents());
  // Before the document opts into Storage Access, the WebSocket should not send
  // unpartitioned cookies during the connection.
  {
    OpenConnectToPage(GetFrame(), wss_server);

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_THAT(message, testing::Not(testing::HasSubstr("cross-site=b.test")));
  }

  // Get storage access and then do a self-initiated same-site navigation to the
  // connect_to.html, so that the websocket's frame has storage access
  // upon load (and therefore has storage access when it creates the websocket).
  NavigateFrameTo(EchoCookiesURL(kHostB));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // After the document opts into Storage Access, the WebSocket *should* send
  // unpartitioned cookies during the connection.
  {
    OpenConnectToPage(GetFrame(), wss_server);
    ASSERT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

    std::string message;
    EXPECT_TRUE(message_queue.WaitForMessage(&message));
    EXPECT_THAT(message, testing::HasSubstr("cross-site=b.test"));
  }
}

// Validate that in a A(B) frame tree, the embedded B iframe can obtain cookie
// access if requested and got accepted.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       EmbeddedCrossSiteCookieAccess_Accept) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_EQ(1, prompt_factory()->RequestTypeSeen(
                   permissions::RequestType::kStorageAccess));
}

// Validate that in a A(B) frame tree, the embedded B iframe can not obtain
// cookie access if requested and got denied.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       EmbeddedCrossSiteCookieAccess_Deny) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(1, prompt_factory()->TotalRequestCount());
  EXPECT_EQ(1, prompt_factory()->RequestTypeSeen(
                   permissions::RequestType::kStorageAccess));
}

// Validate that if third-party cookies are blocked and the permission is
// denied, requestStorageAccess beyond cookies fails.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       BeyondCookies_WithoutCookiesWithoutPermission) {
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_FALSE(storage::test::RequestAndCheckStorageAccessBeyondCookiesForFrame(
      GetFrame()));
}

// Validate that if third-party cookies are blocked but the permission is
// allowed, requestStorageAccess beyond cookies succeeds.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       BeyondCookies_WithoutCookiesWithPermission) {
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessBeyondCookiesForFrame(
      GetFrame()));
}

class StorageAccessAPIStorageBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<std::tuple<TestType, bool>> {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return GetEnabledFeaturesForStorage(IsStoragePartitioned());
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return GetDisabledFeaturesForStorage(IsStoragePartitioned());
  }

  void ExpectStorage(content::RenderFrameHost* frame, bool expected) {
    switch (GetTestType()) {
      case TestType::kFrame:
        storage::test::ExpectStorageForFrame(frame, expected);
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

  bool DoesPermissionGrantStorage() const { return IsStoragePartitioned(); }

 private:
  TestType GetTestType() const { return std::get<0>(GetParam()); }
  bool IsStoragePartitioned() const { return std::get<1>(GetParam()); }
};

// Validate that the Storage Access API will unblock other types of storage
// access when a grant is given and that it only applies to the top-level/third
// party pair requested on.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest,
                       ThirdPartyIFrameStorageRequestsAccess) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), CONTENT_SETTING_ALLOW);

  ExpectStorage(GetFrame(), false);
  SetStorage(GetFrame());
  ExpectStorage(GetFrame(), true);

  SetBlockThirdPartyCookies(true);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostB));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  ExpectStorage(GetFrame(), DoesPermissionGrantStorage());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest,
                       NestedThirdPartyIFrameStorage) {
  EnsureUserInteractionOn(kHostC);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), CONTENT_SETTING_ALLOW);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostC), CONTENT_SETTING_ALLOW);

  ExpectStorage(GetNestedFrame(), false);
  SetStorage(GetNestedFrame());
  ExpectStorage(GetNestedFrame(), true);

  SetBlockThirdPartyCookies(true);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostB));
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostC));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(),
                ThirdPartyPartitionedStorageAllowedByDefault());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/iframe.html");
  NavigateNestedFrameTo(kHostC, "/browsing_data/site_data.html");

  ExpectStorage(GetNestedFrame(), DoesPermissionGrantStorage());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

// Test third-party cookie blocking of features that allow to communicate
// between tabs such as SharedWorkers.
IN_PROC_BROWSER_TEST_P(StorageAccessAPIStorageBrowserTest, MultiTabTest) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), CONTENT_SETTING_ALLOW);

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), false);
  storage::test::SetCrossTabInfoForFrame(GetFrame());
  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Create a second tab to test communication between tabs.
  NavigateToNewTabWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  permissions::PermissionRequestManager::FromWebContents(
      browser()->tab_strip_model()->GetActiveWebContents())
      ->set_auto_response_for_test(
          permissions::PermissionRequestManager::ACCEPT_ALL);

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(), true);
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  SetBlockThirdPartyCookies(true);
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->ResetCookieSetting(GetURL(kHostB));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(
      GetFrame(), ThirdPartyPartitionedStorageAllowedByDefault());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");

  storage::test::ExpectCrossTabInfoForFrame(GetFrame(),
                                            DoesPermissionGrantStorage());
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         StorageAccessAPIOriginIsolationBrowserTest,
                         testing::Bool(),
                         [](const testing::TestParamInfo<bool>& info) {
                           return info.param ? "origin_keyed" : "site_keyed";
                         });

INSTANTIATE_TEST_SUITE_P(/*no prefix*/,
                         StorageAccessAPIStorageBrowserTest,
                         testing::Combine(testing::Values(TestType::kFrame,
                                                          TestType::kWorker),
                                          testing::Bool()));

class StorageAccessAPIWithFirstPartySetsBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  void SetUpCommandLine(base::CommandLine* command_line) override {
    StorageAccessAPIBaseBrowserTest::SetUpCommandLine(command_line);
    command_line->AppendSwitchASCII(
        network::switches::kUseRelatedWebsiteSet,
        base::StrCat({R"({"primary": "https://)", kHostA,
                      R"(", "associatedSites": ["https://)", kHostB, R"("])",
                      R"(, "serviceSites": ["https://)", kHostD, R"("]})"}));
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_AutograntedWithinFirstPartySet) {
  base::HistogramTester histogram_tester;
  // Note: kHostA and kHostB are considered same-party due to the use of
  // `network::switches::kUseFirstPartySet`.
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(
      histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                      RequestOutcome::kGrantedByFirstPartySet),
      Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_PromptOrDenyUnderServiceDomain) {
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  EnsureUserInteractionOn(kHostA);
  SetBlockThirdPartyCookies(true);

  base::HistogramTester histogram_tester;

  NavigateToPageWithFrame(kHostD);
  NavigateFrameTo(EchoCookiesURL(kHostA));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostA), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // The promise should be rejected; `kHostD` is a service domain.
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);

  NavigateFrameTo(EchoCookiesURL(kHostA));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostA), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kDeniedByUser),
              Gt(0));
  // Ensure that the denied state is not exposed to developers, per the spec.
  EXPECT_EQ(QueryPermission(GetFrame()), "prompt");
}

IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIWithFirstPartySetsBrowserTest,
    Permission_AutograntedForServiceDomainWithExistingGrant) {
  SetBlockThirdPartyCookies(true);

  // Manually create a grant for the service site. Other test cases show that
  // the service site cannot create this grant on its own, but such a grant can
  // be created via other APIs (namely `document.requestStorageAccessFor`).
  content_settings::ContentSettingConstraints constraints;
  constraints.set_lifetime(base::Days(30));
  constraints.set_session_model(
      content_settings::mojom::SessionModel::NON_RESTORABLE_USER_SESSION);
  HostContentSettingsMapFactory::GetForProfile(browser()->profile())
      ->SetContentSettingDefaultScope(GetURL(kHostD), GetURL(kHostA),
                                      ContentSettingsType::STORAGE_ACCESS,
                                      CONTENT_SETTING_ALLOW, constraints);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostD));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostD), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostD), CookieBundle("cross-site=d.test"));

  EXPECT_EQ(QueryPermission(GetFrame()), "granted");
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_AutodeniedOutsideFirstPartySet_Overridden) {
  base::HistogramTester histogram_tester;
  // Note: kHostA and kHostC are considered cross-party, since kHostA's set does
  // not include kHostC.
  SetBlockThirdPartyCookies(true);
  EnsureUserInteractionOn(kHostC);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostC));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostC), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostC), CookieBundle("cross-site=c.test"));

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                              RequestOutcome::kGrantedByUser),
              Gt(0));
}

IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIWithFirstPartySetsBrowserTest,
    Permission_AutodeniedInsideFirstPartySet_WithoutInteraction) {
  base::HistogramTester histogram_tester;
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()",
                               content::EXECUTE_SCRIPT_NO_USER_GESTURE));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);

  content::FetchHistogramsFromChildProcesses();

  EXPECT_THAT(
      histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                      RequestOutcome::kDeniedByPrerequisites),
      Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       PRE_PermissionGrantsRestoredAfterRestart) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  ASSERT_EQ("granted", QueryPermission(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       PermissionGrantsRestoredAfterRestart) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ("granted", QueryPermission(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       Permission_GrantedForServiceDomain) {
  SetBlockThirdPartyCookies(true);
  base::HistogramTester histogram_tester;

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostD));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostD), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // `kHostD` is a service domain, but is not the top-level site, so the request
  // should be auto-granted.
  EXPECT_TRUE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  content::FetchHistogramsFromChildProcesses();
  EXPECT_THAT(
      histogram_tester.GetBucketCount(kRequestOutcomeHistogram,
                                      RequestOutcome::kGrantedByFirstPartySet),
      Gt(0));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithFirstPartySetsBrowserTest,
                       SameSite_NoRegression) {
  // Note: kHostA and kHostB are considered same-party due to the use of
  // `network::switches::kUseFirstPartySet`. But they should not be "same-site",
  // so SameSite=Lax and SameSite=Strict should still block cookie access.
  ASSERT_TRUE(
      SetCookie(browser()->profile(), GetURL(kHostB),
                "samesitelax=1; SameSite=Lax; Secure",
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));
  ASSERT_TRUE(
      SetCookie(browser()->profile(), GetURL(kHostB),
                "samesitestrict=1; SameSite=Strict; Secure",
                net::CookieOptions::SameSiteCookieContext::MakeInclusive()));
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_EQ(CookiesFromFetch(GetFrame(), kHostB), "cross-site=b.test");
}

class StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest() {
    StorageAccessGrantPermissionContext::SetImplicitGrantLimitForTesting(5);
  }
};

// Validate that when auto-deny-outside-fps is disabled (but auto-grant is
// enabled), implicit grants still work.
IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIWithFirstPartySetsAndImplicitGrantsBrowserTest,
    ImplicitGrants) {
  // Note: kHostA and kHostC are considered cross-party, since kHostA's set does
  // not include kHostC.
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostC));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostC), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // kHostC can request storage access, due to implicit grants.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(EchoCookiesURL(kHostC));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostC), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       RequestStorageAccess_CoexistsWithPartitionedCookies) {
  SetBlockThirdPartyCookies(true);

  SetPartitionedCookieInContext(/*top_level_host=*/kHostA,
                                /*embedded_host=*/kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB),
            CookieBundleWithContent("cross-site=b.test(partitioned)"));
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB),
            CookieBundle("cross-site=b.test; cross-site=b.test(partitioned)"));
}

class StorageAccessAPIEnterprisePolicyBrowserTest
    : public StorageAccessAPIBaseBrowserTest,
      public testing::WithParamInterface<
          /* (origin, content_setting, is_storage_partitioned) */
          std::tuple<const char*, ContentSetting, bool>> {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return GetEnabledFeaturesForStorage(IsStoragePartitioned());
  }

  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return GetDisabledFeaturesForStorage(IsStoragePartitioned());
  }

  void SetUpInProcessBrowserTestFixture() override {
    policy::PolicyTest::SetUpInProcessBrowserTestFixture();
    policy::PolicyMap policies;
    SetPolicy(&policies,
              policy::key::kDefaultThirdPartyStoragePartitioningSetting,
              base::Value(GetContentSetting()));
    base::Value::List origins;
    origins.Append(base::Value(GetContentOrigin()));
    SetPolicy(&policies,
              policy::key::kThirdPartyStoragePartitioningBlockedForOrigins,
              base::Value(std::move(origins)));
    UpdateProviderPolicy(policies);
  }

  bool ExpectPartitionedStorage() const {
    // We only expect storage to be partitioned if the base::Feature is enabled
    // and the default content setting isn't BLOCK and the origin block list
    // doesn't match a.test (paths are ignored)
    return IsStoragePartitioned() &&
           GetContentSetting() != CONTENT_SETTING_BLOCK &&
           GetContentOrigin() != kHostA && GetContentOrigin() != kOriginA &&
           GetContentOrigin() != kUrlA;
  }

  // Derive a test name from parameter information.
  static std::string TestName(const ::testing::TestParamInfo<ParamType>& info) {
    const char* origin = std::get<0>(info.param);
    ContentSetting content_setting = std::get<1>(info.param);
    bool is_storage_partitioned = std::get<2>(info.param);
    return base::JoinString(
        {
            origin == kHostA            ? "kHostA"
            : origin == kOriginA        ? "kOriginA"
            : origin == kUrlA           ? "kUrlA"
            : origin == kHostASubdomain ? "kHostASubdomain"
            : origin == kHostB          ? "kHostB"
                                        : "empty",
            content_setting == CONTENT_SETTING_DEFAULT ? "DEFAULT"
            : content_setting == CONTENT_SETTING_ALLOW ? "ALLOW"
                                                       : "BLOCK",
            is_storage_partitioned ? "Partitioned" : "Unpartitioned",
        },
        "_");
  }

 private:
  ContentSetting GetContentSetting() const { return std::get<1>(GetParam()); }
  const char* GetContentOrigin() const { return std::get<0>(GetParam()); }
  bool IsStoragePartitioned() const { return std::get<2>(GetParam()); }
};

INSTANTIATE_TEST_SUITE_P(
    /*no prefix*/,
    StorageAccessAPIEnterprisePolicyBrowserTest,
    testing::Combine(
        testing::Values(kHostA, kOriginA, kUrlA, kHostASubdomain, kHostB, ""),
        testing::Values(CONTENT_SETTING_DEFAULT,
                        CONTENT_SETTING_ALLOW,
                        CONTENT_SETTING_BLOCK),
        testing::Bool()),
    StorageAccessAPIEnterprisePolicyBrowserTest::TestName);

IN_PROC_BROWSER_TEST_P(StorageAccessAPIEnterprisePolicyBrowserTest,
                       PartitionedStorage) {
  // Navigate to Origin B, setup storage, and expect storage.
  NavigateToPage(kHostB, "/browsing_data/site_data.html");
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSetting(GetURL(kHostB), CONTENT_SETTING_ALLOW);
  storage::test::ExpectStorageForFrame(GetPrimaryMainFrame(),
                                       /*expected=*/false);
  storage::test::SetStorageForFrame(GetPrimaryMainFrame(),
                                    /*include_cookies=*/false);
  storage::test::ExpectStorageForFrame(GetPrimaryMainFrame(),
                                       /*expected=*/true);

  // Navigate to Origin A w/ Frame B and expect storage if not partitioned.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(kHostB, "/browsing_data/site_data.html");
  storage::test::ExpectStorageForFrame(GetFrame(), !ExpectPartitionedStorage());
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       EnsureOnePromptDenialSuffices) {
  SetBlockThirdPartyCookies(true);
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  {
    // The first request should show a prompt, which is denied.
    permissions::PermissionRequestObserver pre_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    ASSERT_FALSE(pre_observer.request_shown());
    ASSERT_FALSE(
        content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
    ASSERT_TRUE(pre_observer.request_shown());
    ASSERT_EQ(prompt_factory()->TotalRequestCount(), 1);
  }
  {
    // However, subsequent requests should not re-prompt.
    permissions::PermissionRequestObserver post_observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    // Validate that there's no stale data.
    ASSERT_FALSE(post_observer.request_shown());

    EXPECT_FALSE(
        content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
    // Verify no prompt was shown after the first one was already denied.
    EXPECT_FALSE(post_observer.request_shown());
    EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);
  }
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       DismissalAllowsFuturePrompts) {
  SetBlockThirdPartyCookies(true);
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DISMISS);

  {
    // The first request should show a prompt, which is dismissed.
    permissions::PermissionRequestObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());
    EXPECT_FALSE(
        content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
    ASSERT_TRUE(observer.request_shown());
    EXPECT_EQ(false,
              content::EvalJs(GetFrame(), "document.hasStorageAccess()"));
    ASSERT_EQ(prompt_factory()->TotalRequestCount(), 1);
  }

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  {
    // However, subsequent requests should be able to re-prompt.
    permissions::PermissionRequestObserver observer(
        browser()->tab_strip_model()->GetActiveWebContents());

    EXPECT_TRUE(
        storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

    // Verify a prompt was shown.
    EXPECT_TRUE(observer.request_shown());
    EXPECT_EQ(prompt_factory()->TotalRequestCount(), 2);
  }
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       TopLevelUserInteractionRequired) {
  SetBlockThirdPartyCookies(true);

  // The test fixture pre-seeds kHostB with top-level user interaction, but not
  // the other hosts. We intentionally use kHostA as the embed, since it has not
  // been seeded with a top-level user interaction.

  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(EchoCookiesURL(kHostA));

  ASSERT_EQ(ReadCookiesAndContent(GetFrame(), kHostA), kNoCookiesWithContent);

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostA), kNoCookies);

  // If kHostA has a top-level interaction, it can request storage access. The
  // user interaction should be tracked by site, not origin.
  EnsureUserInteractionOn(kHostASubdomain);
  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(EchoCookiesURL(kHostA));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostA), CookieBundle("cross-site=a.test"));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest,
                       IncognitoDoesntUseRegularInteractionsOrPermission) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Even though there was previous interaction in the regular profile, requests
  // made by incognito profiles should be denied, due to the top-level user
  // interaction requirement.
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      /*user_gesture=*/true));

  NavigateToURLWithDisposition(incognito_browser,
                               https_server().GetURL(kHostA, "/iframe.html"),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);
  NavigateFrameTo(EchoCookiesURL(kHostB), incognito_browser);

  EXPECT_FALSE(content::ExecJs(GetFrame(incognito_browser),
                               "document.requestStorageAccess()"));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIBrowserTest, IncognitoCanUseAPI) {
  Browser* incognito_browser = Browser::Create(Browser::CreateParams(
      browser()->profile()->GetPrimaryOTRProfile(/*create_if_needed=*/true),
      /*user_gesture=*/true));

  NavigateToURLWithDisposition(incognito_browser,
                               https_server().GetURL(kHostA, "/empty.html"),
                               WindowOpenDisposition::NEW_FOREGROUND_TAB,
                               ui_test_utils::BROWSER_TEST_WAIT_FOR_LOAD_STOP);

  // Even in an incognito profile, it's possible to use the Storage Access API
  // if all the requirements are satisfied.

  EnsureUserInteractionOn(kHostB, incognito_browser);

  NavigateToPageWithFrame(kHostA, incognito_browser);
  NavigateFrameTo(EchoCookiesURL(kHostB), incognito_browser);

  std::unique_ptr<permissions::MockPermissionPromptFactory>
      incognito_prompt_factory = MakePromptFactory(incognito_browser);
  incognito_prompt_factory->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      GetFrame(incognito_browser)));
}

class StorageAccessAPIWithImplicitGrantsBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  StorageAccessAPIWithImplicitGrantsBrowserTest() {
    StorageAccessGrantPermissionContext::SetImplicitGrantLimitForTesting(2);
  }

 protected:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {};
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWithImplicitGrantsBrowserTest,
                       ImplicitGrantsAllowAccess) {
  base::HistogramTester histogram_tester;
  SetBlockThirdPartyCookies(true);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  // Access should be allowed by the first implicit grant.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  histogram_tester.ExpectUniqueSample(kGrantIsImplicitHistogram,
                                      /*sample=*/true, 1);

  NavigateToPageWithFrame(kHostC);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  // Access should be allowed by the second implicit grant.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  histogram_tester.ExpectUniqueSample(kGrantIsImplicitHistogram,
                                      /*sample=*/true, 2);

  NavigateToPageWithFrame(kHostD);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  // Access is denied since kHostB has already exhausted both of its implicit
  // grants, and we've set the prompt_factory to always deny.
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  histogram_tester.ExpectBucketCount(
      kRequestOutcomeHistogram, /*sample=*/RequestOutcome::kDeniedByUser, 1);

  NavigateToPageWithFrame(kHostB);
  NavigateFrameTo(EchoCookiesURL(kHostA));
  // Other embeds can still obtain access via their own implicit grants.
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  histogram_tester.ExpectUniqueSample(kGrantIsImplicitHistogram,
                                      /*sample=*/true, 3);
}

// Tests to verify that when 3p cookie is allowed, the embedded iframe can
// access cookie without requesting, and no prompt is shown if the iframe makes
// the request.
class StorageAccessAPIWith3PCEnabledBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {content_settings::features::kTrackingProtection3pcd};
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWith3PCEnabledBrowserTest,
                       AllowedWhenUnblocked) {
  SetBlockThirdPartyCookies(false);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB),
            CookieBundleWithContent("cross-site=b.test"));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DISMISS);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIWith3PCEnabledBrowserTest,
                       AllowedByUserBypass) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);

  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  // Enable UserBypass on hostA as top-level.
  CookieSettingsFactory::GetForProfile(browser()->profile())
      ->SetCookieSettingForUserBypass(GetURL(kHostA));

  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB),
            CookieBundleWithContent("cross-site=b.test"));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DISMISS);
  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(0, prompt_factory()->TotalRequestCount());
}

// Validate that if third-party cookies are allowed but the permission is
// denied, requestStorageAccess beyond cookies succeeds.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIWith3PCEnabledBrowserTest,
                       BeyondCookies_WithCookiesWithoutPermission) {
  SetBlockThirdPartyCookies(false);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::DENY_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessBeyondCookiesForFrame(
      GetFrame()));
}

// Validate that if third-party cookies are allowed and the permission is
// allowed, requestStorageAccess beyond cookies succeeds.
IN_PROC_BROWSER_TEST_F(StorageAccessAPIWith3PCEnabledBrowserTest,
                       BeyondCookies_WithCookiesWithPermission) {
  SetBlockThirdPartyCookies(false);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessBeyondCookiesForFrame(
      GetFrame()));
}

class StorageAccessAPIAutograntsWithFedCMBrowserTest
    : public StorageAccessAPIBaseBrowserTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {{blink::features::kFedCmWithStorageAccessAPI, {}}};
  }

  void GrantFedCMPermission() {
    const url::Origin rp_embedder =
        url::Origin::Create(GetURL(kHostASubdomain));
    const url::Origin rp_requester = url::Origin::Create(GetURL(kHostC));
    const url::Origin idp = url::Origin::Create(GetURL(kHostB));
    constexpr char account_id[] = "my account";

    FederatedIdentityPermissionContextFactory::GetForProfile(
        browser()->profile())
        ->GrantSharingPermission(rp_requester, rp_embedder, idp, account_id);
  }

  void NavigateToPageWithPermissionsPolicyIframes(
      std::initializer_list<const std::string_view> hosts_list) {
    base::span hosts(hosts_list);
    ASSERT_GT(hosts.size(), 0U);
    ASSERT_TRUE(ui_test_utils::NavigateToURL(
        browser(), https_server().GetURL(
                       hosts[0], base::StrCat({
                                     "/cross_site_iframe_factory.html?",
                                     hosts[0],
                                     "(",
                                     MakeNonRootFrameNodes(hosts.subspan(1)),
                                     ")",
                                 }))));
  }

 private:
  std::string MakeNonRootFrameNodes(base::span<const std::string_view> hosts) {
    std::string tree;
    for (const auto& host : hosts) {
      base::StrAppend(&tree, {host, "{allow-identity-credentials-get}("});
    }
    for (const auto& host : hosts) {
      (void)host;
      base::StrAppend(&tree, {")"});
    }
    return tree;
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrants_RequiresPermissionPolicy) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrants_PreventSilentAccess) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(content::ExecJs(GetPrimaryMainFrame(),
                              "navigator.credentials.preventSilentAccess()"));

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrants_PreventSilentAccess_AfterAutogrant) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      GetFrame(), /*omit_user_gesture=*/true));
  EXPECT_TRUE(content::ExecJs(GetPrimaryMainFrame(),
                              "navigator.credentials.preventSilentAccess()"));

  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrants_PermissionPolicyHeaderIgnored) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::AutoResponseType::DENY_ALL);

  constexpr char kPageWithPermissionPolicyHeader[] = "/page_with_header.html";
  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [&](content::URLLoaderInterceptor::RequestParams* params) {
        if (params->url_request.url.path() != kPageWithPermissionPolicyHeader) {
          return false;
        }

        CHECK_EQ(params->url_request.url.host_piece(), kHostB);
        content::URLLoaderInterceptor::WriteResponse(
            "HTTP/1.1 200 OK\n"
            "Content-type: text/html\n"
            "Permissions-Policy: identity-credentials-get=(self "
            "\"https://b.com\")\n"
            "\n",
            /*body=*/"", params->client.get());
        return true;
      }));

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(
      GURL(base::StrCat({"https://", kHostB, kPageWithPermissionPolicyHeader})),
      browser());
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_FALSE(content::ExecJs(GetFrame(), "document.requestStorageAccess()"));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 1);
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrantsAllowCookieAccessViaSAA) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      GetFrame(), /*omit_user_gesture=*/true));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 0);
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  EXPECT_EQ(ReadCookiesAndContent(GetFrame(), kHostB), kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessAPIAutograntsWithFedCMBrowserTest,
                       FedCMGrantsAllowCookieAccess_NestedFrame) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostC, kHostB});
  EXPECT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetNestedFrame(), EchoCookiesURL(kHostB)));
  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));

  EXPECT_TRUE(
      storage::test::RequestAndCheckStorageAccessForFrame(GetNestedFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 0);
  EXPECT_EQ(ReadCookies(GetNestedFrame(), kHostB),
            CookieBundle("cross-site=b.test"));

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostC, kHostB});
  EXPECT_TRUE(content::NavigateToURLFromRendererWithoutUserGesture(
      GetNestedFrame(), EchoCookiesURL(kHostB)));
  EXPECT_EQ(ReadCookiesAndContent(GetNestedFrame(), kHostB),
            kNoCookiesWithContent);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetNestedFrame()));
}

class StorageAccessAPIAutograntsWithFedCMOriginTrialBrowserTest
    : public StorageAccessAPIAutograntsWithFedCMBrowserTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    // We intentionally do not enable the kFedCmWithStorageAccessAPI feature,
    // since overriding its state means we'd ignore the origin trial token.
    return {};
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    // See
    // https://chromium.googlesource.com/chromium/src/+/HEAD/docs/origin_trials_integration.md#manual-testing.
    command_line->AppendSwitchASCII(
        "origin-trial-public-key",
        "dRCs+TocuKkocNKa0AtZ4awrt9XKH2SQCI6o4FY6BNA=");
  }

 protected:
  bool OnRequest(content::URLLoaderInterceptor::RequestParams* params) {
    if (params->url_request.url.path() != kPageWithOriginTrialHeader) {
      return false;
    }

    CHECK_EQ(params->url_request.url.host_piece(), kHostB);
    // Origin Trials key generated with:
    //
    // tools/origin_trials/generate_token.py --expire-timestamp 2000000000
    // https://b.test FedCmWithStorageAccessAPI
    content::URLLoaderInterceptor::WriteResponse(
        "HTTP/1.1 200 OK\n"
        "Content-type: text/html\n\n",

        /*body=*/
        "<meta http-equiv='origin-trial' "
        "content='A4qD0M27fNpFkAe8cZ74fkY2Vfo6a+h9ZUbyG1E/nTooswOEp0LE/"
        "uhVUCx6nH68NoK7GoYsmgw+"
        "yigPZmay2ggAAABeeyJvcmlnaW4iOiAiaHR0cHM6Ly9iLnRlc3Q6NDQzIiwgImZlYXR1"
        "cmUiOiAiRmVkQ21XaXRoU3RvcmFnZUFjY2Vzc0FQSSIsICJleHBpcnkiOiAyMDAwMDAw"
        "MDAwfQ=='>",
        params->client.get());
    return true;
  }

  GURL OriginTrialPage() const {
    return GURL(base::StrCat({"https://", kHostB, kPageWithOriginTrialHeader}));
  }

 private:
  static constexpr char kPageWithOriginTrialHeader[] = "/page_with_token.html";
};

IN_PROC_BROWSER_TEST_F(
    StorageAccessAPIAutograntsWithFedCMOriginTrialBrowserTest,
    FedCMGrantsAllowCookieAccessViaSAA) {
  SetBlockThirdPartyCookies(true);
  GrantFedCMPermission();

  content::URLLoaderInterceptor interceptor(base::BindLambdaForTesting(
      [this](content::URLLoaderInterceptor::RequestParams* params) {
        return OnRequest(params);
      }));

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(OriginTrialPage(), browser(),
                  /*iframe_id=*/"child-0");
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));

  EXPECT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
  EXPECT_EQ(prompt_factory()->TotalRequestCount(), 0);
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), CookieBundle("cross-site=b.test"));

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(OriginTrialPage(), browser(),
                  /*iframe_id=*/"child-0");
  EXPECT_EQ(ReadCookies(GetFrame(), kHostB), kNoCookies);
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

class StorageAccessHeadersDisabledBrowserTest
    : public StorageAccessAPIBrowserTest {
 public:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    return {
        {network::features::kStorageAccessHeaders},
    };
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersDisabledBrowserTest, RetryHeader) {
  SetBlockThirdPartyCookies(true);

  // Pre-seed with a <A, B> permission grant.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Now attempt to use that permission grant for a B subresource fetched by an
  // A document, without invoking the Storage Access API. The feature is
  // disabled, so the request header is not sent and neither are cookies.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, kHeaderNotProvidedSentinel),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess,
               kHeaderNotProvidedSentinel))));
  EXPECT_EQ(retry_path_fetch_count_, 1);
}

class StorageAccessHeadersBrowserTest : public StorageAccessAPIBrowserTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    return {
        {network::features::kStorageAccessHeaders, {}},
    };
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest, RetryHeader) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostA);

  // Pre-seed with a <A, B> permission grant.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Now attempt to use that permission grant for a B subresource fetched by an
  // A document, without invoking the Storage Access API.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=b.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));
  EXPECT_EQ(retry_path_fetch_count_, 2);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest, RetryHeader_Wildcard) {
  SetBlockThirdPartyCookies(true);
  set_retry_allowed_origin("*");

  // Pre-seed with a <A, B> permission grant.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Now attempt to use that permission grant for a B subresource fetched by an
  // A document, without invoking the Storage Access API.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=b.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));
  EXPECT_EQ(retry_path_fetch_count_, 2);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RetryHeader_NoopWithoutGrant) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostA);

  // Note: we do *not* pre-seed with a <A, B> permission grant.

  // Now attempt to use the `retry` header for a B subresource fetched by an A
  // document, without invoking the Storage Access API.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, kHeaderNotProvidedSentinel),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "none"))));
  EXPECT_EQ(retry_path_fetch_count_, 1);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RetryHeader_ABAContext) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostB);

  // Attempt to get Storage Access for an A subresource fetched by a B document
  // (embedded under an A top-level document), without invoking the Storage
  // Access API. There's no privacy boundary here (since the top-level site and
  // the subresource fetch are for the same site); there's just a security
  // boundary, which the header opt-in disables.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB, "/empty.html"));
  EXPECT_THAT(
      ContentFromFetch(GetFrame(), kHostA, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=a.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));
  EXPECT_EQ(retry_path_fetch_count_, 2);
}

// Regression test for https://crbug.com/352722603. Same as
// `RetryHeader_ABAContext`, except that the iframe calls
// `document.requestStorageAccess()` before issuing the fetch.
IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RetryHeader_ABAContext_WithIrrelevantApiCall) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostB);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB, "/empty.html"));

  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  EXPECT_THAT(
      ContentFromFetch(GetFrame(), kHostA, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=a.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));
  EXPECT_EQ(retry_path_fetch_count_, 2);
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest, LoadHeader) {
  SetBlockThirdPartyCookies(true);

  // Pre-seed with a <A, B> permission grant.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(EchoCookiesURL(kHostB));
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Now attempt to use that permission grant for a cross-site iframe, without
  // invoking the Storage Access API.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB, "/set-header?Activate-Storage-Access: load"));
  // No need to request storage access, because we already have it.
  EXPECT_TRUE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       LoadHeader_NoopWithoutGrant) {
  SetBlockThirdPartyCookies(true);

  // Note: we do *not* pre-seed with a <A, B> permission grant.

  // Now attempt to get storage access in a cross-site iframe, without invoking
  // the Storage Access API.
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB, "/set-header?Activate-Storage-Access: load"));
  // Permission was never granted, so we don't have storage access.
  EXPECT_FALSE(storage::test::HasStorageAccessForFrame(GetFrame()));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RequestHeadersFirstParty) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostA));
  EXPECT_THAT(MostRecentRequestHeaders(),
              testing::Not(Contains(testing::Key(
                  net::HttpRequestHeaders::kSecFetchStorageAccess))));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RequestHeadersCredentialsBlocked) {
  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB));
  ASSERT_TRUE(content::ExecJs(
      GetFrame(), content::JsReplace("fetch($1, {'credentials': 'omit'})",
                                     GetURL(kHostB))));
  EXPECT_THAT(MostRecentRequestHeaders(),
              testing::Not(Contains(testing::Key(
                  net::HttpRequestHeaders::kSecFetchStorageAccess))));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest, RequestHeadersNone) {
  SetBlockThirdPartyCookies(true);

  NavigateToPageWithFrame(kHostA);
  NavigateFrameTo(GetURL(kHostB));
  EXPECT_THAT(
      MostRecentRequestHeaders(),
      Contains(Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "none")));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RequestHeadersInactive) {
  SetBlockThirdPartyCookies(true);
  EnsureUserInteractionOn(kHostB);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  // Header will be 'none' first time we navigate to `kHostB` since the
  // permission grant does not exist yet.
  NavigateFrameTo(GetURL(kHostB));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Top-level subresource fetches also include the "inactive" header.
  EXPECT_EQ(CookiesFromFetch(GetPrimaryMainFrame(), kHostB), "None");
  EXPECT_THAT(MostRecentRequestHeaders(),
              Contains(Pair(net::HttpRequestHeaders::kSecFetchStorageAccess,
                            "inactive")));

  // Subsequent navigation should be `inactive`.
  NavigateFrameTo(GetURL(kHostB));

  EXPECT_THAT(MostRecentRequestHeaders(),
              Contains(Pair(net::HttpRequestHeaders::kSecFetchStorageAccess,
                            "inactive")));
}

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersBrowserTest,
                       RequestHeaderRetryToActive) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostA);
  EnsureUserInteractionOn(kHostB);
  prompt_factory()->set_response_type(
      permissions::PermissionRequestManager::ACCEPT_ALL);

  NavigateToPageWithFrame(kHostA);
  // Header will be 'none' first time we navigate to `kHostB` since the
  // permission grant does not exist yet.
  NavigateFrameTo(GetURL(kHostB));
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(GetFrame()));

  // Top-level subresource fetches can benefit from the permission.
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=b.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));

  // Next navigation would be inactive, but with the `kRetryPath` we end up
  // opting into `storage-access`, making it `active`.
  NavigateFrameTo(GetURL(kHostB, kRetryPath));
  EXPECT_THAT(MostRecentRequestHeaders(),
              Contains(Pair(net::HttpRequestHeaders::kSecFetchStorageAccess,
                            "active")));
}

class StorageAccessHeadersWithThirdPartyCookiesBrowserTest
    : public StorageAccessHeadersBrowserTest {
 public:
  std::vector<base::test::FeatureRef> GetDisabledFeatures() override {
    std::vector<base::test::FeatureRef> features =
        StorageAccessHeadersBrowserTest::GetDisabledFeatures();
    features.push_back(content_settings::features::kTrackingProtection3pcd);
    return features;
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersWithThirdPartyCookiesBrowserTest,
                       RetryHeader_NoopWhenCookiesAllowed) {
  SetBlockThirdPartyCookies(false);
  SetRetryAllowedOriginFromHost(kHostA);

  // Note: we do *not* pre-seed with a <A, B> permission grant.

  // Now attempt to use the `retry` header for a B subresource fetched by an A
  // document, without invoking the Storage Access API.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, "cross-site=b.test"),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "active"))));
  EXPECT_EQ(retry_path_fetch_count_, 1);
}

class StorageAccessHeadersWithFedCMBrowserTest
    : public StorageAccessAPIAutograntsWithFedCMBrowserTest {
 public:
  std::vector<base::test::FeatureRefAndParams> GetEnabledFeatures() override {
    std::vector<base::test::FeatureRefAndParams> features =
        StorageAccessAPIAutograntsWithFedCMBrowserTest::GetEnabledFeatures();
    features.push_back({network::features::kStorageAccessHeaders, {}});
    return features;
  }
};

IN_PROC_BROWSER_TEST_F(StorageAccessHeadersWithFedCMBrowserTest, RetryHeader) {
  SetBlockThirdPartyCookies(true);
  SetRetryAllowedOriginFromHost(kHostA);
  GrantFedCMPermission();

  NavigateToPageWithPermissionsPolicyIframes({kHostA, kHostB});
  NavigateFrameTo(EchoCookiesURL(kHostB), browser(), /*iframe_id=*/"child-0");
  ASSERT_TRUE(storage::test::RequestAndCheckStorageAccessForFrame(
      GetFrame(), /*omit_user_gesture=*/true));

  // Attempt to use the FedCM permission grant for a B subresource fetched by an
  // A document, without invoking the Storage Access API. Since there was no
  // `identity-credentials-get` permissions policy associated with this
  // particular opt-in, the FedCM grant should not be usable via the header.
  NavigateToPage(kHostA, "/empty.html");
  EXPECT_THAT(
      ContentFromFetch(GetPrimaryMainFrame(), kHostB, kRetryPath),
      HeadersAre(UnorderedElementsAre(
          Pair(net::HttpRequestHeaders::kCookie, kHeaderNotProvidedSentinel),
          Pair(net::HttpRequestHeaders::kSecFetchStorageAccess, "none"))));
  EXPECT_EQ(retry_path_fetch_count_, 1);
}

// TODO(): Add test cases of 3PC enabled by other mechanisms.

}  // namespace
