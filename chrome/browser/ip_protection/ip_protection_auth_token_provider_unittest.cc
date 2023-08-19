// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_provider.h"

#include "base/test/metrics/histogram_tester.h"
#include "base/test/test_future.h"
#include "base/time/time.h"
#include "components/signin/public/identity_manager/account_capabilities_test_mutator.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "net/third_party/quiche/src/quiche/blind_sign_auth/blind_sign_auth_interface.h"
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

class MockBlindSignAuth : public quiche::BlindSignAuthInterface {
 public:
  void GetTokens(std::string oauth_token,
                 int num_tokens,
                 quiche::SignedTokenCallback callback) override {
    get_tokens_called_ = true;
    oauth_token_ = oauth_token;
    num_tokens_ = num_tokens;

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

  // If not Ok, the status that will be returned from `GetTokens()`.
  absl::Status status_ = absl::OkStatus();

  // The tokens that will be returned from `GetTokens()` , if `status_` is not
  // `OkStatus`.
  std::vector<quiche::BlindSignToken> tokens_;
};

enum class PrimaryAccountBehavior {
  // Primary account not set.
  kNone,

  // Primary account exists but returns an error fetching access token.
  kTokenFetchError,

  // Primary account exists but is not eligible for IP protection.
  kIneligible,

  // Primary account exists but eligibility is kUnknown.
  kUnknownEligibility,

  // Primary account exists, is eligible, and returns OAuth token
  // "access_token".
  kReturnsToken,
};

}  // namespace

class IpProtectionAuthTokenProviderTest : public testing::Test {
 protected:
  IpProtectionAuthTokenProviderTest()
      : absl_expiration_time_(absl::Now() + absl::Hours(1)),
        base_expiration_time_(
            base::Time::FromTimeT(absl::ToTimeT(absl_expiration_time_))) {}

  // Get the IdentityManager for this test.
  signin::IdentityManager* IdentityManager() {
    return identity_test_env_.identity_manager();
  }

  // Call `TryGetAuthTokens()` and run until it completes.
  void TryGetAuthTokens(int num_tokens, IpProtectionAuthTokenProvider* getter) {
    if (primary_account_behavior_ != PrimaryAccountBehavior::kNone) {
      identity_test_env_.MakePrimaryAccountAvailable(
          kTestEmail, signin::ConsentLevel::kSignin);
    }

    if (primary_account_behavior_ ==
            PrimaryAccountBehavior::kUnknownEligibility ||
        primary_account_behavior_ == PrimaryAccountBehavior::kReturnsToken) {
      SetCanUseChromeIpProtectionCapability(true);
    } else if (primary_account_behavior_ ==
               PrimaryAccountBehavior::kIneligible) {
      SetCanUseChromeIpProtectionCapability(false);
    }

    getter->TryGetAuthTokens(num_tokens, tokens_future_.GetCallback());

    switch (primary_account_behavior_) {
      case PrimaryAccountBehavior::kNone:
      case PrimaryAccountBehavior::kIneligible:
        break;
      case PrimaryAccountBehavior::kTokenFetchError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::CONNECTION_FAILED));
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

  // A convenient expiration time for fake tokens, in the future. These specify
  // the same time with two types.
  absl::Time absl_expiration_time_;
  base::Time base_expiration_time_;

  base::HistogramTester histogram_tester_;
};

// The success case: a primary account is available, and BSA gets a token for
// it.
TEST_F(IpProtectionAuthTokenProviderTest, Success) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);
  bsa.tokens_ = {{"single-use-1", absl_expiration_time_},
                 {"single-use-2", absl_expiration_time_}};

  TryGetAuthTokens(2, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  EXPECT_EQ(bsa.num_tokens_, 2);
  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(network::mojom::BlindSignedAuthToken::New(
      "single-use-1", base_expiration_time_));
  expected.push_back(network::mojom::BlindSignedAuthToken::New(
      "single-use-2", base_expiration_time_));
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// BSA returns no tokens.
TEST_F(IpProtectionAuthTokenProviderTest, NoTokens) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.num_tokens_, 1);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 400 error.
TEST_F(IpProtectionAuthTokenProviderTest, BlindSignedTokenError400) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);
  bsa.status_ = absl::InvalidArgumentError("uhoh");

  TryGetAuthTokens(1, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.num_tokens_, 1);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kBugBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA400, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 401 error.
