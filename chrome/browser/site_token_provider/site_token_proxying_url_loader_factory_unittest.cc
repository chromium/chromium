// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/site_token_provider/site_token_proxying_url_loader_factory.h"

#include <memory>
#include <string>
#include <vector>

#include "base/functional/bind.h"
#include "base/run_loop.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/site_token_provider/site_token_provider_service_factory.h"
#include "chrome/test/base/testing_profile.h"
#include "components/site_token_provider/features.h"
#include "components/site_token_provider/site_token_provider_service.h"
#include "content/public/test/browser_task_environment.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "mojo/public/cpp/bindings/pending_remote.h"
#include "mojo/public/cpp/bindings/remote.h"
#include "net/traffic_annotation/network_traffic_annotation.h"
#include "services/network/public/cpp/resource_request.h"
#include "services/network/public/cpp/url_loader_factory_builder.h"
#include "services/network/public/mojom/url_loader.mojom.h"
#include "services/network/public/mojom/url_loader_factory.mojom.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "url/gurl.h"

namespace site_token_provider {
namespace {

constexpr char kAllowlistedHost[] = "example.com";
constexpr char kTestToken[] = "test-token-value";

class SiteTokenProxyingURLLoaderFactoryTest : public testing::Test {
 public:
  SiteTokenProxyingURLLoaderFactoryTest() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        features::kSiteTokenProviderEnabled,
        {{"site_token_allowlist", "example.com,localhost"}});
  }

 protected:
  void SetUp() override { profile_ = std::make_unique<TestingProfile>(); }

  void TearDown() override { profile_.reset(); }

  SiteTokenProviderService* GetService() {
    return SiteTokenProviderServiceFactory::GetForProfile(profile_.get());
  }

  mojo::Remote<network::mojom::URLLoaderFactory> FinishBuilder(
      network::URLLoaderFactoryBuilder builder) {
    mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
    test_url_loader_factory_.Clone(
        target_factory_remote.InitWithNewPipeAndPassReceiver());

    return mojo::Remote<network::mojom::URLLoaderFactory>(
        std::move(builder)
            .Finish<mojo::PendingRemote<network::mojom::URLLoaderFactory>>(
                std::move(target_factory_remote)));
  }

  uint32_t CreateLoaderAndGetOptions(network::mojom::URLLoaderFactory* factory,
                                     const GURL& url,
                                     uint32_t initial_options = 0) {
    size_t prev_count = test_url_loader_factory_.pending_requests()->size();

    network::ResourceRequest request;
    request.url = url;

    mojo::Remote<network::mojom::URLLoader> loader;
    mojo::PendingRemote<network::mojom::URLLoaderClient> client_remote;
    auto client_receiver = client_remote.InitWithNewPipeAndPassReceiver();

    factory->CreateLoaderAndStart(loader.BindNewPipeAndPassReceiver(),
                                  /*request_id=*/0, initial_options, request,
                                  std::move(client_remote),
                                  net::MutableNetworkTrafficAnnotationTag());

    test_url_loader_factory_.WaitForRequest(url);
    CHECK_EQ(test_url_loader_factory_.pending_requests()->size(),
             prev_count + 1);

    return test_url_loader_factory_.pending_requests()->back().options;
  }

  content::BrowserTaskEnvironment task_environment_;
  std::unique_ptr<TestingProfile> profile_;
  network::TestURLLoaderFactory test_url_loader_factory_;
  base::test::ScopedFeatureList scoped_feature_list_;
};

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       RequestNotProxiedIfFeatureDisabled) {
  base::test::ScopedFeatureList disabled_feature_list;
  disabled_feature_list.InitAndDisableFeature(
      features::kSiteTokenProviderEnabled);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest, NoFlagIfEmptyAllowlist) {
  base::test::ScopedFeatureList empty_allowlist_feature_list;
  empty_allowlist_feature_list.InitAndEnableFeatureWithParameters(
      features::kSiteTokenProviderEnabled, {{"site_token_allowlist", ""}});

  TestingProfile empty_profile;
  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(&empty_profile, builder);
  ASSERT_EQ(1u, builder.num_interceptors());

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("https://example.com/article"));
  EXPECT_FALSE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       RequestNotProxiedIfNullBrowserContext) {
  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(nullptr, builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       RequestNotProxiedIfIncognitoProfile) {
  TestingProfile::Builder otr_builder;
  TestingProfile* otr_profile = otr_builder.BuildIncognito(profile_.get());

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(otr_profile, builder);
  EXPECT_EQ(0u, builder.num_interceptors());
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest, AddsFlagForAllowlistedDomain) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);
  ASSERT_EQ(1u, builder.num_interceptors());

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("https://example.com/article"));
  EXPECT_TRUE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest, NoFlagForNonAllowlistedDomain) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("https://google.com/search"));
  EXPECT_FALSE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       NoFlagForInsecureHttpEvenIfAllowlisted) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("http://example.com/article"));
  EXPECT_FALSE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest, PreservesExistingLoadOptions) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  uint32_t initial_options =
      network::mojom::kURLLoadOptionSendSSLInfoWithResponse |
      network::mojom::kURLLoadOptionSniffMimeType;

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("https://example.com/article"), initial_options);

  EXPECT_TRUE(options & network::mojom::kURLLoadOptionUseHeaderClient);
  EXPECT_TRUE(options & network::mojom::kURLLoadOptionSendSSLInfoWithResponse);
  EXPECT_TRUE(options & network::mojom::kURLLoadOptionSniffMimeType);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       MultipleRequestsThroughSameFactory) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  // Initial request for allowlisted domain: sets flag.
  uint32_t opt1 = CreateLoaderAndGetOptions(factory.get(),
                                            GURL("https://example.com/page1"));
  EXPECT_TRUE(opt1 & network::mojom::kURLLoadOptionUseHeaderClient);

  // Sets flag -> doesn't set flag.
  uint32_t opt2 = CreateLoaderAndGetOptions(factory.get(),
                                            GURL("https://google.com/search"));
  EXPECT_FALSE(opt2 & network::mojom::kURLLoadOptionUseHeaderClient);

  // Doesn't set flag -> sets flag.
  uint32_t opt3 = CreateLoaderAndGetOptions(factory.get(),
                                            GURL("https://example.com/page2"));
  EXPECT_TRUE(opt3 & network::mojom::kURLLoadOptionUseHeaderClient);

  // Sets flag -> doesn't set flag.
  uint32_t opt4 = CreateLoaderAndGetOptions(factory.get(),
                                            GURL("https://youtube.com/watch"));
  EXPECT_FALSE(opt4 & network::mojom::kURLLoadOptionUseHeaderClient);
}

