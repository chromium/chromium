// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_url_loader_throttle.h"

#include <memory>
#include <string>
#include <utility>

#include "base/test/scoped_feature_list.h"
#include "chrome/test/base/testing_profile.h"
#include "components/contextual_tasks/public/features.h"
#include "content/public/test/browser_task_environment.h"
#include "extensions/buildflags/buildflags.h"
#include "extensions/common/extension_features.h"
#include "net/base/isolation_info.h"
#include "net/traffic_annotation/network_traffic_annotation_test_helper.h"
#include "net/url_request/redirect_info.h"
#include "services/network/public/cpp/http_request_headers_update_params.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/shared_url_loader_factory.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/public/mojom/url_response_head.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"
#include "url/origin.h"

namespace contextual_tasks {

class ContextualTasksURLLoaderThrottleTest : public testing::Test {
 public:
  ContextualTasksURLLoaderThrottleTest() { EnableRequiredFeatures(); }

  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  void EnableRequiredFeatures() {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/
        {
            kContextualTasksRearchitecture,
            extensions_features::kApiContextualTasksPrivate,
        },
        /*disabled_features=*/{});
  }

  TestingProfile* profile() { return profile_.get(); }

  static net::IsolationInfo CreateIsolationInfo(const GURL& top_frame_url) {
    url::Origin origin = url::Origin::Create(top_frame_url);
    return net::IsolationInfo::Create(net::IsolationInfo::RequestType::kOther,
                                      origin, origin, net::SiteForCookies());
  }

 protected:
  content::BrowserTaskEnvironment task_environment_;
  base::test::ScopedFeatureList scoped_feature_list_;
  std::unique_ptr<TestingProfile> profile_;
};

// 1. Initial request to https://www.google.com/search:
// Header Chrome-Search-Capabilities-Version: 1 present in
// cors_exempt_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_GoogleUrl_AppendsHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            kContextualTasksSearchCapabilitiesDefaultVersion);
}

// 2. Initial request to https://example.com: Header absent.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_NonGoogleUrl_DoesNotAppendHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://example.com");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
}

// Non-HTTPS Google URL: Header absent.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       InitialRequest_NonHttpsGoogleUrl_DoesNotAppendHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("http://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(request.cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
}

// 3. Redirect Non-Google -> Google (e.g. https://g.ai/ ->
// https://www.google.com/search?udm=50): Header added in
// modified_cors_exempt_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       Redirect_NonGoogleToGoogle_AppendsHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://www.google.com/search?udm=50");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle->WillRedirectRequest(&redirect_info, response_head, &defer,
                                &headers_update_params);
  EXPECT_FALSE(defer);

  EXPECT_EQ(headers_update_params.modified_cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            kContextualTasksSearchCapabilitiesDefaultVersion);
  EXPECT_TRUE(headers_update_params.removed_headers.empty());
}

// 4. Redirect Google -> Non-Google (e.g. https://www.google.com ->
// https://external.com): Header added to removed_headers.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       Redirect_GoogleToNonGoogle_RemovesHeader) {
  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  net::RedirectInfo redirect_info;
  redirect_info.new_url = GURL("https://external.com");
  network::mojom::URLResponseHead response_head;
  bool defer = false;
  network::HttpRequestHeadersUpdateParams headers_update_params;

  throttle->WillRedirectRequest(&redirect_info, response_head, &defer,
                                &headers_update_params);
  EXPECT_FALSE(defer);

  EXPECT_FALSE(headers_update_params.modified_cors_exempt_headers.HasHeader(
      kContextualTasksSearchCapabilitiesHeaderName));
  EXPECT_THAT(
      headers_update_params.removed_headers,
      testing::ElementsAre(kContextualTasksSearchCapabilitiesHeaderName));
}

// 5. Ineligible profile (incognito or feature flags disabled): Throttle
// returns nullptr.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_IncognitoReturnsNullptr) {
  Profile* incognito_profile =
      TestingProfile::Builder().BuildIncognito(profile());
  ASSERT_NE(incognito_profile, nullptr);

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(incognito_profile),
            nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_NullProfileReturnsNullptr) {
  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(nullptr), nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_RearchitectureDisabledReturnsNullptr) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{extensions_features::kApiContextualTasksPrivate},
      /*disabled_features=*/{kContextualTasksRearchitecture});

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(profile()), nullptr);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       IneligibleProfile_PrivateApiDisabledReturnsNullptr) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeatures(
      /*enabled_features=*/{kContextualTasksRearchitecture},
      /*disabled_features=*/{extensions_features::kApiContextualTasksPrivate});

  EXPECT_EQ(ContextualTasksURLLoaderThrottle::MaybeCreate(profile()), nullptr);
}

// Custom version string via base::FeatureParam.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       CustomVersion_ConfiguredViaFeatureParam) {
  scoped_feature_list_.Reset();
  scoped_feature_list_.InitWithFeaturesAndParameters(
      /*enabled_features=*/
      {
          {kContextualTasksRearchitecture,
           {{"contextual-tasks-search-capabilities-version", "2.0"}}},
          {extensions_features::kApiContextualTasksPrivate, {}},
      },
      /*disabled_features=*/{});

  auto throttle = ContextualTasksURLLoaderThrottle::MaybeCreate(profile());
  ASSERT_NE(throttle, nullptr);

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/search");
  bool defer = false;

  throttle->WillStartRequest(&request, &defer);

  EXPECT_EQ(request.cors_exempt_headers.GetHeader(
                kContextualTasksSearchCapabilitiesHeaderName),
            "2.0");
}

