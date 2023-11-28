// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_config_provider.h"

#include "base/base64.h"
#include "base/notreached.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "chrome/browser/ip_protection/get_proxy_config.pb.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "components/signin/public/identity_manager/primary_account_change_event.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/proto/spend_token_data.pb.h"
#include "services/network/test/test_shared_url_loader_factory.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

constexpr char kTryGetAuthTokensResultHistogram[] =
    "NetworkService.IpProtection.TryGetAuthTokensResult";
constexpr char kOAuthTokenFetchHistogram[] =
    "NetworkService.IpProtection.OAuthTokenFetchTime";
constexpr char kTokenBatchHistogram[] =
    "NetworkService.IpProtection.TokenBatchRequestTime";

constexpr char kTestEmail[] = "test@example.com";

privacy::ppn::PrivacyPassTokenData CreateMockPrivacyPassToken(
    std::string token_value) {
  privacy::ppn::PrivacyPassTokenData privacy_pass_token_data;

  // The PrivacyPassTokenData values get base64-encoded by BSA, so simulate that
  // here.
  std::string encoded_token_value;
  std::string encoded_extension_value;

  base::Base64Encode(token_value, &encoded_token_value);
  base::Base64Encode("mock-extension-value", &encoded_extension_value);

  privacy_pass_token_data.set_token(std::move(encoded_token_value));
  privacy_pass_token_data.set_encoded_extensions(
      std::move(encoded_extension_value));

  return privacy_pass_token_data;
}

// Creates a `quiche::BlindSignToken()` in the format that the BSA library
// will return them.
quiche::BlindSignToken CreateMockBlindSignToken(std::string token_value,
                                                base::Time expiration) {
  quiche::BlindSignToken blind_sign_token;

  if (net::features::kIpPrivacyBsaEnablePrivacyPass.Get()) {
    privacy::ppn::PrivacyPassTokenData privacy_pass_token_data =
        CreateMockPrivacyPassToken(std::move(token_value));
    blind_sign_token.token = privacy_pass_token_data.SerializeAsString();
  } else {
    blind_sign_token.token = std::move(token_value);
  }
  blind_sign_token.expiration = absl::FromTimeT(expiration.ToTimeT());
  return blind_sign_token;
}

class MockBlindSignAuth : public quiche::BlindSignAuthInterface {
 public:
  void GetTokens(std::string oauth_token,
                 int num_tokens,
                 quiche::ProxyLayer proxy_layer,
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
  std::string oauth_token_;

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

// Mock for `IpProtectionConfigHttp`. This is used only for testing methods that
// are called directly from `IpProtectionConfigProvider`, not those called
// indirectly via BSA.
class MockIpProtectionConfigHttp : public IpProtectionConfigHttp {
 public:
  explicit MockIpProtectionConfigHttp(
      absl::optional<std::vector<std::vector<std::string>>> proxy_list)
      : IpProtectionConfigHttp(
            base::MakeRefCounted<network::TestSharedURLLoaderFactory>()),
        proxy_list_(proxy_list) {}

  void DoRequest(quiche::BlindSignHttpRequestType request_type,
                 const std::string& authorization_header,
                 const std::string& body,
                 quiche::BlindSignHttpCallback callback) override {
    // DoRequest is not supported in this mock.
    NOTREACHED();
  }

  void GetProxyConfig(IpProtectionConfigHttp::GetProxyConfigCallback callback,
                      bool for_testing = false) override {
    if (!proxy_list_.has_value()) {
      std::move(callback).Run(absl::InternalError("uhoh"));
      return;
    }
    ip_protection::GetProxyConfigResponse response;
    for (auto& hostnames : *proxy_list_) {
      if (net::features::kIpPrivacyUseProxyChains.Get()) {
        ip_protection::GetProxyConfigResponse_ProxyChain* proxyChain =
            response.add_proxy_chain();
        proxyChain->set_proxy_a(hostnames.size() > 0 ? hostnames.at(0) : "");
        proxyChain->set_proxy_b(hostnames.size() > 1 ? hostnames.at(1) : "");
      } else {
        CHECK_EQ(1u, hostnames.size());
        response.add_first_hop_hostnames(hostnames.at(0));
      }
    }
    std::move(callback).Run(response);
  }

 private:
  absl::optional<std::vector<std::vector<std::string>>> proxy_list_;
};

enum class PrimaryAccountBehavior {
  // Primary account not set.
  kNone,

  // Primary account exists but returns a transient error fetching access token.
  kTokenFetchTransientError,

  // Primary account exists but returns a persistent error fetching access
  // token.
  kTokenFetchPersistentError,

  // Primary account exists but is not eligible for IP protection.
  kIneligible,

  // Primary account exists but eligibility is kUnknown.
  kUnknownEligibility,

  // Primary account exists, is eligible, and returns OAuth token
  // "access_token".
  kReturnsToken,
};

}  // namespace

class IpProtectionConfigProviderTest : public testing::Test {
 protected:
  IpProtectionConfigProviderTest()
      : expiration_time_(base::Time::Now() + base::Hours(1)) {}