struct SchemeTestCase {
  const char* url_spec;
  bool expected_header_flag;
};

class SiteTokenProxyingURLLoaderFactorySchemeTest
    : public SiteTokenProxyingURLLoaderFactoryTest,
      public ::testing::WithParamInterface<SchemeTestCase> {};

TEST_P(SiteTokenProxyingURLLoaderFactorySchemeTest,
       SetsFlagAccordingToUrlAndScheme) {
  SiteTokenProviderService* service = GetService();
  ASSERT_TRUE(service);
  service->SetTokenForTesting(kAllowlistedHost, kTestToken);
  service->SetTokenForTesting("localhost", kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  const SchemeTestCase& test_case = GetParam();
  const GURL url(test_case.url_spec);
  uint32_t options = CreateLoaderAndGetOptions(factory.get(), url);
  const bool has_flag =
      (options & network::mojom::kURLLoadOptionUseHeaderClient) != 0;
  EXPECT_EQ(test_case.expected_header_flag, has_flag)
      << "Mismatch for URL: " << test_case.url_spec;
}

INSTANTIATE_TEST_SUITE_P(
    All,
    SiteTokenProxyingURLLoaderFactorySchemeTest,
    ::testing::Values(
        // Valid HTTPS matching host -> Flagged
        SchemeTestCase{"https://example.com/article", true},
        // Valid HTTPS non-matching host -> Not flagged
        SchemeTestCase{"https://google.com/search", false},
        // Insecure HTTP matching remote host -> Not flagged (must be
        // cryptographic)
        SchemeTestCase{"http://example.com/article", false},
        // HTTP localhost -> Flagged (localhost exemption)
        SchemeTestCase{"http://localhost:8080/api", true},
        // Non-standard / internal schemes -> Not flagged
        SchemeTestCase{"data:text/plain,hello", false},
        SchemeTestCase{"about:blank", false},
        SchemeTestCase{"chrome://settings", false},
        SchemeTestCase{"file:///tmp/test.txt", false},
        SchemeTestCase{"javascript:void(0)", false},
        // Empty / invalid URL -> Not flagged
        SchemeTestCase{"", false}));

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       ServiceDestroyedPassesThroughWithoutFlag) {
  GetService()->SetTokenForTesting(kAllowlistedHost, kTestToken);

  network::URLLoaderFactoryBuilder builder;
  SiteTokenProxyingURLLoaderFactory::MaybeProxyRequest(profile_.get(), builder);

  mojo::Remote<network::mojom::URLLoaderFactory> factory =
      FinishBuilder(std::move(builder));

  // Destroy Profile and Service while factory remote remains active.
  profile_.reset();

  uint32_t options = CreateLoaderAndGetOptions(
      factory.get(), GURL("https://example.com/article"));
  EXPECT_FALSE(options & network::mojom::kURLLoadOptionUseHeaderClient);
}

TEST_F(SiteTokenProxyingURLLoaderFactoryTest,
       HandlesTargetDisconnectGracefully) {
  mojo::Remote<network::mojom::URLLoaderFactory> proxy_factory;
  mojo::PendingRemote<network::mojom::URLLoaderFactory> target_factory_remote;
  auto target_receiver = target_factory_remote.InitWithNewPipeAndPassReceiver();

  base::MakeSelfDeleting<SiteTokenProxyingURLLoaderFactory>(
      proxy_factory.BindNewPipeAndPassReceiver(),
      std::move(target_factory_remote),
      base::BindRepeating([](const GURL& url) { return true; }));

  target_receiver.reset();

  base::RunLoop run_loop;
  proxy_factory.set_disconnect_handler(run_loop.QuitClosure());
  run_loop.Run();

  EXPECT_FALSE(proxy_factory.is_connected());
}

}  // namespace
}  // namespace site_token_provider
