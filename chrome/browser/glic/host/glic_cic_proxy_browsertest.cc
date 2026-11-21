// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <atomic>
#include <memory>
#include <string>
#include <string_view>
#include <vector>

#include "base/command_line.h"
#include "base/functional/bind.h"
#include "base/json/json_writer.h"
#include "base/strings/escape.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/string_util.h"
#include "base/test/gmock_expected_support.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/values.h"
#include "chrome/browser/glic/host/glic_cic_proxy_loader.h"
#include "chrome/browser/glic/host/guest_source.h"
#include "chrome/browser/glic/host/guest_util.h"
#include "chrome/browser/glic/host/host.h"
#include "chrome/browser/glic/public/features.h"
#include "chrome/browser/glic/service/glic_instance_impl.h"
#include "chrome/browser/glic/test_support/glic_browser_test.h"
#include "chrome/common/chrome_switches.h"
#include "components/tabs/public/tab_interface.h"
#include "content/public/browser/navigation_controller.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/storage_partition.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/referrer.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_devtools_protocol_client.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/net_errors.h"
#include "net/base/url_util.h"
#include "net/cookies/canonical_cookie.h"
#include "net/cookies/cookie_options.h"
#include "net/cookies/cookie_partition_key_collection.h"
#include "net/dns/mock_host_resolver.h"
#include "net/http/http_status_code.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "services/network/public/mojom/cookie_manager.mojom.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/page_transition_types.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

constexpr char kGuestHostname[] = "gemini.test";
constexpr char kCorpLoginHostname[] = "login.corp.google.com";

constexpr char kCicClientHtml[] = R"html(
<!doctype html>
<html>
<head>
<script id="glic-api-script" async src="chrome__glic_api.js"></script>
</head>
<body>
<div id="source-marker">from-cic-server</div>
<script type="module" src="/glic/browser_tests/minimal_client.js"></script>
</body>
</html>
)html";

constexpr char kGuestClientHtml[] = R"html(
<!doctype html>
<html>
<head>
<script id="glic-api-script" async src="chrome__glic_api.js"></script>
</head>
<body>
<div id="source-marker">from-guest-server</div>
<script type="module" src="/glic/browser_tests/minimal_client.js"></script>
</body>
</html>
)html";

std::string GetAllCookiesForWebContentsUrl(content::WebContents* web_contents,
                                           const GURL& url) {
  base::test::TestFuture<const net::CookieAccessResultList&,
                         const net::CookieAccessResultList&>
      future;
  web_contents->GetPrimaryMainFrame()
      ->GetStoragePartition()
      ->GetCookieManagerForBrowserProcess()
      ->GetCookieList(url, net::CookieOptions::MakeAllInclusive(),
                      net::CookiePartitionKeyCollection::ContainsAll(),
                      future.GetCallback());
  return net::CanonicalCookie::BuildCookieLine(future.Get<0>());
}

class GlicCicProxyBrowserTestBase : public GlicBrowserTest {
 public:
  enum class CicGlicSsoMode {
    kDirect,
    kSilentBounce,
    kInteractiveExpired,
  };

  GlicCicProxyBrowserTestBase(
      std::vector<base::test::FeatureRef> enabled_features,
      std::vector<base::test::FeatureRef> disabled_features)
      : enabled_features_(std::move(enabled_features)),
        disabled_features_(std::move(disabled_features)) {
    GetGlicHttpsTestServer()->SetCertHostnames(
        {kGuestHostname, kCorpLoginHostname, "127.0.0.1"});
    SetGlicPagePath("/glic");
  }

  void SetUp() override {
    ASSERT_TRUE(cic_server_.InitializeAndListen());
    std::vector<base::test::FeatureRefAndParams> enabled_with_params;
    for (const auto& feature : enabled_features_) {
      if (&(*feature) == &features::kGlicCic) {
        enabled_with_params.emplace_back(
            features::kGlicCic,
            base::FieldTrialParams{{"url", cic_server_.base_url().spec()}});
      } else {
        enabled_with_params.emplace_back(*feature, base::FieldTrialParams{});
      }
    }
    feature_list_.InitWithFeaturesAndParameters(enabled_with_params,
                                                disabled_features_);
    GlicBrowserTest::SetUp();
  }