  void SetUp() override {
    getter_ = std::make_unique<IpProtectionConfigProvider>(IdentityManager(),
                                                           /*profile=*/nullptr);
    bsa_ = std::make_unique<MockBlindSignAuth>();
    getter_->SetUpForTesting(
        std::make_unique<MockIpProtectionConfigHttp>(absl::nullopt),
        bsa_.get());
  }

  void TearDown() override { getter_->Shutdown(); }

  // Get the IdentityManager for this test.
  signin::IdentityManager* IdentityManager() {
    return identity_test_env_.identity_manager();
  }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens,
                        network::mojom::IpProtectionProxyLayer proxy_layer) {
    if (primary_account_behavior_ == PrimaryAccountBehavior::kNone) {
#if !BUILDFLAG(IS_CHROMEOS_ASH)
      // Simulate a log out event on all platforms except ChromeOS Ash where
      // this is not supported.
      identity_test_env_.ClearPrimaryAccount();
#endif
    } else {
      identity_test_env_.MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);

      if (primary_account_behavior_ == PrimaryAccountBehavior::kIneligible) {
        SetCanUseChromeIpProtectionCapability(false);
      } else {
        SetCanUseChromeIpProtectionCapability(true);
      }
    }

    getter_->TryGetAuthTokens(num_tokens, proxy_layer,
                              tokens_future_.GetCallback());

    switch (primary_account_behavior_) {
      case PrimaryAccountBehavior::kNone:
      case PrimaryAccountBehavior::kIneligible:
        break;
      case PrimaryAccountBehavior::kTokenFetchTransientError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::CONNECTION_FAILED));
        break;
      case PrimaryAccountBehavior::kTokenFetchPersistentError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::INVALID_GAIA_CREDENTIALS));
        break;
      case PrimaryAccountBehavior::kUnknownEligibility:
      case PrimaryAccountBehavior::kReturnsToken:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
                "access_token", base::Time::Now());
        break;
    }
    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // Set the CanUseChromeIpProtection account capability. The capability tribool
  // defaults to `kUnknown`.
  void SetCanUseChromeIpProtectionCapability(bool enabled) {
    auto account_info = identity_test_env_.identity_manager()
                            ->FindExtendedAccountInfoByEmailAddress(kTestEmail);
    AccountCapabilitiesTestMutator mutator(&account_info.capabilities);
    mutator.set_can_use_chrome_ip_protection(enabled);
    signin::UpdateAccountInfoForAccount(identity_test_env_.identity_manager(),
                                        account_info);
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
    EXPECT_EQ(bsa_tokens, absl::nullopt);
    if (!bsa_tokens) {
      EXPECT_EQ(*try_again_after, base::Time::Now() + try_again_delta);
    }
  }

  // Converts a mock token value and expiration time into the struct that will
  // be passed to the network service, including the formatting that the
  // `IpProtectionConfigProvider()` will do.
  static network::mojom::BlindSignedAuthTokenPtr CreateMockBlindSignedAuthToken(
      std::string token_value,
      base::Time expiration) {
    quiche::BlindSignToken blind_sign_token =
        CreateMockBlindSignToken(token_value, expiration);
    return IpProtectionConfigProvider::CreateBlindSignedAuthToken(
        std::move(blind_sign_token));
  }

  // The behavior of the identity manager.
  PrimaryAccountBehavior primary_account_behavior_ =
      PrimaryAccountBehavior::kReturnsToken;

  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_{
      base::test::TaskEnvironment::TimeSource::MOCK_TIME};
  base::test::TestFuture<
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>,
      absl::optional<base::Time>>
      tokens_future_;

  // Test environment for IdentityManager. This must come after the
  // TaskEnvironment.
  signin::IdentityTestEnvironment identity_test_env_;

  // A convenient expiration time for fake tokens, in the future.
  base::Time expiration_time_;

  base::HistogramTester histogram_tester_;

  std::unique_ptr<IpProtectionConfigProvider> getter_;
  // Note: In the real implementation, `IpProtectionConfigProvider()` owns the
  // BSA implementation and can't be destroyed before it, which is important
  // because BSA takes a pointer to `IpProtectionConfigProvider()` when
  // requesting tokens and calls back with it once tokens are available. Here,
  // `getter_` doesn't own the BSA implementation, so it's important to ensure
  // that `bsa_` gets destroyed before `getter_` when the test class is
  // destroyed so that `bsa_` doesn't call back into a destroyed `getter_`.
  std::unique_ptr<MockBlindSignAuth> bsa_;
};

