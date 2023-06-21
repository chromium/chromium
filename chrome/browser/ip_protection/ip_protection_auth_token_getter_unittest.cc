// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ip_protection/ip_protection_auth_token_getter.h"
#include "base/test/test_future.h"
#include "components/signin/public/identity_manager/identity_test_environment.h"
#include "content/public/browser/browser_thread.h"
#include "content/public/test/browser_task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class MockBlindSignAuth : public quiche::BlindSignAuthInterface {
 public:
  using BlindSignTokenCallback =
      std::function<void(absl::StatusOr<absl::Span<quiche::BlindSignToken>>)>;
  void GetTokens(absl::string_view oauth_token,
                 int num_tokens,
                 BlindSignTokenCallback callback) override {
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
            [](BlindSignTokenCallback callback,
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

  // Primary account exists and returns OAuth token "access_token".
  kExists,
};

}  // namespace

class IpProtectionAuthTokenGetterTest : public testing::Test {
 protected:
  using TokensResult = const absl::optional<std::vector<std::string>>;
  // Get the IdentityManager for this test.
  signin::IdentityManager* IdentityManager() {
    return identity_test_env_.identity_manager();
  }

  // Call `TryGetAuthTokens()` with `TokensCallback()` and run until it
  // completes.
  void TryGetAuthTokens(int num_tokens, IpProtectionAuthTokenGetter& getter) {
    if (primary_account_behavior_ != PrimaryAccountBehavior::kNone) {
      identity_test_env_.MakePrimaryAccountAvailable(
          "test@example.com", signin::ConsentLevel::kSignin);
    }

    getter.TryGetAuthTokens(num_tokens, tokens_future_.GetCallback());

    switch (primary_account_behavior_) {
      case PrimaryAccountBehavior::kNone:
        break;
      case PrimaryAccountBehavior::kTokenFetchError:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithError(
                GoogleServiceAuthError(
                    GoogleServiceAuthError::CONNECTION_FAILED));
        break;
      case PrimaryAccountBehavior::kExists:
        identity_test_env_
            .WaitForAccessTokenRequestIfNecessaryAndRespondWithToken(
                "access_token", base::Time::Now());
        break;
    }
    ASSERT_TRUE(tokens_future_.Wait()) << "TryGetAuthTokens did not call back";
  }

  // The behavior of the identity manager.
  PrimaryAccountBehavior primary_account_behavior_ =
      PrimaryAccountBehavior::kExists;

  // Run on the UI thread.
  content::BrowserTaskEnvironment task_environment_;
  base::test::TestFuture<TokensResult&> tokens_future_;

  // Test environment for IdentityManager. This must come after the
  // TaskEnvironment.
  signin::IdentityTestEnvironment identity_test_env_;
};

// The success case: a primary account is available, and BSA gets a token for
// it.
TEST_F(IpProtectionAuthTokenGetterTest, Success) {
  primary_account_behavior_ = PrimaryAccountBehavior::kExists;
  auto bsa = MockBlindSignAuth();
  auto getter =
      IpProtectionAuthTokenGetter::CreateForTesting(IdentityManager(), &bsa);
  bsa.tokens_ = {{"single-use-1", absl::Now()}, {"single-use-2", absl::Now()}};

  TryGetAuthTokens(2, getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  EXPECT_EQ(bsa.num_tokens_, 2);
  std::vector<std::string> expected = {"single-use-1", "single-use-2"};
  EXPECT_EQ(*tokens_future_.Get(), expected);
}

// BSA returns no tokens.
TEST_F(IpProtectionAuthTokenGetterTest, NoTokens) {
  primary_account_behavior_ = PrimaryAccountBehavior::kExists;
  auto bsa = MockBlindSignAuth();
  auto getter =
      IpProtectionAuthTokenGetter::CreateForTesting(IdentityManager(), &bsa);

  TryGetAuthTokens(1, getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  EXPECT_EQ(tokens_future_.Get(), absl::nullopt);
}

// BSA returns an error.
TEST_F(IpProtectionAuthTokenGetterTest, BlindSignedTokenError) {
  primary_account_behavior_ = PrimaryAccountBehavior::kExists;
  auto bsa = MockBlindSignAuth();
  auto getter =
      IpProtectionAuthTokenGetter::CreateForTesting(IdentityManager(), &bsa);
  bsa.status_ = absl::NotFoundError("uhoh");

  TryGetAuthTokens(1, getter);

  EXPECT_TRUE(bsa.get_tokens_called_);
  EXPECT_EQ(bsa.oauth_token_, "access_token");
  EXPECT_EQ(tokens_future_.Get(), absl::nullopt);
}

// Fetching OAuth token returns an error.
TEST_F(IpProtectionAuthTokenGetterTest, AuthTokenError) {
  primary_account_behavior_ = PrimaryAccountBehavior::kTokenFetchError;
  auto bsa = MockBlindSignAuth();
  auto getter =
      IpProtectionAuthTokenGetter::CreateForTesting(IdentityManager(), &bsa);

  TryGetAuthTokens(1, getter);

  EXPECT_FALSE(bsa.get_tokens_called_);
  EXPECT_EQ(tokens_future_.Get(), absl::nullopt);
}

// No primary account.
TEST_F(IpProtectionAuthTokenGetterTest, NoPrimary) {
  primary_account_behavior_ = PrimaryAccountBehavior::kNone;
  auto bsa = MockBlindSignAuth();
  auto getter =
      IpProtectionAuthTokenGetter::CreateForTesting(IdentityManager(), &bsa);

  TryGetAuthTokens(1, getter);

  EXPECT_FALSE(bsa.get_tokens_called_);
  EXPECT_EQ(tokens_future_.Get(), absl::nullopt);
}