  void SetUpCommandLine(base::CommandLine* command_line) override {
    GlicBrowserTest::SetUpCommandLine(command_line);
    command_line->RemoveSwitch(::switches::kGlicGuestURL);
    command_line->AppendSwitchASCII(
        ::switches::kGlicGuestURL,
        GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/glic").spec());
    command_line->AppendSwitch(::switches::kGlicSkipReloadAfterNavigation);
  }

  void SetUpOnMainThread() override {
    GetGlicHttpsTestServer()->RegisterRequestHandler(base::BindRepeating(
        &GlicCicProxyBrowserTestBase::HandleGuestServerRequest,
        base::Unretained(this)));
    cic_server_.RegisterRequestHandler(base::BindRepeating(
        &GlicCicProxyBrowserTestBase::HandleCicServerRequest,
        base::Unretained(this)));
    cic_server_.StartAcceptingConnections();
    GlicBrowserTest::SetUpOnMainThread();
    host_resolver()->AddRule(kGuestHostname, "127.0.0.1");
    host_resolver()->AddRule(kCorpLoginHostname, "127.0.0.1");
  }

  void TearDownOnMainThread() override {
    EXPECT_TRUE(cic_server_.ShutdownAndWaitUntilComplete());
    GlicBrowserTest::TearDownOnMainThread();
  }

  void set_cic_glic_sso_mode(CicGlicSsoMode mode) {
    cic_glic_sso_mode_.store(mode, std::memory_order_relaxed);
  }

  int cic_total_hits() const {
    return cic_total_hits_.load(std::memory_order_relaxed);
  }

  void ResetCicTotalHits() {
    cic_total_hits_.store(0, std::memory_order_relaxed);
  }

  int cic_healthz_hits() const {
    return cic_healthz_hits_.load(std::memory_order_relaxed);
  }

  int cic_bundle_hits() const {
    return cic_bundle_hits_.load(std::memory_order_relaxed);
  }

  int guest_glic_hits() const {
    return guest_glic_hits_.load(std::memory_order_relaxed);
  }

  int guest_healthz_hits() const {
    return guest_healthz_hits_.load(std::memory_order_relaxed);
  }

  int guest_bundle_hits() const {
    return guest_bundle_hits_.load(std::memory_order_relaxed);
  }

  const GURL& cic_base_url() const { return cic_server_.base_url(); }

 private:
  std::unique_ptr<net::test_server::HttpResponse> HandleGuestServerRequest(
      const net::test_server::HttpRequest& request) {
    const GURL url = request.GetURL();
    if (url.path() == "/silent_sso_bounce") {
      std::string next_path;
      net::GetValueForKeyInQuery(url, "next", &next_path);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location",
                                cic_server_.GetURL(next_path).spec());
      return response;
    }
    if (url.path() == "/interactive_sso_login") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html; charset=utf-8");
      response->set_content(
          "<!doctype html><html><body>Corp Login Form</body></html>");
      return response;
    }
    if (base::EndsWith(url.path(), kGlicApiBundleResourcePathSuffix,
                       base::CompareCase::SENSITIVE)) {
      guest_bundle_hits_.fetch_add(1, std::memory_order_relaxed);
      return nullptr;
    }
    if (url.path() == "/glic" || url.path() == "/glic/" ||
        url.path() == "/u/0/glic") {
      guest_glic_hits_.fetch_add(1, std::memory_order_relaxed);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html; charset=utf-8");
      response->set_content(kGuestClientHtml);
      return response;
    }
    if (url.path() == "/empty_guest_origin.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html; charset=utf-8");
      response->set_content("<!doctype html><html><body>Empty</body></html>");
      return response;
    }
    if (url.path() == "/cic/api/healthz" ||
        url.path() == "/u/0/cic/api/healthz") {
      guest_healthz_hits_.fetch_add(1, std::memory_order_relaxed);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(R"({"status":"ok","source":"guest"})");
      return response;
    }
    return nullptr;
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleCicServerRequest(
      const net::test_server::HttpRequest& request) {
    cic_total_hits_.fetch_add(1, std::memory_order_relaxed);
    const GURL url = request.GetURL();
    const std::string_view path = url.path();

    if (base::EndsWith(path, kGlicApiBundleResourcePathSuffix,
                       base::CompareCase::SENSITIVE)) {
      cic_bundle_hits_.fetch_add(1, std::memory_order_relaxed);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/javascript");
      response->set_content("// unexpected cic bundle");
      return response;
    }

    if (path == "/glic") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      const CicGlicSsoMode sso_mode =
          cic_glic_sso_mode_.load(std::memory_order_relaxed);
      std::string sso_minted;
      const bool has_minted =
          net::GetValueForKeyInQuery(url, "sso_minted", &sso_minted) &&
          sso_minted == "1";
      if (sso_mode == CicGlicSsoMode::kSilentBounce && !has_minted) {
        const std::string next = std::string(path) + "?sso_minted=1";
        response->set_code(net::HTTP_FOUND);
        response->AddCustomHeader(
            "Location",
            GetGlicHttpsTestServer()
                ->GetURL(kCorpLoginHostname,
                         "/silent_sso_bounce?next=" +
                             base::EscapeQueryParamValue(next, true))
                .spec());
        return response;
      }
      if (sso_mode == CicGlicSsoMode::kInteractiveExpired) {
        response->set_code(net::HTTP_FOUND);
        response->AddCustomHeader(
            "Location",
            GetGlicHttpsTestServer()
                ->GetURL(kCorpLoginHostname, "/interactive_sso_login")
                .spec());
        return response;
      }
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html; charset=utf-8");
      response->AddCustomHeader("Cache-Control", "no-cache");
      response->AddCustomHeader("ETag", "\"cic-glic-v1\"");
      response->AddCustomHeader("Set-Cookie", "cic_secret=1; Path=/");
      response->AddCustomHeader("Set-Cookie",
                                "cic_httponly=1; Path=/; HttpOnly");
      response->AddCustomHeader("Content-Security-Policy",
                                "default-src 'none'");
      response->AddCustomHeader("X-Frame-Options", "DENY");
      response->AddCustomHeader("Cross-Origin-Opener-Policy", "same-origin");
      response->AddCustomHeader("Alt-Svc", "h3=\":443\"");
      response->set_content(kCicClientHtml);
      return response;
    }

    if (path == "/cic/sandbox.html") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/html; charset=utf-8");
      response->set_content(
          "<!doctype html><html><body>"
          "<div id=\"sandbox-marker\">from-cic-sandbox</div>"
          "</body></html>");
      return response;
    }

    if (path == "/cic/test_script.js") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("text/javascript; charset=utf-8");
      response->AddCustomHeader("Cache-Control", "no-store");
      response->set_content("window.__cicScriptLoaded = true;");
      return response;
    }

    if (path == "/cic/api/healthz") {
      cic_healthz_hits_.fetch_add(1, std::memory_order_relaxed);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->AddCustomHeader("Cache-Control", "no-store");
      response->AddCustomHeader("ETag", "\"healthz-v1\"");
      response->AddCustomHeader("Set-Cookie", "cic_api_cookie=1; Path=/");
      response->AddCustomHeader("Set-Cookie",
                                "cic_api_httponly=1; Path=/; HttpOnly");
      response->AddCustomHeader("Content-Security-Policy",
                                "default-src 'none'");
      response->AddCustomHeader("X-Frame-Options", "DENY");
      response->AddCustomHeader("Alt-Svc", "h3=\":443\"");
      response->set_content(R"({"status":"ok","source":"cic"})");
      return response;
    }

    if (path == "/cic/api/silent_sso") {
      std::string sso_minted;
      const bool has_minted =
          net::GetValueForKeyInQuery(url, "sso_minted", &sso_minted) &&
          sso_minted == "1";
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      if (!has_minted) {
        response->set_code(net::HTTP_FOUND);
        response->AddCustomHeader(
            "Location",
            GetGlicHttpsTestServer()
                ->GetURL(kCorpLoginHostname,
                         "/silent_sso_bounce?next=" +
                             base::EscapeQueryParamValue(
                                 "/cic/api/silent_sso?sso_minted=1", true))
                .spec());
        return response;
      }
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(R"({"status":"ok","sso":"minted"})");
      return response;
    }

    if (path == "/cic/api/echo") {
      base::DictValue payload;
      payload.Set("method", request.method_string);
      payload.Set("body", request.content);
      auto find_header = [&](const char* name) -> std::string {
        auto it = request.headers.find(name);
        return it != request.headers.end() ? it->second : std::string();
      };
      payload.Set("accept", find_header("Accept"));
      payload.Set("contentType", find_header("Content-Type"));
      payload.Set("ifNoneMatch", find_header("If-None-Match"));
      payload.Set("hasCustomHeader",
                  request.headers.contains("X-Disallowed-Header"));
      std::string json;
      base::JSONWriter::Write(payload, &json);

      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_OK);
      response->set_content_type("application/json");
      response->set_content(json);
      return response;
    }

    if (path == "/cic/api/auth_expired") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader(
          "Location", GetGlicHttpsTestServer()
                          ->GetURL(kCorpLoginHostname, "/interactive_sso_login")
                          .spec());
      return response;
    }

    if (path == "/cic/api/redirect_evil") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", "https://evil.example.com/phish");
      return response;
    }

    if (path == "/cic/api/redirect_http_login") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", "http://login.corp.google.com/");
      return response;
    }

    if (path == "/cic/api/redirect_same_origin") {
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", "/cic/api/healthz");
      return response;
    }

    if (base::StartsWith(path, "/cic/api/redirect_loop/")) {
      int hop = 0;
      base::StringToInt(path.substr(strlen("/cic/api/redirect_loop/")), &hop);
      auto response = std::make_unique<net::test_server::BasicHttpResponse>();
      response->set_code(net::HTTP_FOUND);
      response->AddCustomHeader("Location", "/cic/api/redirect_loop/" +
                                                base::NumberToString(hop + 1));
      return response;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_NOT_FOUND);
    return response;
  }

  std::vector<base::test::FeatureRef> enabled_features_;
  std::vector<base::test::FeatureRef> disabled_features_;
  base::test::ScopedFeatureList feature_list_;
  net::test_server::EmbeddedTestServer cic_server_{
      net::test_server::EmbeddedTestServer::TYPE_HTTP};
  std::atomic<CicGlicSsoMode> cic_glic_sso_mode_{CicGlicSsoMode::kDirect};
  std::atomic<int> cic_total_hits_{0};
  std::atomic<int> cic_healthz_hits_{0};
  std::atomic<int> cic_bundle_hits_{0};
  std::atomic<int> guest_glic_hits_{0};
  std::atomic<int> guest_healthz_hits_{0};
  std::atomic<int> guest_bundle_hits_{0};
};