TEST_F(IpProtectionAuthTokenProviderTest, BlindSignedTokenError401) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  bsa.status_ = absl::UnauthenticatedError("uhoh");
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.num_tokens_, 1);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kBugBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA401, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns a 403 error.
TEST_F(IpProtectionAuthTokenProviderTest, BlindSignedTokenError403) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  bsa.status_ = absl::PermissionDeniedError("uhoh");
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.num_tokens_, 1);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kNotEligibleBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSA403, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// BSA returns some other error.
TEST_F(IpProtectionAuthTokenProviderTest, BlindSignedTokenErrorOther) {
  primary_account_behavior_ = PrimaryAccountBehavior::kReturnsToken;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  bsa.status_ = absl::UnknownError("uhoh");
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.num_tokens_, 1);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedBSAOther, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// The CanUseChromeIpProtection capability is not present (`kUnknown`).
TEST_F(IpProtectionAuthTokenProviderTest, AccountCapabilityUnknown) {
  primary_account_behavior_ = PrimaryAccountBehavior::kUnknownEligibility;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  bsa.tokens_ = {{"single-use-1", absl_expiration_time_},
                 {"single-use-2", absl_expiration_time_}};
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(2, &getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  EXPECT_EQ(bsa.num_tokens_, 2);
  std::vector<network::mojom::BlindSignedAuthTokenPtr> expected;
  expected.push_back(network::mojom::BlindSignedAuthToken::New(
      "single-use-1", base_expiration_time_));
  expected.push_back(network::mojom::BlindSignedAuthToken::New(
      "single-use-2", base_expiration_time_));
  ExpectTryGetAuthTokensResult(std::move(expected));
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kSuccess, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 1);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 1);
}

// Fetching OAuth token returns an error.
TEST_F(IpProtectionAuthTokenProviderTest, AuthTokenError) {
  primary_account_behavior_ = PrimaryAccountBehavior::kTokenFetchError;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_FALSE(bsa.get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kTransientBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedOAuthToken, 1);
}

// No primary account.
TEST_F(IpProtectionAuthTokenProviderTest, NoPrimary) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;
  auto bsa = MockBlindSignAuth();
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());
  getter.SetBlindSignAuthInterfaceForTesting(&bsa);

  TryGetAuthTokens(1, &getter);

  EXPECT_FALSE(bsa.get_tokens_called_);
  ExpectTryGetAuthTokensResultFailed(
      IpProtectionAuthTokenProvider::kNoAccountBackoff);
  histogram_tester_.ExpectUniqueSample(
      kTryGetAuthTokensResultHistogram,
      IpProtectionTryGetAuthTokensResult::kFailedNoAccount, 1);
  histogram_tester_.ExpectTotalCount(kOAuthTokenFetchHistogram, 0);
  histogram_tester_.ExpectTotalCount(kTokenBatchHistogram, 0);
}

// Backoff calculations.
TEST_F(IpProtectionAuthTokenProviderTest, CalculateBackoff) {
  using enum IpProtectionTryGetAuthTokensResult;
  IpProtectionAuthTokenProvider getter(
      IdentityManager(),
      base::MakeRefCounted<network::TestSharedURLLoaderFactory>());

  auto check = [&](IpProtectionTryGetAuthTokensResult result,
                   absl::optional<base::TimeDelta> backoff, bool exponential) {
    EXPECT_EQ(getter.CalculateBackoff(result), backoff);
    if (backoff && exponential) {
      EXPECT_EQ(getter.CalculateBackoff(result), (*backoff) * 2);
      EXPECT_EQ(getter.CalculateBackoff(result), (*backoff) * 4);
    } else {
      EXPECT_EQ(getter.CalculateBackoff(result), backoff);
    }
  };

  check(kSuccess, absl::nullopt, false);
  check(kFailedNoAccount, getter.kNoAccountBackoff, false);
  check(kFailedNotEligible, getter.kNotEligibleBackoff, false);
  check(kFailedOAuthToken, getter.kTransientBackoff, true);
  check(kFailedBSA400, getter.kBugBackoff, true);
  check(kFailedBSA401, getter.kBugBackoff, true);
  check(kFailedBSA403, getter.kNotEligibleBackoff, false);
  check(kFailedBSAOther, getter.kTransientBackoff, true);
}
