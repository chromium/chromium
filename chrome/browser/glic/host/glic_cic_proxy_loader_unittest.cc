// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/glic/host/glic_cic_proxy_loader.h"

#include <array>
#include <cstddef>
#include <cstdint>
#include <memory>
#include <optional>
#include <string>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/bind.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/memory/self_deleting.h"
#include "base/run_loop.h"
#include "base/strings/string_number_conversions.h"
#include "base/test/run_until.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/task_environment.h"
#include "base/unguessable_token.h"
#include "chrome/browser/glic/public/features.h"
#include "content/public/browser/content_browser_client.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/receiver.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/base/isolation_info.h"
#include "net/base/load_flags.h"
#include "net/base/net_errors.h"
#include "net/base/request_priority.h"
#include "net/cookies/site_for_cookies.h"
#include "net/http/http_request_headers.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/resource_request_body.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_completion_status.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/cookie_access_observer.mojom.h"
#include "services/network/public/mojom/devtools_observer.mojom.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_client.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace glic {
namespace {

using FactoryType = content::ContentBrowserClient::URLLoaderFactoryType;

constexpr char kProdOriginSpec[] = "https://glic-cic.apps.corp.google.com/";

TEST(GlicCicProxyLoaderTest, InvalidCicUrlParamFallsBackToProdOrigin) {
  const GURL prod_origin(kProdOriginSpec);

  for (const char* invalid_url : {
           "javascript:alert(1)",
           "http://example.com",
           "https://user:pass@glic-cic.apps.corp.google.com",
           "ftp://127.0.0.1:8080",
       }) {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(features::kGlicCic,
                                                    {{"url", invalid_url}});
    EXPECT_EQ(prod_origin, GetGlicCicBaseUrl()) << "url=" << invalid_url;
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicCic,
        {{"url", "http://127.0.0.1:8765/ignored/path?foo=bar"}});
    EXPECT_EQ(GURL("http://127.0.0.1:8765/"), GetGlicCicBaseUrl());
  }

  {
    base::test::ScopedFeatureList feature_list;
    feature_list.InitAndEnableFeatureWithParameters(
        features::kGlicCic,
        {{"url", "https://custom-cic.example.com:8443/prefix"}});
    EXPECT_EQ(GURL("https://custom-cic.example.com:8443/"),
              GetGlicCicBaseUrl());
  }
}

TEST(GlicCicProxyLoaderTest, RewritesMainFrameGlicNavigationPaths) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("https://glic-cic.apps.corp.google.com/");

  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic?hl=en"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic/?hl=en#frag"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "HEAD", std::nullopt));

  // Multi-login `/u/<N>/glic` prefix is stripped.
  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic?hl=ja"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/u/0/glic?hl=ja"), guest_origin,
                "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // Custom `--glic-guest-url` document paths normalize to `/glic`.
  EXPECT_EQ(GURL("https://glic-cic.apps.corp.google.com/glic"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/custom/entry"), guest_origin,
                "/custom/entry", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // `glic_passthrough=1` bypasses the proxy.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic?glic_passthrough=1"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));

  // Non-GET/HEAD navigations are not proxied.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/true, "POST", std::nullopt));

  // Subframe `/glic` navigations are not proxied.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/glic"), guest_origin, "/glic",
                cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/false, "GET", std::nullopt));
}