// 6. Subresource proxying URLLoaderFactory appends header on Google HTTPS
// subresource requests (e.g. /async/folif and /async/folwr) when the top-level
// frame also has the header.
TEST_F(ContextualTasksURLLoaderThrottleTest,
       SubresourceProxy_GoogleUrl_AppendsHeader) {
  network::URLLoaderFactoryBuilder builder;
  ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
      profile(), CreateIsolationInfo(GURL("https://www.google.com")), builder);
  EXPECT_EQ(builder.num_interceptors(), 1u);

  network::TestURLLoaderFactory test_factory;
  scoped_refptr<network::SharedURLLoaderFactory> proxied_factory =
      std::move(builder).Finish(test_factory.GetSafeWeakWrapper());

  network::ResourceRequest request;
  request.url = GURL("https://www.google.com/async/folif");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();
  proxied_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/1,
      /*options=*/0, request, std::move(client_remote),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  test_factory.WaitForRequest(GURL("https://www.google.com/async/folif"));
  ASSERT_EQ(test_factory.NumPending(), 1);
  EXPECT_EQ(
      test_factory.GetPendingRequest(0)->request.cors_exempt_headers.GetHeader(
          kContextualTasksSearchCapabilitiesHeaderName),
      kContextualTasksSearchCapabilitiesDefaultVersion);
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       SubresourceProxy_NonGoogleTopLevelFrame_DoesNotProxy) {
  {
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        profile(), CreateIsolationInfo(GURL("https://example.com")), builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }

  {
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        profile(), CreateIsolationInfo(GURL("http://www.google.com")), builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }

  {
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        profile(), net::IsolationInfo(), builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       SubresourceProxy_NonGoogleUrl_DoesNotAppendHeader) {
  network::URLLoaderFactoryBuilder builder;
  ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
      profile(), CreateIsolationInfo(GURL("https://www.google.com")), builder);
  EXPECT_EQ(builder.num_interceptors(), 1u);

  network::TestURLLoaderFactory test_factory;
  scoped_refptr<network::SharedURLLoaderFactory> proxied_factory =
      std::move(builder).Finish(test_factory.GetSafeWeakWrapper());

  network::ResourceRequest request;
  request.url = GURL("https://example.com/async/folif");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();
  proxied_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/1,
      /*options=*/0, request, std::move(client_remote),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  test_factory.WaitForRequest(GURL("https://example.com/async/folif"));
  ASSERT_EQ(test_factory.NumPending(), 1);
  EXPECT_FALSE(
      test_factory.GetPendingRequest(0)->request.cors_exempt_headers.HasHeader(
          kContextualTasksSearchCapabilitiesHeaderName));
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       SubresourceProxy_NonHttpsGoogleUrl_DoesNotAppendHeader) {
  network::URLLoaderFactoryBuilder builder;
  ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
      profile(), CreateIsolationInfo(GURL("https://www.google.com")), builder);
  EXPECT_EQ(builder.num_interceptors(), 1u);

  network::TestURLLoaderFactory test_factory;
  scoped_refptr<network::SharedURLLoaderFactory> proxied_factory =
      std::move(builder).Finish(test_factory.GetSafeWeakWrapper());

  network::ResourceRequest request;
  request.url = GURL("http://www.google.com/async/folwr");

  mojo::PendingRemote<network::mojom::URLLoader> loader;
  mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
  auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();
  proxied_factory->CreateLoaderAndStart(
      loader.InitWithNewPipeAndPassReceiver(), /*request_id=*/1,
      /*options=*/0, request, std::move(client_remote),
      net::MutableNetworkTrafficAnnotationTag(TRAFFIC_ANNOTATION_FOR_TESTS));

  test_factory.WaitForRequest(GURL("http://www.google.com/async/folwr"));
  ASSERT_EQ(test_factory.NumPending(), 1);
  EXPECT_FALSE(
      test_factory.GetPendingRequest(0)->request.cors_exempt_headers.HasHeader(
          kContextualTasksSearchCapabilitiesHeaderName));
}

TEST_F(ContextualTasksURLLoaderThrottleTest,
       SubresourceProxy_IneligibleProfile_DoesNotProxy) {
  const net::IsolationInfo google_isolation_info =
      CreateIsolationInfo(GURL("https://www.google.com"));

  {
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        /*browser_context=*/nullptr, google_isolation_info, builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }

  {
    Profile* incognito_profile =
        TestingProfile::Builder().BuildIncognito(profile());
    ASSERT_NE(incognito_profile, nullptr);
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        incognito_profile, google_isolation_info, builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }

  {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{extensions_features::kApiContextualTasksPrivate},
        /*disabled_features=*/{kContextualTasksRearchitecture});
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        profile(), google_isolation_info, builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }

  {
    scoped_feature_list_.Reset();
    scoped_feature_list_.InitWithFeatures(
        /*enabled_features=*/{kContextualTasksRearchitecture},
        /*disabled_features=*/{
            extensions_features::kApiContextualTasksPrivate});
    network::URLLoaderFactoryBuilder builder;
    ContextualTasksSubresourceProxyingURLLoaderFactory::MaybeProxyRequest(
        profile(), google_isolation_info, builder);
    EXPECT_EQ(builder.num_interceptors(), 0u);
  }
}

}  // namespace contextual_tasks
