// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <map>
#include <memory>
#include <optional>
#include <string>
#include <string_view>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/lock.h"
#include "base/test/scoped_feature_list.h"
#include "base/thread_annotations.h"
#include "chrome/browser/chrome_content_browser_client.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/site_token_provider/fake_trusted_url_loader_header_client.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chrome/test/base/ui_test_utils.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_constants.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/browser/browser_context.h"
#include "content/public/browser/render_frame_host.h"
#include "content/public/browser/web_contents.h"
#include "content/public/common/content_client.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/browser_test_utils.h"
#include "content/public/test/test_navigation_observer.h"
#include "net/base/url_util.h"
#include "net/dns/mock_host_resolver.h"
#include "net/test/embedded_test_server/embedded_test_server.h"
#include "net/test/embedded_test_server/http_request.h"
#include "net/test/embedded_test_server/http_response.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_token_provider {

namespace {

constexpr char kAllowlistedHost[] = "a.com";
constexpr char kNonAllowlistedHost[] = "b.com";
constexpr char kTestToken[] = "mock-site-token-value-5678";

}  // namespace

class SiteTokenBrowserTest : public InProcessBrowserTest {
 protected:
  SiteTokenBrowserTest() {
    feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"site_token_allowlist", kAllowlistedHost}});
  }
  ~SiteTokenBrowserTest() override = default;

  void SetUpOnMainThread() override {
    InProcessBrowserTest::SetUpOnMainThread();

    host_resolver()->AddRule("*", "127.0.0.1");

    embedded_https_test_server().RegisterRequestHandler(base::BindRepeating(
        &SiteTokenBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_https_test_server().Start());

    embedded_test_server()->RegisterRequestHandler(base::BindRepeating(
        &SiteTokenBrowserTest::HandleRequest, base::Unretained(this)));
    ASSERT_TRUE(embedded_test_server()->Start());

    // Seed the token cache for the allowlisted host.
    auto* service =
        SiteTokenProviderServiceFactory::GetForProfile(GetProfile());
    ASSERT_TRUE(service);
    service->SetTokenForTesting(kAllowlistedHost, kTestToken);
  }

  GURL GetUrl(std::string_view host, std::string_view path) {
    return embedded_https_test_server().GetURL(host, path);
  }

  GURL GetCleartextUrl(std::string_view host, std::string_view path) {
    return embedded_test_server()->GetURL(host, path);
  }

  content::RenderFrameHost* NavigateTo(std::string_view host,
                                       std::string_view path) {
    return ui_test_utils::NavigateToURL(browser(), GetUrl(host, path));
  }

  void AddIframeAndWait(content::RenderFrameHost* rfh, const GURL& url) {
    content::TestNavigationObserver nav_observer(
        content::WebContents::FromRenderFrameHost(rfh));
    ASSERT_TRUE(content::ExecJs(
        rfh,
        content::JsReplace("const iframe = document.createElement('iframe');"
                           "iframe.src = $1;"
                           "document.body.appendChild(iframe);",
                           url)));
    nav_observer.Wait();
    ASSERT_TRUE(nav_observer.last_navigation_succeeded());
    ASSERT_EQ(url, nav_observer.last_navigation_url());
  }

  std::unique_ptr<net::test_server::HttpResponse> HandleRequest(
      const net::test_server::HttpRequest& request) {
    if (request.relative_url != "/match" &&
        request.relative_url != "/subresource" &&
        request.relative_url != "/embedder") {
      return nullptr;
    }

    {
      base::AutoLock auto_lock(lock_);
      request_count_++;
      headers_by_path_[request.relative_url] = request.headers;
    }

    auto response = std::make_unique<net::test_server::BasicHttpResponse>();
    response->set_code(net::HTTP_OK);
    response->set_content_type("text/html");
    response->set_content("OK");
    return response;
  }

  void ResetServerState() {
    base::AutoLock auto_lock(lock_);
    request_count_ = 0;
    headers_by_path_.clear();
  }

  [[nodiscard]] testing::AssertionResult CheckHeaderValuePresent(
      std::string_view path,
      const std::string& name,
      const std::string& value) {
    base::AutoLock auto_lock(lock_);
    auto path_it = headers_by_path_.find(std::string(path));
    if (path_it == headers_by_path_.end()) {
      return testing::AssertionFailure()
             << "No request recorded for path " << path;
    }
    const auto& headers = path_it->second;
    auto header_it = headers.find(name);
    if (header_it == headers.end()) {
      return testing::AssertionFailure()
             << "Header '" << name << "' not found for path " << path;
    }
    if (header_it->second != value) {
      return testing::AssertionFailure()
             << "Header '" << name << "' for path " << path << " has value '"
             << header_it->second << "', expected '" << value << "'";
    }
    return testing::AssertionSuccess();
  }

  [[nodiscard]] testing::AssertionResult CheckHeaderNotPresent(
      std::string_view path,
      const std::string& name) {
    base::AutoLock auto_lock(lock_);
    auto path_it = headers_by_path_.find(std::string(path));
    if (path_it == headers_by_path_.end()) {
      return testing::AssertionFailure()
             << "No request recorded for path " << path;
    }
    const auto& headers = path_it->second;
    auto header_it = headers.find(name);
    if (header_it != headers.end()) {
      return testing::AssertionFailure()
             << "Header '" << name << "' unexpectedly found for path " << path
             << " with value '" << header_it->second << "'";
    }
    return testing::AssertionSuccess();
  }

  base::test::ScopedFeatureList feature_list_;

  base::Lock lock_;
  int request_count_ GUARDED_BY(lock_) = 0;
  std::map<std::string, net::test_server::HttpRequest::HeaderMap>
      headers_by_path_ GUARDED_BY(lock_);
};

IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest, InjectsHeaderForAllowedDomain) {
  ASSERT_TRUE(NavigateTo(kAllowlistedHost, "/match"));

  EXPECT_TRUE(
      CheckHeaderValuePresent("/match", kChromeSiteTokenHeader, kTestToken));
}

IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest,
                       DoesNotInjectHeaderForOtherDomains) {
  ASSERT_TRUE(NavigateTo(kNonAllowlistedHost, "/match"));

  EXPECT_TRUE(CheckHeaderNotPresent("/match", kChromeSiteTokenHeader));
}

IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest,
                       DoesNotInjectHeaderForSubresourceRequests) {
  content::RenderFrameHost* rfh = NavigateTo(kAllowlistedHost, "/match");
  ASSERT_TRUE(rfh);

  ResetServerState();

  // Issue a subresource fetch() to the allowlisted domain.
  ASSERT_EQ("OK",
            content::EvalJs(rfh, "fetch('/subresource').then(r => r.text())"));

  EXPECT_TRUE(CheckHeaderNotPresent("/subresource", kChromeSiteTokenHeader));
}

// Injection is restricted to top-level document navigations, so an allowlisted
// subframe does not get the header even when navigating to an allowlisted host.
IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest,
                       DoesNotInjectHeaderForCrossOriginSubframe) {
  content::RenderFrameHost* rfh = NavigateTo(kNonAllowlistedHost, "/embedder");
  ASSERT_TRUE(rfh);

  ResetServerState();

  ASSERT_NO_FATAL_FAILURE(
      AddIframeAndWait(rfh, GetUrl(kAllowlistedHost, "/match")));

  EXPECT_TRUE(CheckHeaderNotPresent("/match", kChromeSiteTokenHeader));
}

// Verifies that even a same-origin subframe does not get the header injected,
// since injection is strictly limited to top-level documents.
IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest,
                       DoesNotInjectHeaderForSameOriginSubframe) {
  content::RenderFrameHost* rfh = NavigateTo(kAllowlistedHost, "/embedder");
  ASSERT_TRUE(rfh);

  ResetServerState();

  ASSERT_NO_FATAL_FAILURE(
      AddIframeAndWait(rfh, GetUrl(kAllowlistedHost, "/match")));

  EXPECT_TRUE(CheckHeaderNotPresent("/match", kChromeSiteTokenHeader));
}