TEST(GlicCicProxyLoaderTest, RewritesCicPathsAndStripsMultiLoginPrefix) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("http://127.0.0.1:8765/");

  // Primary and multi-login `/cic/` subresources.
  EXPECT_EQ(
      GURL("http://127.0.0.1:8765/cic/api/healthz?v=1"),
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/cic/api/healthz?v=1#ref"),
          guest_origin, "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/true, "GET", guest_origin));
  EXPECT_EQ(
      GURL("http://127.0.0.1:8765/cic/api/healthz"),
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/u/0/cic/api/healthz"), guest_origin,
          "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/false, "POST", guest_origin));

  // Subframe `/cic/` navigation (e.g. sandbox iframe).
  EXPECT_EQ(GURL("http://127.0.0.1:8765/cic/sandbox.html"),
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/u/2/cic/sandbox.html"),
                guest_origin, "/glic", cic_base, FactoryType::kNavigation,
                /*is_primary_main_frame=*/false, "GET", std::nullopt));

  // Non-same-origin or missing initiator is rejected for subresources.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET", std::nullopt));
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://gemini.google.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET",
                url::Origin::Create(GURL("https://evil.example.com"))));

  // Cross-origin request URL is rejected.
  EXPECT_EQ(std::nullopt,
            RewriteGlicCicTargetUrl(
                GURL("https://other.example.com/cic/api/healthz"), guest_origin,
                "/glic", cic_base, FactoryType::kDocumentSubResource,
                /*is_primary_main_frame=*/true, "GET", guest_origin));

  // Non-`/cic/` subresources stay on the guest origin.
  EXPECT_EQ(
      std::nullopt,
      RewriteGlicCicTargetUrl(
          GURL("https://gemini.google.com/chrome__glic_api.js"), guest_origin,
          "/glic", cic_base, FactoryType::kDocumentSubResource,
          /*is_primary_main_frame=*/true, "GET", guest_origin));
}

TEST(GlicCicProxyLoaderTest, RejectsPathTraversal) {
  const url::Origin guest_origin =
      url::Origin::Create(GURL("https://gemini.google.com"));
  const GURL cic_base("https://glic-cic.apps.corp.google.com/");

  for (const char* bad_url : {
           "https://gemini.google.com/cic/%2e%2e/secret",
           "https://gemini.google.com/cic/%2E%2Fsecret",
           "https://gemini.google.com/cic/a%5cb",
           "https://gemini.google.com/cic/a%00b",
           "https://gemini.google.com/u/0/cic/%2e%2e/secret",
       }) {
    EXPECT_EQ(std::nullopt,
              RewriteGlicCicTargetUrl(
                  GURL(bad_url), guest_origin, "/glic", cic_base,
                  FactoryType::kDocumentSubResource,
                  /*is_primary_main_frame=*/true, "GET", guest_origin))
        << "url=" << bad_url;
  }
}

class GlicCicProxyURLLoaderRequestTest : public testing::Test {
 protected:
  static constexpr char kGuestOriginSpec[] = "https://gemini.google.com";
  static constexpr char kCicBaseSpec[] =
      "https://glic-cic.apps.corp.google.com/";

  struct ProxyFixture {
    mojo::Remote<network::mojom::URLLoaderFactory> remote;
    std::unique_ptr<mojo::Receiver<network::mojom::URLLoaderFactory>>
        target_receiver;
    // Owned by self-deleting `GlicCicProxyingURLLoaderFactory`; cleared before
    // waiting for self-deletion in `TearDownProxy`.
    raw_ptr<GlicCicProxyingURLLoaderFactory> raw_factory = nullptr;
  };

  ProxyFixture CreateProxyFactory(FactoryType type,
                                  bool is_primary_main_frame,
                                  bool attach_upstream_override = true) {
    ProxyFixture fixture;
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_remote;
    fixture.target_receiver =
        std::make_unique<mojo::Receiver<network::mojom::URLLoaderFactory>>(
            &target_factory_, target_remote.InitWithNewPipeAndPassReceiver());

    GlicCicProxyingURLLoaderFactory* factory =
        base::MakeSelfDeleting<GlicCicProxyingURLLoaderFactory>(
            fixture.remote.BindNewPipeAndPassReceiver(),
            std::move(target_remote), /*profile=*/nullptr, type,
            is_primary_main_frame, url::Origin::Create(GURL(kGuestOriginSpec)),
            "/glic", GURL(kCicBaseSpec));
    if (attach_upstream_override) {
      factory->SetUpstreamFactoryForTesting(
          upstream_factory_.GetSafeWeakWrapper());
    }
    fixture.raw_factory = factory;
    return fixture;
  }