enum class TokenFormatTestCase {
  kTokenFormatRegular,
  kTokenFormatPrivacyPass,
};

class IpProtectionConfigProviderTest_TokenFormat
    : public IpProtectionConfigProviderTest,
      public ::testing::WithParamInterface<TokenFormatTestCase> {
 public:
  IpProtectionConfigProviderTest_TokenFormat() {
    scoped_feature_list_.InitAndEnableFeatureWithParameters(
        net::features::kEnableIpProtectionProxy,
        {{net::features::kIpPrivacyBsaEnablePrivacyPass.name,
          IsPrivacyPassTokenFormatEnabled() ? "true" : "false"}});
  }

  bool IsPrivacyPassTokenFormatEnabled() const {
    return GetParam() == TokenFormatTestCase::kTokenFormatPrivacyPass;
  }

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

INSTANTIATE_TEST_SUITE_P(
    All,
    IpProtectionConfigProviderTest_TokenFormat,
    testing::ValuesIn({TokenFormatTestCase::kTokenFormatRegular,
                       TokenFormatTestCase::kTokenFormatPrivacyPass}),
    [](const testing::TestParamInfo<TokenFormatTestCase>& info) {
      switch (info.param) {
        case (TokenFormatTestCase::kTokenFormatRegular):
          return "TokenFormatRegular";
        case (TokenFormatTestCase::kTokenFormatPrivacyPass):
          return "TokenFormatPrivacyPass";
      }
    });

// The success case: a primary account is available, and BSA gets a token for
// it.
TEST_P(IpProtectionConfigProviderTest_TokenFormat, Success) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->tokens_ = {CreateMockBlindSignToken("single-use-1", expiration_time_),
                   CreateMockBlindSignToken("single-use-2", expiration_time_)};

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  EXPECT_EQ(bsa_->num_tokens_, 2);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(
      CreateMockBlindSignedAuthToken("single-use-1", expiration_time_));
  expected.push_back(
      CreateMockBlindSignedAuthToken("single-use-2", expiration_time_));
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(IpProtectionConfigProviderTest, NoTokens) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionConfigProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns malformed tokens.
TEST_P(IpProtectionConfigProviderTest_TokenFormat, MalformedTokens) {
  if (!IsPrivacyPassTokenFormatEnabled()) {
    // We can't detect malformed tokens unless the privacy pass token format is
    // being used.
    return;
  }
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->tokens_ = {{"invalid-token-proto-data", absl::Now() + absl::Hours(1)}};

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionConfigProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 400 error.
TEST_F(IpProtectionConfigProviderTest, BlindSignedTokenError400) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->status_ = absl::InvalidArgumentError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(IpProtectionConfigProvider::kBugBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA400, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 401 error.
TEST_F(IpProtectionConfigProviderTest, BlindSignedTokenError401) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->status_ = absl::UnauthenticatedError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(IpProtectionConfigProvider::kBugBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA401, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 403 error.
TEST_F(IpProtectionConfigProviderTest, BlindSignedTokenError403) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->status_ = absl::PermissionDeniedError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionConfigProvider::kNotEligibleBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA403, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(IpProtectionConfigProviderTest, BlindSignedTokenErrorOther) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->status_ = absl::UnknownError("uhoh");

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyB);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionConfigProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// The CanUseChromeIpProtection capability is not present (`kUnknown`).
TEST_F(IpProtectionConfigProviderTest, AccountCapabilityUnknown) {
  primary_account_behavior_ = PrimaryAccountBehavior::kUnknownEligibility;
  bsa_->tokens_ = {CreateMockBlindSignToken("single-use-1", expiration_time_),
                   CreateMockBlindSignToken("single-use-2", expiration_time_)};

  TryGetAuthTokens(2, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  EXPECT_EQ(bsa_->num_tokens_, 2);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(
      CreateMockBlindSignedAuthToken("single-use-1", expiration_time_));
  expected.push_back(
      CreateMockBlindSignedAuthToken("single-use-2", expiration_time_));
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// Fetching OAuth token returns a transient error.
TEST_F(IpProtectionConfigProviderTest, AuthTokenTransientError) {
  primary_account_behavior_ = PrimaryAccountBehavior::kTokenFetchTransientError;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionConfigProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenTransient, 1);
}

// Fetching OAuth token returns a persistent error.
TEST_F(IpProtectionConfigProviderTest, AuthTokenPersistentError) {
  primary_account_behavior_ =
      PrimaryAccountBehavior::kTokenFetchPersistentError;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedOAuthTokenPersistent, 1);
}

// No primary account.
TEST_F(IpProtectionConfigProviderTest, NoPrimary) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB);

  EXPECT_FALSE(bsa_->get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedNoAccount, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// No primary account initially but this changes when the account status
// changes.
#if !BUILDFLAG(IS_CHROMEOS_ASH)
TEST_F(IpProtectionConfigProviderTest, AccountLoginTriggersBackoffReset) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_FALSE(bsa_->get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(base::TimeDelta::Max());

  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  bsa_->tokens_ = {CreateMockBlindSignToken("single-use-1", expiration_time_)};

  TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyA);

  EXPECT_TRUE(bsa_->get_tokens_called_);
  EXPECT_EQ(bsa_->oauth_token_, "access_token");
  EXPECT_EQ(bsa_->num_tokens_, 1);
  EXPECT_EQ(bsa_->proxy_layer_, quiche::ProxyLayer::kProxyA);
}
#endif

// If the account session token expires and is renewed, the persistent backoff
// should be cleared.
TEST_F(IpProtectionConfigProviderTest, SessionRefreshTriggersBackoffReset) {
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  SetCanUseChromeIpProtectionCapability(true);

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));

  base::test::TestFuture<
      absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>,
      absl::optional<base::Time>>
      tokens_future;
  getter_->TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB,
                            tokens_future.GetCallback());
  const absl::optional<base::Time>& try_again_after =
      tokens_future.Get<absl::optional<base::Time>>();
  ASSERT_TRUE(try_again_after);
  EXPECT_EQ(*try_again_after, base::Time::Max());

  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::State::NONE));

  bsa_->tokens_ = {CreateMockBlindSignToken("single-use-1", expiration_time_)};
  tokens_future.Clear();
  getter_->TryGetAuthTokens(1, network::mojom::IpProtectionProxyLayer::kProxyB,
                            tokens_future.GetCallback());
  identity_test_env_.WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
      "access_token", base::Time::Now());
  const absl::optional<std::vector<network::mojom::BlindSignedAuthTokenPtr>>&
      tokens = tokens_future.Get<absl::optional<
          std::vector<network::mojom::BlindSignedAuthTokenPtr>>>();
  ASSERT_TRUE(tokens);
}