class GlicCicProxyBrowserTest : public GlicCicProxyBrowserTestBase {
 public:
  GlicCicProxyBrowserTest()
      : GlicCicProxyBrowserTestBase(
            /*enabled_features=*/{features::kGlicCic},
            /*disabled_features=*/{features::kGlicNoWebview,
                                   features::kGlicApiBundleResource}) {}
};

class GlicCicProxyDisabledBrowserTest : public GlicCicProxyBrowserTestBase {
 public:
  GlicCicProxyDisabledBrowserTest()
      : GlicCicProxyBrowserTestBase(
            /*enabled_features=*/{features::kGlicNoWebview},
            /*disabled_features=*/{features::kGlicCic}) {}
};

// 1. `/glic` navigation is served from the configured CiC test server while
// Blink's committed origin remains the Glic guest origin, and `Set-Cookie`
// headers from the CiC server never leak into the guest origin's cookies.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       ProxiesGlicNavigationPreservingGuestOrigin) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  const url::Origin expected_guest_origin = GetGuestOrigin(GetProfile());
  EXPECT_EQ(expected_guest_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_NE(url::Origin::Create(cic_base_url()),
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(
      "from-cic-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));
  EXPECT_EQ(guest_glic_hits(), 0);
  EXPECT_GE(cic_total_hits(), 1);

  // Neither non-HttpOnly nor HttpOnly cookies from `cic_server_` may leak into
  // the guest origin.
  EXPECT_EQ("", content::EvalJs(guest_contents, "document.cookie"));
  const std::string guest_cookies = GetAllCookiesForWebContentsUrl(
      guest_contents, expected_guest_origin.GetURL());
  EXPECT_EQ(guest_cookies.find("cic_secret"), std::string::npos);
  EXPECT_EQ(guest_cookies.find("cic_httponly"), std::string::npos);
}

// 2. `/glic?glic_passthrough=1` bypasses the CiC proxy and reaches the real
// guest origin.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       PassthroughQueryBypassesCicProxy) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  EXPECT_EQ(
      "from-cic-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));
  EXPECT_EQ(guest_glic_hits(), 0);

  const GURL passthrough_url = GetGlicHttpsTestServer()->GetURL(
      kGuestHostname, "/glic?glic_passthrough=1");
  ASSERT_TRUE(content::NavigateToURL(guest_contents, passthrough_url));

  EXPECT_EQ(GetGuestOrigin(GetProfile()),
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(
      "from-guest-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));
  EXPECT_EQ(guest_glic_hits(), 1);
}

// 3. `/cic/api/healthz` and `/cic/api/echo` subresource requests proxy to the
// CiC server with allowlisted headers & body (without leaking cookies);
// `/chrome__glic_api.js` is still served from memory and NOT proxied to CiC.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       ProxiesCicSubresourcesAndPreservesGlicApiBundle) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  // Verify allowlisted response headers pass through while CSP / framing /
  // Alt-Svc / Set-Cookie headers are stripped.
  EXPECT_EQ(base::Value(base::DictValue()
                            .Set("body", R"({"status":"ok","source":"cic"})")
                            .Set("contentType", "application/json")
                            .Set("cacheControl", "no-store")
                            .Set("etag", "\"healthz-v1\"")
                            .Set("csp", "")
                            .Set("xfo", "")
                            .Set("altSvc", "")),
            content::EvalJs(guest_contents, R"js(
        (async () => {
          const resp = await fetch('/cic/api/healthz');
          return {
            body: await resp.text(),
            contentType: resp.headers.get('content-type') ?? '',
            cacheControl: resp.headers.get('cache-control') ?? '',
            etag: resp.headers.get('etag') ?? '',
            csp: resp.headers.get('content-security-policy') ?? '',
            xfo: resp.headers.get('x-frame-options') ?? '',
            altSvc: resp.headers.get('alt-svc') ?? '',
          };
        })()
      )js"));

  // Multi-login `/u/0/cic/*` subresources are also proxied.
  EXPECT_EQ(
      R"({"status":"ok","source":"cic"})",
      content::EvalJs(guest_contents,
                      "fetch('/u/0/cic/api/healthz').then(r => r.text())"));

  // `Set-Cookie` on `/cic/api/healthz` must not leak to the guest origin.
  EXPECT_EQ("", content::EvalJs(guest_contents, "document.cookie"));
  const std::string guest_cookies = GetAllCookiesForWebContentsUrl(
      guest_contents, GetGuestOrigin(GetProfile()).GetURL());
  EXPECT_EQ(guest_cookies.find("cic_api_cookie"), std::string::npos);
  EXPECT_EQ(guest_cookies.find("cic_api_httponly"), std::string::npos);

  EXPECT_EQ(base::Value(base::DictValue()
                            .Set("method", "POST")
                            .Set("body", R"({"hello":"cic"})")
                            .Set("accept", "application/json")
                            .Set("contentType", "application/json")
                            .Set("ifNoneMatch", "\"etag-123\"")
                            .Set("hasCustomHeader", false)),
            content::EvalJs(guest_contents, R"js(
        fetch('/cic/api/echo', {
          method: 'POST',
          headers: {
            'Accept': 'application/json',
            'Content-Type': 'application/json',
            'If-None-Match': '"etag-123"',
            'X-Disallowed-Header': 'should-not-forward',
          },
          body: JSON.stringify({hello: 'cic'}),
        }).then(r => r.json())
      )js"));

  const std::string expected_bundle = GetGuestAPISource();
  EXPECT_EQ(
      expected_bundle,
      content::EvalJs(guest_contents,
                      "fetch('/chrome__glic_api.js').then(r => r.text())"));
  EXPECT_EQ(cic_bundle_hits(), 0);
  EXPECT_EQ(guest_bundle_hits(), 0);
}

// Non-Glic browser tabs are never proxied. Inside the Glic guest primary page,
// multi-login `/u/0/glic` main-frame navigations and `/cic/` subframe
// navigations/subresources are proxied, while subframe `/glic` navigations stay
// on the guest server.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       ScopesProxyingToGlicGuestPrimaryPage) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  ResetCicTotalHits();

  // 1. A normal browser tab on the exact same guest origin must not be proxied.
  const GURL normal_tab_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/glic");
  tabs::TabInterface* normal_tab = CreateAndActivateTab(normal_tab_url);
  ASSERT_TRUE(normal_tab);
  EXPECT_EQ(
      "from-guest-server",
      content::EvalJs(normal_tab->GetContents(),
                      "document.getElementById('source-marker').textContent"));
  EXPECT_EQ(R"({"status":"ok","source":"guest"})",
            content::EvalJs(normal_tab->GetContents(),
                            "fetch('/cic/api/healthz').then(r => r.text())"));
  EXPECT_EQ(guest_glic_hits(), 1);
  EXPECT_EQ(guest_healthz_hits(), 1);
  EXPECT_EQ(cic_healthz_hits(), 0);

  // 2. Multi-login `/u/0/glic` navigation on the primary main frame is proxied.
  const GURL multi_login_glic_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/u/0/glic");
  ASSERT_TRUE(content::NavigateToURL(guest_contents, multi_login_glic_url));
  EXPECT_EQ(
      "from-cic-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));

  // 3. A child iframe inside the Glic guest navigating to `/glic` is NOT
  // proxied, whereas navigating to `/cic/sandbox.html` and fetching `/cic/*`
  // from a child iframe ARE proxied.
  const GURL glic_iframe_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/glic");
  ASSERT_TRUE(
      content::ExecJs(guest_contents, content::JsReplace(R"js(
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.onload = () => resolve(true);
        document.body.appendChild(f);
      })
    )js",
                                                         glic_iframe_url)));
  content::RenderFrameHost* glic_child =
      content::ChildFrameAt(guest_contents->GetPrimaryMainFrame(), 0);
  ASSERT_TRUE(glic_child);
  EXPECT_EQ(
      "from-guest-server",
      content::EvalJs(glic_child,
                      "document.getElementById('source-marker').textContent"));

  const GURL sandbox_iframe_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/cic/sandbox.html");
  ASSERT_TRUE(
      content::ExecJs(guest_contents, content::JsReplace(R"js(
      new Promise(resolve => {
        const f = document.createElement('iframe');
        f.src = $1;
        f.onload = () => resolve(true);
        document.body.appendChild(f);
      })
    )js",
                                                         sandbox_iframe_url)));
  content::RenderFrameHost* sandbox_child =
      content::ChildFrameAt(guest_contents->GetPrimaryMainFrame(), 1);
  ASSERT_TRUE(sandbox_child);
  EXPECT_EQ(
      "from-cic-sandbox",
      content::EvalJs(sandbox_child,
                      "document.getElementById('sandbox-marker').textContent"));
  EXPECT_EQ(R"({"status":"ok","source":"cic"})",
            content::EvalJs(sandbox_child,
                            "fetch('/cic/api/healthz').then(r => r.text())"));
  EXPECT_EQ(cic_healthz_hits(), 1);
  EXPECT_EQ(guest_healthz_hits(), 1);
}