// Tests that the entitlement token header is not injected over cleartext HTTP
// to a non-localhost hostname.
IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest, DoesNotInjectOverCleartext) {
  GURL url = GetCleartextUrl(kAllowlistedHost, "/match");
  ASSERT_FALSE(url.SchemeIsCryptographic());
  ASSERT_FALSE(net::IsLocalhost(url));
  ASSERT_TRUE(ui_test_utils::NavigateToURL(browser(), url));

  EXPECT_TRUE(CheckHeaderNotPresent("/match", kChromeSiteTokenHeader));
}

namespace {

// Installs `fake_header_client` as the trusted header client before
// ChromeContentBrowserClient gets a chance to wrap it, so that the SiteToken
// wrapper ends up chaining to it rather than being installed on its own.
class ChainingTestContentBrowserClient : public ChromeContentBrowserClient {
 public:
  explicit ChainingTestContentBrowserClient(
      FakeTrustedURLLoaderHeaderClient* fake_header_client)
      : fake_header_client_(fake_header_client) {}
  ~ChainingTestContentBrowserClient() override = default;

  void WillCreateURLLoaderFactory(
      content::BrowserContext* browser_context,
      content::RenderFrameHost* frame,
      int render_process_id,
      URLLoaderFactoryType type,
      const url::Origin& request_initiator,
      const net::IsolationInfo& isolation_info,
      std::optional<int64_t> navigation_id,
      ukm::SourceIdObj ukm_source_id,
      network::URLLoaderFactoryBuilder& factory_builder,
      mojo::PendingRemote<network::mojom::TrustedURLLoaderHeaderClient>*
          header_client,
      bool* bypass_redirect_checks,
      bool* disable_secure_dns,
      network::mojom::URLLoaderFactoryOverridePtr* factory_override,
      scoped_refptr<base::SequencedTaskRunner> navigation_response_task_runner,
      bool is_for_network_service) override {
    if (header_client && !header_client->is_valid()) {
      *header_client = fake_header_client_->AddReceiver();
    }
    ChromeContentBrowserClient::WillCreateURLLoaderFactory(
        browser_context, frame, render_process_id, type, request_initiator,
        isolation_info, std::move(navigation_id), ukm_source_id,
        factory_builder, header_client, bypass_redirect_checks,
        disable_secure_dns, factory_override,
        std::move(navigation_response_task_runner), is_for_network_service);
  }

 private:
  raw_ptr<FakeTrustedURLLoaderHeaderClient> fake_header_client_;
};

}  // namespace

// Another header client may already be installed, by extensions or by
// enterprise policy. Replacing it instead of chaining to it would silently
// drop their header modifications.
IN_PROC_BROWSER_TEST_F(SiteTokenBrowserTest, PreservesExistingHeaderClient) {
  FakeTrustedURLLoaderHeaderClient fake_header_client;
  ChainingTestContentBrowserClient chaining_browser_client(&fake_header_client);
  content::ContentBrowserClient* original_browser_client =
      content::SetBrowserClientForTesting(&chaining_browser_client);
  base::ScopedClosureRunner restore_browser_client(base::BindOnce(
      [](content::ContentBrowserClient* client) {
        content::SetBrowserClientForTesting(client);
      },
      original_browser_client));

  ResetServerState();
  ASSERT_TRUE(NavigateTo(kAllowlistedHost, "/match"));

  ASSERT_GT(fake_header_client.observed_request_count(), 0);
  EXPECT_TRUE(CheckHeaderValuePresent("/match", kFakeHeaderClientHeaderName,
                                      kFakeHeaderClientHeaderValue));
  EXPECT_TRUE(
      CheckHeaderValuePresent("/match", kChromeSiteTokenHeader, kTestToken));
}

}  // namespace site_token_provider
