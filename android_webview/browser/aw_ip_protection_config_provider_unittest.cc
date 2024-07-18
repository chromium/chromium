// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/browser/aw_ip_protection_config_provider.h"

#include <memory>
#include <optional>

#include "base/memory/scoped_refptr.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "base/types/expected.h"
#include "components/ip_protection/blind_sign_message_android_impl.h"
#include "components/ip_protection/ip_protection_config_provider_helper.h"
#include "components/ip_protection/ip_protection_proxy_config_fetcher.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {
namespace {
constexpr char kTryGetAuthTokensResultHistogram[] =
    "NetworkService.AwIpProtection.TryGetAuthTokensResult";
constexpr char kTokenBatchHistogram[] =
    "NetworkService.AwIpProtection.TokenBatchRequestTime";

class MockBlindSignAuth : public quiche::BlindSignAuthInterface {
 public:
  void GetTokens(std::optional<std::string> oauth_token,
                 int num_tokens,
                 quiche::ProxyLayer proxy_layer,
                 quiche::BlindSignAuthServiceType /*service_type*/,
                 quiche::SignedTokenCallback callback) override {
    get_tokens_called_ = true;
    oauth_token_ = oauth_token;
    num_tokens_ = num_tokens;
    proxy_layer_ = proxy_layer;

    absl::StatusOr<absl::Span<quiche::BlindSignToken>> result;
    if (status_.ok()) {
      result = absl::Span<quiche::BlindSignToken>(tokens_);
    } else {
      result = status_;
    }

    content::GetUIThreadTaskRunner({})->PostTask(
        FROM_HERE,
        base::BindOnce(
            [](quiche::SignedTokenCallback callback,
               absl::StatusOr<absl::Span<quiche::BlindSignToken>> result) {
              std::move(callback)(std::move(result));
            },
            std::move(callback), std::move(result)));
  }

  // True if `GetTokens()` was called.
  bool get_tokens_called_;

  // The token with which `GetTokens()` was called.
  std::optional<std::string> oauth_token_;

  // The num_tokens with which `GetTokens()` was called.
  int num_tokens_;

  // The proxy for which the tokens are intended for.
  quiche::ProxyLayer proxy_layer_;

  // If not Ok, the status that will be returned from `GetTokens()`.
  absl::Status status_ = absl::OkStatus();

  // The tokens that will be returned from `GetTokens()` , if `status_` is not
  // `OkStatus`.
  std::vector<quiche::BlindSignToken> tokens_;
};

class MockIpProtectionProxyConfigRetriever
    : public ip_protection::IpProtectionProxyConfigRetriever {
 public:
  explicit MockIpProtectionProxyConfigRetriever(
      std::optional<ip_protection::GetProxyConfigResponse>
          proxy_config_response)
      : ip_protection::IpProtectionProxyConfigRetriever(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>(),
            "test_service_type",
            "test_api_key"),
        proxy_config_response_(proxy_config_response) {}

  void GetProxyConfig(
      std::optional<std::string> oauth_token,
      IpProtectionProxyConfigRetriever::GetProxyConfigCallback callback,
      bool for_testing) override {
    if (!proxy_config_response_.has_value()) {
      std::move(callback).Run(base::unexpected("uhoh"));
      return;
    }
    std::move(callback).Run(*proxy_config_response_);
  }

 private:
  std::optional<ip_protection::GetProxyConfigResponse> proxy_config_response_;
};

}  // namespace