// Backoff calculations.
TEST_F(IpProtectionConfigProviderTest, CalculateBackoff) {
  using enum IpProtectionTryGetAuthTokensResult;

  auto check = [&](IpProtectionTryGetAuthTokensResult result,
                   absl::optional<base::TimeDelta> backoff, bool exponential) {
    SCOPED_TRACE(::testing::Message()
                 << "result: " << static_cast<int>(result));
    EXPECT_EQ(getter_->CalculateBackoff(result), backoff);
    if (backoff && exponential) {
      EXPECT_EQ(getter_->CalculateBackoff(result), (*backoff) * 2);
      EXPECT_EQ(getter_->CalculateBackoff(result), (*backoff) * 4);
    } else {
      EXPECT_EQ(getter_->CalculateBackoff(result), backoff);
    }
  };

  check(kSuccess, absl::nullopt, false);
  check(kFailedNotEligible, getter_->kNotEligibleBackoff, false);
  check(kFailedBSA400, getter_->kBugBackoff, true);
  check(kFailedBSA401, getter_->kBugBackoff, true);
  check(kFailedBSA403, getter_->kNotEligibleBackoff, false);
  check(kFailedBSAOther, getter_->kTransientBackoff, true);
  check(kFailedOAuthTokenTransient, getter_->kTransientBackoff, true);

  check(kFailedNoAccount, base::TimeDelta::Max(), false);
  // The account-related backoffs should not be changed except by account change
  // events.
  check(kFailedBSA400, base::TimeDelta::Max(), false);
  AccountInfo account_info = identity_test_env_.MakePrimaryAccountAvailable(
      kTestEmail, signin::ConsentLevel::kSignin);
  // The backoff time should have been reset.
  check(kFailedBSA400, getter_->kBugBackoff, true);

  check(kFailedOAuthTokenPersistent, base::TimeDelta::Max(), false);
  check(kFailedBSA400, base::TimeDelta::Max(), false);
  // Change the refresh token error state to an error state and then back to a
  // no-error state so that the latter clears the backoff time.
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(
          GoogleServiceAuthError::State::INVALID_GAIA_CREDENTIALS));
  identity_test_env_.UpdatePersistentErrorOfRefreshTokenForAccount(
      account_info.account_id,
      GoogleServiceAuthError(GoogleServiceAuthError::State::NONE));
  check(kFailedBSA400, getter_->kBugBackoff, true);
}

