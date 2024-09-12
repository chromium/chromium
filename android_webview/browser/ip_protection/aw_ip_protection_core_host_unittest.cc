// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/ip_protection/aw_ip_protection_core_host.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/task/single_thread_task_runner.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/android/blind_sign_message_android_impl.h"
#include "components/ip_protection/common/ip_protection_core_host_helper.h"
#include "components/ip_protection/common/ip_protection_data_types.h"
#include "components/ip_protection/common/ip_protection_proxy_config_direct_fetcher.h"
#include "components/ip_protection/common/mock_blind_sign_auth.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/public/cpp/weak_wrapper_shared_url_loader_factory.h"
#include "services/network/test/test_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/status/status.h"

namespace android_webview {

namespace {

constexpr char kTryGetAuthTokensResultHistogram[] =
    "NetworkService.AwIpProtection.TryGetAuthTokensResult";
constexpr char kTokenBatchHistogram[] =
    "NetworkService.AwIpProtection.TokenBatchRequestTime";

}  // namespace

class AwIpProtectionCoreHostTest : public testing::Test {
 protected:
  AwIpProtectionCoreHostTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        geo_hint_({.country_code = "US",
                   .iso_region = "US-AL",
                   .city_name = "ALABASTER"}) {}

  void SetUp() override {
    core_host_ = std::make_unique<AwIpProtectionCoreHost>(
        /*aw_browser_context=*/nullptr);
    auto bsa = std::make_unique<ip_protection::MockBlindSignAuth>();
    bsa_ = bsa.get();
    core_host_->SetUpForTesting(
        base::MakeRefCounted<network::WeakWrapperSharedURLLoaderFactory>(
            &test_url_loader_factory_),
        std::move(bsa));

    token_server_get_proxy_config_url_ = GURL(base::StrCat(
        {net::features::kIpPrivacyTokenServer.Get(),
         net::features::kIpPrivacyTokenServerGetProxyConfigPath.Get()}));
    ASSERT_TRUE(token_server_get_proxy_config_url_.is_valid());
  }

  void TearDown() override { core_host_->Shutdown(); }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens,
                        network::mojom::IpProtectionProxyLayer proxy_layer) {
    core_host_->TryGetAuthTokens(num_tokens, proxy_layer,
                                 tokens_future_.GetCallback());
    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // Expect that the TryGetAuthTokens call returned the given tokens.
  void ExpectTryGetAuthTokensResult(
      std::vector<ip_protection::BlindSignedAuthToken> bsa_tokens) {
    EXPECT_EQ(std::get<0>(tokens_future_.Get()), bsa_tokens);
    // Clear future so it can be reused and accept new tokens.
    tokens_future_.Clear();
  }

  // Expect that the TryGetAuthTokens call returned nullopt, with
  // `try_again_after` at the given delta from the current time.
  void ExpectTryGetAuthTokensResultFailed(base::TimeDelta try_again_delta) {
    auto& [bsa_tokens, try_again_after] = tokens_future_.Get();
    EXPECT_EQ(bsa_tokens, std::nullopt);
    if (!bsa_tokens) {
      EXPECT_EQ(*try_again_after, base::Time::Now() + try_again_delta);
    }
  }

  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TestFuture<
      const std::optional<std::vector<ip_protection::BlindSignedAuthToken>>&,
      std::optional<base::Time>>
      tokens_future_;

 protected:
  base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                         const std::optional<ip_protection::GeoHint>&>
      proxy_list_future_;

  // URL loader factory used for all fetchers.
  network::TestURLLoaderFactory test_url_loader_factory_;

  // URL at which getProxyConfig is invoked.
  GURL token_server_get_proxy_config_url_;

  // A convenient expiration time for fake tokens, in the future.
  base::Time expiration_time_;

  // A convenient geo hint for fake tokens.
  ip_protection::GeoHint geo_hint_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<AwIpProtectionCoreHost> core_host_;

  // quiche::BlindSignAuthInterface owned and used by the sequence bound
  // ip_protection_token_batch_ipc_fetcher_ in core_host_.
  raw_ptr<ip_protection::MockBlindSignAuth> bsa_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// BSA gets tokens.
TEST_F(AwIpProtectionCoreHostTest, Success) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint_),
                    ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-2", expiration_time_, geo_hint_)});

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);

  std::vector<ip_protection::BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_)
                             .value());
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, geo_hint_)
                             .value());

  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(AwIpProtectionCoreHostTest, NoTokens) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns malformed tokens.