class AwIpProtectionConfigProviderTest : public testing::Test {
 protected:
  AwIpProtectionConfigProviderTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)),
        geo_hint_(network::mojom::GeoHint::New("US", "US-AL", "ALABASTER")) {}

  void SetUp() override {
    getter_ = std::make_unique<AwIpProtectionConfigProvider>(
        /*aw_browser_context=*/nullptr);
    bsa_ = std::make_unique<MockBlindSignAuth>();
    getter_->SetUpForTesting(
        std::make_unique<MockIpProtectionProxyConfigRetriever>(std::nullopt),
        std::make_unique<ip_protection::BlindSignMessageAndroidImpl>(),
        bsa_.get());
  }

  void TearDown() override { getter_->Shutdown(); }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens,
                        network::mojom::IpProtectionProxyLayer proxy_layer) {
    getter_->TryGetAuthTokens(num_tokens, proxy_layer,
                              tokens_future_.GetCallback());
    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // Expect that the TryGetAuthTokens call returned the given tokens.
  void ExpectTryGetAuthTokensResult(
      std::vector<network::mojom::BlindSignedAuthTokenPtr> bsa_tokens) {
    EXPECT_EQ(std::get<0>(tokens_future_.Get()), bsa_tokens);
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
      std::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>,
      std::optional<base::Time>>
      tokens_future_;

 protected:
  base::test::TestFuture<const std::optional<std::vector<net::ProxyChain>>&,
                         network::mojom::GeoHintPtr>
      proxy_list_future_;

  // A convenient expiration time for fake tokens, in the future.
  base::Time expiration_time_;

  // A convenient geo hint for fake tokens.
  network::mojom::GeoHintPtr geo_hint_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<AwIpProtectionConfigProvider> getter_;
  // Note: In the real implementation, `AwIpProtectionConfigProvider()` owns the
  // BSA implementation and can't be destroyed before it, which is important
  // because BSA takes a pointer to `AwIpProtectionConfigProvider()` when
  // requesting tokens and calls back with it once tokens are available. Here,
  // `getter_` doesn't own the BSA implementation, so it's important to ensure
  // that `bsa_` gets destroyed before `getter_` when the test class is
  // destroyed so that `bsa_` doesn't call back into a destroyed `getter_`.
  std::unique_ptr<MockBlindSignAuth> bsa_;

  base::test::ScopedFeatureList scoped_feature_list_;
};

// BSA gets tokens.
TEST_F(AwIpProtectionConfigProviderTest, Success) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->tokens_ = {ip_protection::IpProtectionConfigProviderHelper::
                       CreateBlindSignTokenForTesting(
                           "single-use-1", expiration_time_, *geo_hint_),
                   ip_protection::IpProtectionConfigProviderHelper::
                       CreateBlindSignTokenForTesting(
                           "single-use-2", expiration_time_, *geo_hint_)};

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  EXPECT_EQ(bsa_->num_tokens_, 2);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);

  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(ip_protection::IpProtectionConfigProviderHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-1", expiration_time_, *geo_hint_));
  expected.push_back(ip_protection::IpProtectionConfigProviderHelper::
                         CreateMockBlindSignedAuthTokenForTesting(
                             "single-use-2", expiration_time_, *geo_hint_));

  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(AwIpProtectionConfigProviderTest, NoTokens) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns malformed tokens.
TEST_F(AwIpProtectionConfigProviderTest, MalformedTokens) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  auto geo_hint = anonymous_tokens::GeoHint{
      .geo_hint = "US,US-CA,MOUNTAIN VIEW",
      .country_code = "US",
      .region = "US-CA",
      .city = "MOUNTAIN VIEW",
  };

  bsa_->tokens_ = {
      {"invalid-token-proto-data", absl::Now() + absl::Hours(1), geo_hint}};

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA gets tokens.
TEST_F(AwIpProtectionConfigProviderTest, TokenGeoHintContainsOnlyCountry) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  auto geo_hint_country = network::mojom::GeoHint::New();
  geo_hint_country->country_code = "US";
  bsa_->tokens_ = {
      ip_protection::IpProtectionConfigProviderHelper::
          CreateBlindSignTokenForTesting("single-use-1", expiration_time_,
                                         *geo_hint_country),
      ip_protection::IpProtectionConfigProviderHelper::
          CreateBlindSignTokenForTesting("single-use-2", expiration_time_,
                                         *geo_hint_country)};

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  EXPECT_EQ(bsa_->num_tokens_, 2);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);

  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(
      ip_protection::IpProtectionConfigProviderHelper::
          CreateMockBlindSignedAuthTokenForTesting(
              "single-use-1", expiration_time_, *geo_hint_country));
  expected.push_back(
      ip_protection::IpProtectionConfigProviderHelper::
          CreateMockBlindSignedAuthTokenForTesting(
              "single-use-2", expiration_time_, *geo_hint_country));

  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(AwIpProtectionConfigProviderTest, TokenHasMissingGeoHint) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);
  bsa_->tokens_ = {
      ip_protection::IpProtectionConfigProviderHelper::
          CreateBlindSignTokenForTesting("single-use-1", expiration_time_,
                                         *network::mojom::GeoHint::New())};

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a transient error.
TEST_F(AwIpProtectionConfigProviderTest, BlindSignedAuthTransientError) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->status_ = absl::UnavailableError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSATransient, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a persistent error.
TEST_F(AwIpProtectionConfigProviderTest, BlindSignedAuthPersistentError) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->status_ = absl::FailedPreconditionError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSAPersistent, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(AwIpProtectionConfigProviderTest, BlindSignedTokenErrorOther) {
  scoped_feature_list_.InitAndEnableFeature(
      net::features::kEnableIpProtectionProxy);

  bsa_->status_ = absl::UnknownError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, std::nullopt);
  ExpectTryGetAuthTokensResultFailed(
      ip_protection::IpProtectionConfigProviderHelper::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      AwIpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// TryGetAuthTokens() fails because IP Protection is disabled.
TEST_F(AwIpProtectionConfigProviderTest,
       TryGetAuthTokens_IpProtectionDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableIpProtectionProxy);

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}