// 4. Silent UberProxy SSO bounces through `https://login.corp.google.com/...`
// back to the CiC server succeed transparently. When the corp login host
// returns an interactive login page instead of bouncing back, `/cic/*` returns
// `401 Unauthorized` JSON `CORP_SSO_EXPIRED` (and handles `HEAD` cleanly), and
// `/glic` returns the script-free `200 OK` recovery link using `cic_base_url()`
// without leaving the guest origin.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       HandlesCorpLoginRedirectsWithoutLeavingGuestOrigin) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);
  const url::Origin expected_guest_origin = GetGuestOrigin(GetProfile());
  const std::string expected_cic_url = cic_base_url().spec();

  // Silent UberProxy SSO bounce on subresource succeeds transparently.
  EXPECT_EQ(
      R"({"status":"ok","sso":"minted"})",
      content::EvalJs(guest_contents,
                      "fetch('/cic/api/silent_sso').then(r => r.text())"));

  // Silent UberProxy SSO bounce on main-frame navigation succeeds transparently
  // while preserving the guest origin.
  set_cic_glic_sso_mode(CicGlicSsoMode::kSilentBounce);
  const GURL glic_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/glic");
  ASSERT_TRUE(content::NavigateToURL(guest_contents, glic_url));
  EXPECT_EQ(expected_guest_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(
      "from-cic-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));

  // Expired SSO (interactive login page on `login.corp.google.com`) on
  // subresource GET returns 401 JSON with strict headers and `cic_base_url()`.
  EXPECT_EQ(base::Value(base::DictValue()
                            .Set("status", 401)
                            .Set("error", "CORP_SSO_EXPIRED")
                            .Set("loginUrl", expected_cic_url)
                            .Set("csp",
                                 "default-src 'none'; base-uri 'none'; "
                                 "form-action 'none'; frame-ancestors 'none'")
                            .Set("referrerPolicy", "no-referrer")),
            content::EvalJs(guest_contents, R"js(
        (async () => {
          const resp = await fetch('/cic/api/auth_expired');
          const data = await resp.json();
          return {
            status: resp.status,
            error: data.error,
            loginUrl: data.loginUrl,
            csp: resp.headers.get('content-security-policy') ?? '',
            referrerPolicy: resp.headers.get('referrer-policy') ?? '',
          };
        })()
      )js"));

  // Expired SSO on subresource HEAD returns 401 with an empty body.
  EXPECT_EQ(base::Value(base::DictValue().Set("status", 401).Set("body", "")),
            content::EvalJs(guest_contents, R"js(
        (async () => {
          const resp = await fetch('/cic/api/auth_expired', {method: 'HEAD'});
          return {
            status: resp.status,
            body: await resp.text(),
          };
        })()
      )js"));

  // Expired SSO on main-frame navigation returns the script-free recovery link
  // on the guest origin referencing `cic_base_url()`.
  set_cic_glic_sso_mode(CicGlicSsoMode::kInteractiveExpired);
  ASSERT_TRUE(content::NavigateToURL(guest_contents, glic_url));

  EXPECT_EQ(expected_guest_origin,
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(base::Value(base::DictValue()
                            .Set("title", "Corp SSO Session Expired")
                            .Set("loginHref", expected_cic_url)
                            .Set("reloadText", "Reload")
                            .Set("reloadAttrHref", "")
                            .Set("hasInlineOnclick", false)),
            content::EvalJs(guest_contents, R"js(
        (() => {
          const links = Array.from(document.querySelectorAll('a'));
          const reloadLink = links.find(a => a.textContent.trim() === 'Reload');
          return {
            title: document.title,
            loginHref: links[0]?.href ?? '',
            reloadText: reloadLink?.textContent.trim() ?? '',
            reloadAttrHref: reloadLink?.getAttribute('href') ?? 'missing',
            hasInlineOnclick: document.querySelector('[onclick]') !== null,
          };
        })()
      )js"));
}

// Cross-origin non-login redirects (and insecure `http://` corp login
// redirects) fail closed, while same-origin redirects succeed up to the hop
// limit and loops fail closed.
IN_PROC_BROWSER_TEST_F(
    GlicCicProxyBrowserTest,
    RejectsCrossOriginRedirectsAndAllowsBoundedSameOriginRedirects) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ("rejected", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/redirect_evil')
      .then(() => 'succeeded')
      .catch(() => 'rejected')
  )js"));

  EXPECT_EQ("rejected", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/redirect_http_login')
      .then(() => 'succeeded')
      .catch(() => 'rejected')
  )js"));

  EXPECT_EQ(R"({"status":"ok","source":"cic"})",
            content::EvalJs(
                guest_contents,
                "fetch('/cic/api/redirect_same_origin').then(r => r.text())"));

  EXPECT_EQ("rejected", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/redirect_loop/0')
      .then(() => 'succeeded')
      .catch(() => 'rejected')
  )js"));
}

// 5. CDP `Network.emulateNetworkConditions` applies end-to-end across the CiC
// proxy boundary for both main-frame navigations (`/glic`) and subresources
// (`/cic/*` fetch, script, POST upload) without leaking the upstream CiC origin
// or corp SSO URLs via DevTools.
IN_PROC_BROWSER_TEST_F(GlicCicProxyBrowserTest,
                       HonorsDevToolsNetworkConditionsEmulation) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  content::TestDevToolsProtocolClient devtools_client;
  devtools_client.AttachToWebContents(guest_contents);
  devtools_client.SendCommandSync("Network.enable");
  devtools_client.SendCommandSync("Network.setCacheDisabled",
                                  base::DictValue().Set("cacheDisabled", true));

  auto set_conditions = [&](bool offline, double latency_ms,
                            double download_bps, double upload_bps) {
    devtools_client.SendCommandSync("Network.emulateNetworkConditions",
                                    base::DictValue()
                                        .Set("offline", offline)
                                        .Set("latency", latency_ms)
                                        .Set("downloadThroughput", download_bps)
                                        .Set("uploadThroughput", upload_bps));
    // Flush the guest StoragePartition's NetworkContext Mojo pipe so the
    // network service updates `network::ThrottlingController` before proxied
    // requests issue on the default StoragePartition's URLLoaderFactory pipe.
    guest_contents->GetPrimaryMainFrame()
        ->GetStoragePartition()
        ->FlushNetworkInterfaceForTesting();
  };

  // Phase 1: Emulate offline -> proxied subresources (GET fetch, <script>, POST
  // upload) and main-frame `/glic` navigation must fail with
  // `ERR_INTERNET_DISCONNECTED` without reaching `cic_server_`.
  set_conditions(/*offline=*/true, /*latency_ms=*/0, /*download_bps=*/-1,
                 /*upload_bps=*/-1);
  devtools_client.ClearNotifications();
  const int hits_before_offline = cic_total_hits();

  EXPECT_EQ("rejected", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/healthz')
      .then(() => 'succeeded')
      .catch(() => 'rejected')
  )js"));
  devtools_client.WaitForMatchingNotification(
      "Network.loadingFailed",
      base::BindRepeating([](const base::DictValue& params) {
        const std::string* error_text = params.FindString("errorText");
        return error_text && *error_text == "net::ERR_INTERNET_DISCONNECTED";
      }));

  EXPECT_EQ("error", content::EvalJs(guest_contents, R"js(
    new Promise(resolve => {
      const s = document.createElement('script');
      s.src = '/cic/test_script.js';
      s.onload = () => resolve('loaded');
      s.onerror = () => resolve('error');
      document.head.appendChild(s);
    })
  )js"));

  EXPECT_EQ("rejected", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/echo', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({hello: 'offline'}),
    })
      .then(() => 'succeeded')
      .catch(() => 'rejected')
  )js"));

  const GURL glic_url =
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/glic");
  {
    content::TestNavigationObserver nav_observer(guest_contents);
    guest_contents->GetController().LoadURL(glic_url, content::Referrer(),
                                            ui::PAGE_TRANSITION_TYPED,
                                            std::string());
    nav_observer.Wait();
    EXPECT_FALSE(nav_observer.last_navigation_succeeded());
    EXPECT_EQ(nav_observer.last_net_error_code(),
              net::ERR_INTERNET_DISCONNECTED);
  }
  EXPECT_EQ(cic_total_hits(), hits_before_offline);

  // Phase 2: Switch back online with active latency + download/upload
  // throttling. Verify proxied navigation, GET/POST fetches, and <script> all
  // succeed cleanly through `ThrottlingNetworkTransaction` /
  // `ThrottlingUploadDataStream`.
  set_conditions(/*offline=*/false, /*latency_ms=*/1,
                 /*download_bps=*/1000000, /*upload_bps=*/1000000);
  devtools_client.ClearNotifications();

  ASSERT_TRUE(content::NavigateToURL(guest_contents, glic_url));
  EXPECT_EQ(GetGuestOrigin(GetProfile()),
            guest_contents->GetPrimaryMainFrame()->GetLastCommittedOrigin());
  EXPECT_EQ(
      "from-cic-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));

  EXPECT_EQ(R"({"status":"ok","source":"cic"})",
            content::EvalJs(guest_contents,
                            "fetch('/cic/api/healthz').then(r => r.text())"));

  EXPECT_EQ(true, content::EvalJs(guest_contents, R"js(
    new Promise((resolve, reject) => {
      window.__cicScriptLoaded = false;
      const s = document.createElement('script');
      s.src = '/cic/test_script.js';
      s.onload = () => resolve(Boolean(window.__cicScriptLoaded));
      s.onerror = () => reject(new Error('script load failed'));
      document.head.appendChild(s);
    })
  )js"));

  EXPECT_EQ("POST", content::EvalJs(guest_contents, R"js(
    fetch('/cic/api/echo', {
      method: 'POST',
      headers: {'Content-Type': 'application/json'},
      body: JSON.stringify({hello: 'throttled'}),
    }).then(r => r.json()).then(d => d.method)
  )js"));

  // Silent corp SSO bounce also works under active throttling without exposing
  // the corp login host or upstream origin to DevTools.
  EXPECT_EQ(
      R"({"status":"ok","sso":"minted"})",
      content::EvalJs(guest_contents,
                      "fetch('/cic/api/silent_sso').then(r => r.text())"));

  // Confirm the DevTools session observes the guest origin URL (not the
  // rewritten upstream `cic_base_url()` or `login.corp.google.com`) and does
  // not receive stripped `Set-Cookie` headers.
  base::DictValue healthz_event = devtools_client.WaitForMatchingNotification(
      "Network.responseReceived",
      base::BindRepeating([](const base::DictValue& params) {
        const base::DictValue* response = params.FindDict("response");
        const std::string* url =
            response ? response->FindString("url") : nullptr;
        return url && url->ends_with("/cic/api/healthz");
      }));
  const std::string* observed_url =
      healthz_event.FindStringByDottedPath("response.url");
  ASSERT_TRUE(observed_url);
  EXPECT_TRUE(observed_url->starts_with(
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/").spec()));
  EXPECT_EQ(observed_url->find(cic_base_url().spec()), std::string::npos);
  const base::DictValue* observed_headers =
      healthz_event.FindDictByDottedPath("response.headers");
  ASSERT_TRUE(observed_headers);
  EXPECT_FALSE(observed_headers->FindString("set-cookie"));
  EXPECT_FALSE(observed_headers->FindString("Set-Cookie"));

  base::DictValue sso_event = devtools_client.WaitForMatchingNotification(
      "Network.responseReceived",
      base::BindRepeating([](const base::DictValue& params) {
        const base::DictValue* response = params.FindDict("response");
        const std::string* url =
            response ? response->FindString("url") : nullptr;
        return url && url->ends_with("/cic/api/silent_sso");
      }));
  const std::string* sso_url = sso_event.FindStringByDottedPath("response.url");
  ASSERT_TRUE(sso_url);
  EXPECT_TRUE(sso_url->starts_with(
      GetGlicHttpsTestServer()->GetURL(kGuestHostname, "/").spec()));
  EXPECT_EQ(sso_url->find(kCorpLoginHostname), std::string::npos);
  EXPECT_EQ(sso_url->find(cic_base_url().spec()), std::string::npos);

  // Clean up throttling before detaching the DevTools client.
  set_conditions(/*offline=*/false, /*latency_ms=*/0, /*download_bps=*/-1,
                 /*upload_bps=*/-1);
  devtools_client.DetachProtocolClient();
}

// 6. When `kGlicCic` is disabled, requests are NOT proxied.
IN_PROC_BROWSER_TEST_F(GlicCicProxyDisabledBrowserTest,
                       DoesNotProxyWhenFeatureDisabled) {
  ASSERT_OK_AND_ASSIGN(GlicInstanceImpl * instance, OpenGlicForActiveTab());
  ASSERT_OK(WaitForGlicClient(instance));

  content::WebContents* guest_contents = instance->host().web_client_contents();
  ASSERT_TRUE(guest_contents);

  EXPECT_EQ(
      "from-guest-server",
      content::EvalJs(guest_contents,
                      "document.getElementById('source-marker').textContent"));
  EXPECT_EQ(cic_total_hits(), 0);
  EXPECT_GE(guest_glic_hits(), 1);
}

}  // namespace
}  // namespace glic