TEST_F(AwIpProtectionCoreHostTest, MalformedTokens) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  auto geo_hint = anonymous_tokens::GeoHint{
      .geo_hint = "US,US-CA,MOUNTAIN VIEW",
      .country_code = "US",
      .region = "US-CA",
      .city = "MOUNTAIN VIEW",
  };

  bsa_->set_tokens(
      {{"invalid-token-proto-data", absl::Now() + absl::Hours(1), geo_hint}});

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA gets tokens.
TEST_F(AwIpProtectionCoreHostTest, TokenGeoHintContainsOnlyCountry) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  ip_protection::GeoHint geo_hint_country;
  geo_hint_country.country_code = "US";
  bsa_->set_tokens(
      {ip_protection::IpProtectionCoreHostHelper::
           CreateBlindSignTokenForTesting("single-use-1", expiration_time_,
                                          geo_hint_country),
       ip_protection::IpProtectionCoreHostHelper::
           CreateBlindSignTokenForTesting("single-use-2", expiration_time_,
                                          geo_hint_country)});

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  EXPECT_EQ(bsa_->num_tokens(), 2);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);

  std::vector<ip_protection::BlindSignedAuthToken> expected;
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, geo_hint_country)
                             .value());
  expected.push_back(ip_protection::IpProtectionCoreHostHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, geo_hint_country)
                             .value());

  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(AwIpProtectionCoreHostTest, TokenHasMissingGeoHint) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  ip_protection::GeoHint geo_hint;
  bsa_->set_tokens({ip_protection::IpProtectionCoreHostHelper::
                        CreateBlindSignTokenForTesting(
                            "single-use-1", expiration_time_, geo_hint)});

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a transient error.
TEST_F(AwIpProtectionCoreHostTest, BlindSignedAuthTransientError) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->set_status(absl::UnavailableError("uhoh"));

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSATransient, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a persistent error.
TEST_F(AwIpProtectionCoreHostTest, BlindSignedAuthPersistentError) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->set_status(absl::FailedPreconditionError("uhoh"));

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSAPersistent, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(AwIpProtectionCoreHostTest, BlindSignedTokenErrorOther) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->set_status(absl::UnknownError("uhoh"));

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called());
  EXPECT_EQ(bsa_->num_tokens(), 1);
  EXPECT_EQ(bsa_->proxy_layer(), quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token(), std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionCoreHostHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      ip_protection::TryGetAuthTokensAndroidResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// TryGetAuthTokens() fails because IP Protection is disabled.
TEST_F(AwIpProtectionCoreHostTest,
       TryGetAuthTokens_IpProtectionDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableIpProtectionProxy);

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called());
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

TEST_F(AwIpProtectionCoreHostTest, ProxyOverrideFlagsAll) {
  std::vector<net::ProxyChain> proxy_override_list = {
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyAOverride", "proxyBOverride"}),
      ip_protection::IpProtectionProxyConfigDirectFetcher::MakeChainForTesting(
          {"proxyAOverride", "proxyBOverride"}),
  };
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {
          {net::features::kIpPrivacyProxyAHostnameOverride.name,
           "proxyAOverride"},
          {net::features::kIpPrivacyProxyBHostnameOverride.name,
           "proxyBOverride"},
      });
  std::vector<std::vector<std::string>> response_proxy_list = {
      {"proxyA1", "proxyB1"}, {"proxyA2", "proxyB2"}};
  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA1");
  chain->set_proxy_b("proxyB1");
  chain = response.add_proxy_chain();
  chain->set_proxy_a("proxyA2");
  chain->set_proxy_b("proxyB2");

  response.mutable_geo_hint()->set_country_code(geo_hint_.country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_.iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_.city_name);
  std::string response_str = response.SerializeAsString();

  test_url_loader_factory_.AddResponse(
      token_server_get_proxy_config_url_.spec(), response_str);

  core_host_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(proxy_override_list));

  ASSERT_TRUE(geo_hint.has_value());  // Check that GeoHint is not null.
  EXPECT_TRUE(geo_hint == geo_hint_);
}

TEST_F(AwIpProtectionCoreHostTest, GetProxyListFailure) {
  core_host_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();
  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_FALSE(geo_hint.has_value());
}

TEST_F(AwIpProtectionCoreHostTest, GetProxyList_IpProtectionDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableIpProtectionProxy);

  core_host_->GetProxyList(proxy_list_future_.GetCallback());

  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_FALSE(geo_hint.has_value());
}

}  // namespace android_webview