TEST_F(AwIpProtectionConfigProviderTest, ProxyOverrideFlagsAll) {
  std::vector<net::ProxyChain> proxy_override_list = {
      ip_protection::IpProtectionProxyConfigFetcher::MakeChainForTesting(
          {"proxyAOverride", "proxyBOverride"}),
      ip_protection::IpProtectionProxyConfigFetcher::MakeChainForTesting(
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

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  getter_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response),
      std::make_unique<ip_protection::BlindSignMessageAndroidImpl>(),
      bsa_.get());
  getter_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  ASSERT_TRUE(proxy_list.has_value());  // Check if optional has value.
  EXPECT_THAT(proxy_list.value(),
              testing::ElementsAreArray(proxy_override_list));

  ASSERT_TRUE(geo_hint);  // Check that GeoHintPtr is not null.
  EXPECT_TRUE(geo_hint->Equals(*geo_hint_));
}

TEST_F(AwIpProtectionConfigProviderTest, GetProxyListFailure) {
  getter_->GetProxyList(proxy_list_future_.GetCallback());
  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();
  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_TRUE(geo_hint.is_null());
}

TEST_F(AwIpProtectionConfigProviderTest, GetProxyList_IpProtectionDisabled) {
  scoped_feature_list_.InitAndDisableFeature(
      net::features::kEnableIpProtectionProxy);

  ip_protection::GetProxyConfigResponse response;
  auto* chain = response.add_proxy_chain();
  chain->set_proxy_a("proxy1");
  chain->set_proxy_b("proxy1b");
  chain->set_chain_id(1);

  response.mutable_geo_hint()->set_country_code(geo_hint_->country_code);
  response.mutable_geo_hint()->set_iso_region(geo_hint_->iso_region);
  response.mutable_geo_hint()->set_city_name(geo_hint_->city_name);

  getter_->SetUpForTesting(
      std::make_unique<MockIpProtectionProxyConfigRetriever>(response),
      std::make_unique<ip_protection::BlindSignMessageAndroidImpl>(),
      bsa_.get());

  getter_->GetProxyList(proxy_list_future_.GetCallback());

  ASSERT_TRUE(proxy_list_future_.Wait()) << "GetProxyList did not call back";

  // Extract tuple elements for individual comparison.
  const auto& [proxy_list, geo_hint] = proxy_list_future_.Get();

  EXPECT_EQ(proxy_list, std::nullopt);
  EXPECT_TRUE(geo_hint.is_null());
}


}  // namespace android_webview