TEST_F(IpProtectionConfigProviderTest, GetProxyListFirstHopHostnames) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {{net::features::kIpPrivacyUseProxyChains.name, "false"}});
  std::vector<std::vector<std::string>> proxy_list = {{"proxy1"}, {"proxy2"}};
  getter_->SetUpForTesting(
      std::make_unique<MockIpProtectionConfigHttp>(proxy_list), bsa_.get());

  base::test::TestFuture<
      const absl::optional<std::vector<std::vector<std::string>>>&>
      proxy_list_future;
  getter_->GetProxyList(proxy_list_future.GetCallback());
  ASSERT_TRUE(proxy_list_future.Wait()) << "GetProxyList did not call back";
  EXPECT_THAT(proxy_list_future.Get(),
              testing::Optional(testing::ElementsAreArray(proxy_list)));
}

TEST_F(IpProtectionConfigProviderTest, GetProxyListProxyChains) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      net::features::kEnableIpProtectionProxy,
      {{net::features::kIpPrivacyUseProxyChains.name, "true"}});
  std::vector<std::vector<std::string>> proxy_list = {{"proxy1"}, {"proxy2"}};
  getter_->SetUpForTesting(
      std::make_unique<MockIpProtectionConfigHttp>(proxy_list), bsa_.get());

  base::test::TestFuture<
      const absl::optional<std::vector<std::vector<std::string>>>&>
      proxy_list_future;
  getter_->GetProxyList(proxy_list_future.GetCallback());
  ASSERT_TRUE(proxy_list_future.Wait()) << "GetProxyList did not call back";
  EXPECT_THAT(proxy_list_future.Get(),
              testing::Optional(testing::ElementsAreArray(proxy_list)));
}

TEST_F(IpProtectionConfigProviderTest, GetProxyListFailure) {
  base::test::TestFuture<
      const absl::optional<std::vector<std::vector<std::string>>>&>
      proxy_list_future;
  getter_->GetProxyList(proxy_list_future.GetCallback());
  ASSERT_TRUE(proxy_list_future.Wait()) << "GetProxyList did not call back";
  EXPECT_EQ(proxy_list_future.Get(), absl::nullopt);
}

// Do a basic check of the token formats.
TEST_P(IpProtectionConfigProviderTest_TokenFormat, TokenFormat) {
  network::mojom::BlindSignedAuthTokenPtr result =
      CreateMockBlindSignedAuthToken("single-use-1", expiration_time_);

  if (IsPrivacyPassTokenFormatEnabled()) {
    EXPECT_TRUE(base::StartsWith((*result).token, "PrivateToken token="));
    EXPECT_NE((*result).token.find("extensions="), std::string::npos);
  } else {
    EXPECT_TRUE(base::StartsWith((*result).token, "Bearer "));
  }
}