  void TearDownProxy(ProxyFixture& fixture) {
    base::RunLoop disconnect_loop;
    fixture.target_receiver->set_disconnect_handler(
        disconnect_loop.QuitClosure());
    fixture.raw_factory = nullptr;
    fixture.remote.reset();
    disconnect_loop.Run();
  }

  void FinishAndDestroyPendingUpstream(const GURL& url,
                                       network::TestURLLoaderClient& client) {
    ASSERT_TRUE(upstream_factory_.SimulateResponseForPendingRequest(
        url, network::URLLoaderCompletionStatus(net::ERR_ABORTED),
        network::mojom::URLResponseHead::New(), /*content=*/""));
    client.RunUntilComplete();
  }

  network::ResourceRequest MakeRequest(
      const std::string& url_spec,
      const std::string& method = net::HttpRequestHeaders::kGetMethod,
      std::optional<url::Origin> initiator = std::nullopt) {
    network::ResourceRequest request;
    request.url = GURL(url_spec);
    request.method = method;
    request.request_initiator = std::move(initiator);
    return request;
  }

  base::test::TaskEnvironment task_environment_;
  network::TestURLLoaderFactory target_factory_;
  network::TestURLLoaderFactory upstream_factory_;
};

TEST_F(GlicCicProxyURLLoaderRequestTest,
       ForwardsThrottlingProfileIdOnNavigation) {
  const url::Origin guest_origin = url::Origin::Create(GURL(kGuestOriginSpec));
  const url::Origin cic_origin = url::Origin::Create(GURL(kCicBaseSpec));
  const base::UnguessableToken throttling_token =
      base::UnguessableToken::Create();

  ProxyFixture proxy = CreateProxyFactory(FactoryType::kNavigation,
                                          /*is_primary_main_frame=*/true);

  network::ResourceRequest request =
      MakeRequest("https://gemini.google.com/u/0/glic?hl=en");
  request.throttling_profile_id = throttling_token;
  request.devtools_request_id = "devtools-nav-1";
  request.devtools_stack_id = "stack-1";
  request.request_initiator = guest_origin;
  request.referrer = GURL("https://gemini.google.com/secret?token=abc");
  request.priority = net::HIGHEST;
  request.priority_incremental = true;
  request.enable_load_timing = true;
  request.enable_upload_progress = true;
  request.load_flags = net::LOAD_BYPASS_CACHE | net::LOAD_DISABLE_CACHE |
                       net::LOAD_VALIDATE_CACHE | net::LOAD_PREFETCH |
                       net::LOAD_BYPASS_PROXY | net::LOAD_ONLY_FROM_CACHE;
  request.headers.SetHeader(net::HttpRequestHeaders::kAccept, "text/html");
  request.headers.SetHeader("Cookie", "gemini_secret=1");
  request.cors_exempt_headers.SetHeader("X-Glic", "1");
  request.request_body = network::ResourceRequestBody::CreateFromCopyOfBytes(
      base::byte_span_from_cstring("ignored-nav-body"));

  request.trusted_params = network::ResourceRequest::TrustedParams();
  mojo::PendingReceiver<network::mojom::DevToolsObserver> devtools_receiver =
      request.trusted_params->devtools_observer
          .InitWithNewPipeAndPassReceiver();
  mojo::PendingReceiver<network::mojom::CookieAccessObserver> cookie_receiver =
      request.trusted_params->cookie_observer.InitWithNewPipeAndPassReceiver();

  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  proxy.raw_factory->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return upstream_factory_.NumPending() == 1; }));

  EXPECT_EQ(target_factory_.NumPending(), 0);
  ASSERT_EQ(upstream_factory_.NumPending(), 1);
  const network::ResourceRequest& upstream =
      upstream_factory_.GetPendingRequest(0)->request;

  EXPECT_EQ(upstream.url,
            GURL("https://glic-cic.apps.corp.google.com/glic?hl=en"));
  EXPECT_EQ(upstream.throttling_profile_id, throttling_token);

  // Security & isolation checks: do not forward DevTools/cookie observers or
  // cross-origin request IDs/metadata to upstream requests.
  EXPECT_EQ(upstream.devtools_request_id, std::nullopt);
  EXPECT_EQ(upstream.devtools_stack_id, std::nullopt);
  EXPECT_EQ(upstream.request_initiator, std::nullopt);
  EXPECT_EQ(upstream.referrer, GURL());
  EXPECT_TRUE(upstream.cors_exempt_headers.IsEmpty());
  EXPECT_EQ(upstream.request_body, nullptr);
  EXPECT_EQ(upstream.headers.GetHeader(net::HttpRequestHeaders::kAccept),
            "text/html");
  EXPECT_FALSE(upstream.headers.HasHeader("Cookie"));
  ASSERT_TRUE(upstream.trusted_params.has_value());
  EXPECT_FALSE(upstream.trusted_params->devtools_observer.is_valid());
  EXPECT_FALSE(upstream.trusted_params->cookie_observer.is_valid());
  EXPECT_EQ(upstream.trusted_params->isolation_info.top_frame_origin(),
            cic_origin);
  EXPECT_EQ(upstream.trusted_params->isolation_info.frame_origin(), cic_origin);
  EXPECT_TRUE(upstream.site_for_cookies.IsEquivalent(
      net::SiteForCookies::FromOrigin(cic_origin)));
  EXPECT_TRUE(upstream.update_first_party_url_on_redirect);

  // Priority, timing, and allowlisted cache load flags are preserved while
  // disallowed load flags are masked off.
  EXPECT_EQ(upstream.priority, net::HIGHEST);
  EXPECT_TRUE(upstream.priority_incremental);
  EXPECT_TRUE(upstream.enable_load_timing);
  EXPECT_TRUE(upstream.enable_upload_progress);
  EXPECT_EQ(upstream.load_flags, net::LOAD_BYPASS_CACHE |
                                     net::LOAD_DISABLE_CACHE |
                                     net::LOAD_VALIDATE_CACHE);

  FinishAndDestroyPendingUpstream(
      GURL("https://glic-cic.apps.corp.google.com/glic?hl=en"), client);
  loader.reset();
  TearDownProxy(proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest,
       ForwardsThrottlingProfileIdOnSubframeCicNavigation) {
  const base::UnguessableToken throttling_token =
      base::UnguessableToken::Create();
  ProxyFixture proxy = CreateProxyFactory(FactoryType::kNavigation,
                                          /*is_primary_main_frame=*/false);

  network::ResourceRequest request =
      MakeRequest("https://gemini.google.com/u/1/cic/sandbox.html");
  request.throttling_profile_id = throttling_token;
  request.devtools_request_id = "devtools-subframe-1";

  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  proxy.remote->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return upstream_factory_.NumPending() == 1; }));

  EXPECT_EQ(target_factory_.NumPending(), 0);
  ASSERT_EQ(upstream_factory_.NumPending(), 1);
  const network::ResourceRequest& upstream =
      upstream_factory_.GetPendingRequest(0)->request;
  EXPECT_EQ(upstream.url,
            GURL("https://glic-cic.apps.corp.google.com/cic/sandbox.html"));
  EXPECT_EQ(upstream.throttling_profile_id, throttling_token);
  EXPECT_EQ(upstream.devtools_request_id, std::nullopt);

  FinishAndDestroyPendingUpstream(
      GURL("https://glic-cic.apps.corp.google.com/cic/sandbox.html"), client);
  loader.reset();
  TearDownProxy(proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest,
       ForwardsThrottlingProfileIdOnSubresources) {
  const url::Origin guest_origin = url::Origin::Create(GURL(kGuestOriginSpec));
  const base::UnguessableToken throttling_token =
      base::UnguessableToken::Create();

  ProxyFixture proxy = CreateProxyFactory(FactoryType::kDocumentSubResource,
                                          /*is_primary_main_frame=*/true);

  struct Case {
    const char* path;
    const char* method;
    const char* expected_upstream;
    bool has_body;
  };
  constexpr auto kCases = std::to_array<Case>({
      {"/cic/app.js", "GET", "https://glic-cic.apps.corp.google.com/cic/app.js",
       false},
      {"/cic/assets/font.woff2", "GET",
       "https://glic-cic.apps.corp.google.com/cic/assets/font.woff2", false},
      {"/u/0/cic/api/healthz", "GET",
       "https://glic-cic.apps.corp.google.com/cic/api/healthz", false},
      {"/cic/api/echo", "POST",
       "https://glic-cic.apps.corp.google.com/cic/api/echo", true},
  });

  std::vector<mojo::Remote<network::mojom::URLLoader>> loaders;
  std::vector<std::unique_ptr<network::TestURLLoaderClient>> clients;

  for (size_t i = 0; i < kCases.size(); ++i) {
    const Case& c = kCases[i];
    network::ResourceRequest request = MakeRequest(
        std::string(kGuestOriginSpec) + c.path, c.method, guest_origin);
    request.throttling_profile_id = throttling_token;
    request.devtools_request_id = "devtools-sub-" + base::NumberToString(i);
    request.devtools_stack_id = "stack-sub";
    request.headers.SetHeader(net::HttpRequestHeaders::kAccept,
                              "application/json");
    request.headers.SetHeader(net::HttpRequestHeaders::kContentType,
                              "application/json");
    request.headers.SetHeader(net::HttpRequestHeaders::kIfNoneMatch, "\"v1\"");
    request.headers.SetHeader("Authorization", "Bearer secret");
    if (c.has_body) {
      request.request_body =
          network::ResourceRequestBody::CreateFromCopyOfBytes(
              base::byte_span_from_cstring(R"({"ping":1})"));
    }

    loaders.emplace_back();
    clients.push_back(std::make_unique<network::TestURLLoaderClient>());
    proxy.remote->CreateLoaderAndStart(
        loaders.back().BindNewPipeAndPassReceiver(),
        /*request_id=*/static_cast<int32_t>(i + 1),
        network::mojom::kURLLoadOptionNone, request,
        clients.back()->CreateRemote(),
        net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  }
  ASSERT_TRUE(base::test::RunUntil([&]() {
    return upstream_factory_.NumPending() == static_cast<int>(kCases.size());
  }));

  EXPECT_EQ(target_factory_.NumPending(), 0);
  ASSERT_EQ(upstream_factory_.NumPending(), static_cast<int>(kCases.size()));
  for (size_t i = 0; i < kCases.size(); ++i) {
    const Case& c = kCases[i];
    const network::ResourceRequest& upstream =
        upstream_factory_.GetPendingRequest(i)->request;
    EXPECT_EQ(upstream.url, GURL(c.expected_upstream));
    EXPECT_EQ(upstream.method, c.method);
    EXPECT_EQ(upstream.throttling_profile_id, throttling_token);
    EXPECT_EQ(upstream.devtools_request_id, std::nullopt);
    EXPECT_EQ(upstream.devtools_stack_id, std::nullopt);
    EXPECT_EQ(upstream.headers.GetHeader(net::HttpRequestHeaders::kAccept),
              "application/json");
    EXPECT_EQ(upstream.headers.GetHeader(net::HttpRequestHeaders::kContentType),
              "application/json");
    EXPECT_EQ(upstream.headers.GetHeader(net::HttpRequestHeaders::kIfNoneMatch),
              "\"v1\"");
    EXPECT_FALSE(upstream.headers.HasHeader("Authorization"));
    EXPECT_EQ(upstream.request_body != nullptr, c.has_body);
  }

  for (size_t i = 0; i < kCases.size(); ++i) {
    FinishAndDestroyPendingUpstream(GURL(kCases[i].expected_upstream),
                                    *clients[i]);
  }
  loaders.clear();
  TearDownProxy(proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest, OmitsMissingOrEmptyThrottlingToken) {
  ProxyFixture proxy = CreateProxyFactory(FactoryType::kNavigation,
                                          /*is_primary_main_frame=*/true);

  // Missing (`std::nullopt`) token remains unset.
  mojo::Remote<network::mojom::URLLoader> loader_none;
  network::TestURLLoaderClient client_none;
  network::ResourceRequest req_none =
      MakeRequest("https://gemini.google.com/glic");
  ASSERT_EQ(req_none.throttling_profile_id, std::nullopt);
  proxy.raw_factory->CreateLoaderAndStart(
      loader_none.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, req_none, client_none.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  // Explicit empty `base::UnguessableToken{}` is dropped so it is never
  // serialized over Mojo.
  mojo::Remote<network::mojom::URLLoader> loader_empty;
  network::TestURLLoaderClient client_empty;
  network::ResourceRequest req_empty =
      MakeRequest("https://gemini.google.com/glic");
  req_empty.throttling_profile_id = base::UnguessableToken();
  ASSERT_TRUE(req_empty.throttling_profile_id->is_empty());
  proxy.raw_factory->CreateLoaderAndStart(
      loader_empty.BindNewPipeAndPassReceiver(), /*request_id=*/2,
      network::mojom::kURLLoadOptionNone, req_empty,
      client_empty.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return upstream_factory_.NumPending() == 2; }));
  EXPECT_EQ(
      upstream_factory_.GetPendingRequest(0)->request.throttling_profile_id,
      std::nullopt);
  EXPECT_EQ(
      upstream_factory_.GetPendingRequest(1)->request.throttling_profile_id,
      std::nullopt);

  const GURL expected_glic_url("https://glic-cic.apps.corp.google.com/glic");
  ASSERT_TRUE(upstream_factory_.SimulateResponseForPendingRequest(
      expected_glic_url, network::URLLoaderCompletionStatus(net::ERR_ABORTED),
      network::mojom::URLResponseHead::New(), /*content=*/""));
  ASSERT_TRUE(upstream_factory_.SimulateResponseForPendingRequest(
      expected_glic_url, network::URLLoaderCompletionStatus(net::ERR_ABORTED),
      network::mojom::URLResponseHead::New(), /*content=*/""));
  client_none.RunUntilComplete();
  client_empty.RunUntilComplete();
  loader_none.reset();
  loader_empty.reset();
  TearDownProxy(proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest,
       NonProxiedRequestsPassThroughWithOriginalFields) {
  const url::Origin guest_origin = url::Origin::Create(GURL(kGuestOriginSpec));
  const base::UnguessableToken throttling_token =
      base::UnguessableToken::Create();

  ProxyFixture nav_proxy = CreateProxyFactory(FactoryType::kNavigation,
                                              /*is_primary_main_frame=*/true);
  network::ResourceRequest passthrough_nav =
      MakeRequest("https://gemini.google.com/glic?glic_passthrough=1");
  passthrough_nav.throttling_profile_id = throttling_token;
  passthrough_nav.devtools_request_id = "devtools-passthrough-nav";

  mojo::Remote<network::mojom::URLLoader> nav_loader;
  network::TestURLLoaderClient nav_client;
  nav_proxy.remote->CreateLoaderAndStart(
      nav_loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, passthrough_nav,
      nav_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  ProxyFixture sub_proxy = CreateProxyFactory(FactoryType::kDocumentSubResource,
                                              /*is_primary_main_frame=*/true);
  network::ResourceRequest passthrough_sub =
      MakeRequest("https://gemini.google.com/chrome__glic_api.js",
                  net::HttpRequestHeaders::kGetMethod, guest_origin);
  passthrough_sub.throttling_profile_id = throttling_token;
  passthrough_sub.devtools_request_id = "devtools-passthrough-sub";

  mojo::Remote<network::mojom::URLLoader> sub_loader;
  network::TestURLLoaderClient sub_client;
  sub_proxy.remote->CreateLoaderAndStart(
      sub_loader.BindNewPipeAndPassReceiver(), /*request_id=*/2,
      network::mojom::kURLLoadOptionNone, passthrough_sub,
      sub_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  ASSERT_TRUE(base::test::RunUntil(
      [&]() { return target_factory_.NumPending() == 2; }));

  EXPECT_EQ(upstream_factory_.NumPending(), 0);
  ASSERT_EQ(target_factory_.NumPending(), 2);
  EXPECT_EQ(target_factory_.GetPendingRequest(0)->request.throttling_profile_id,
            throttling_token);
  EXPECT_EQ(target_factory_.GetPendingRequest(0)->request.devtools_request_id,
            "devtools-passthrough-nav");
  EXPECT_EQ(target_factory_.GetPendingRequest(1)->request.throttling_profile_id,
            throttling_token);
  EXPECT_EQ(target_factory_.GetPendingRequest(1)->request.devtools_request_id,
            "devtools-passthrough-sub");

  nav_loader.reset();
  sub_loader.reset();
  TearDownProxy(nav_proxy);
  TearDownProxy(sub_proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest,
       PropagatesOfflineErrorWithoutRecoveryResponse) {
  const url::Origin guest_origin = url::Origin::Create(GURL(kGuestOriginSpec));
  const base::UnguessableToken throttling_token =
      base::UnguessableToken::Create();

  upstream_factory_.AddResponse(
      GURL("https://glic-cic.apps.corp.google.com/glic"),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED));
  upstream_factory_.AddResponse(
      GURL("https://glic-cic.apps.corp.google.com/cic/api/healthz"),
      network::mojom::URLResponseHead::New(), /*content=*/"",
      network::URLLoaderCompletionStatus(net::ERR_INTERNET_DISCONNECTED));

  ProxyFixture nav_proxy = CreateProxyFactory(FactoryType::kNavigation,
                                              /*is_primary_main_frame=*/true);
  network::ResourceRequest nav_req =
      MakeRequest("https://gemini.google.com/glic");
  nav_req.throttling_profile_id = throttling_token;
  mojo::Remote<network::mojom::URLLoader> nav_loader;
  network::TestURLLoaderClient nav_client;
  nav_proxy.remote->CreateLoaderAndStart(
      nav_loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, nav_req, nav_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  nav_client.RunUntilComplete();
  EXPECT_FALSE(nav_client.has_received_response());
  EXPECT_EQ(nav_client.completion_status().error_code,
            net::ERR_INTERNET_DISCONNECTED);

  ProxyFixture sub_proxy = CreateProxyFactory(FactoryType::kDocumentSubResource,
                                              /*is_primary_main_frame=*/true);
  network::ResourceRequest sub_req =
      MakeRequest("https://gemini.google.com/cic/api/healthz",
                  net::HttpRequestHeaders::kGetMethod, guest_origin);
  sub_req.throttling_profile_id = throttling_token;
  mojo::Remote<network::mojom::URLLoader> sub_loader;
  network::TestURLLoaderClient sub_client;
  sub_proxy.remote->CreateLoaderAndStart(
      sub_loader.BindNewPipeAndPassReceiver(), /*request_id=*/2,
      network::mojom::kURLLoadOptionNone, sub_req, sub_client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  sub_client.RunUntilComplete();
  EXPECT_FALSE(sub_client.has_received_response());
  EXPECT_EQ(sub_client.completion_status().error_code,
            net::ERR_INTERNET_DISCONNECTED);

  TearDownProxy(nav_proxy);
  TearDownProxy(sub_proxy);
}

TEST_F(GlicCicProxyURLLoaderRequestTest, FailsWithAbortedWhenProfileIsNull) {
  ProxyFixture proxy = CreateProxyFactory(FactoryType::kNavigation,
                                          /*is_primary_main_frame=*/true,
                                          /*attach_upstream_override=*/false);
  network::ResourceRequest request =
      MakeRequest("https://gemini.google.com/glic");
  mojo::Remote<network::mojom::URLLoader> loader;
  network::TestURLLoaderClient client;
  proxy.remote->CreateLoaderAndStart(
      loader.BindNewPipeAndPassReceiver(), /*request_id=*/1,
      network::mojom::kURLLoadOptionNone, request, client.CreateRemote(),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));
  client.RunUntilComplete();
  EXPECT_EQ(client.completion_status().error_code, net::ERR_ABORTED);
  EXPECT_EQ(upstream_factory_.NumPending(), 0);
  EXPECT_EQ(target_factory_.NumPending(), 0);
  TearDownProxy(proxy);
}

}  // namespace
}  // namespace glic
