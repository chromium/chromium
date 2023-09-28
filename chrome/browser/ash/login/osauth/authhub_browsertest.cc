// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <string>

#include "base/memory/raw_ptr.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/login_manager_test.h"
#include "chrome/browser/ash/login/test/cryptohome_mixin.h"
#include "chrome/browser/ash/login/test/login_manager_mixin.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "chromeos/ash/components/osauth/public/auth_engine_api.h"
#include "chromeos/ash/components/osauth/public/auth_hub.h"
#include "chromeos/ash/components/osauth/public/auth_session_storage.h"
#include "chromeos/ash/components/osauth/public/common_types.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_attempt_consumer.h"
#include "chromeos/ash/components/osauth/test_support/mock_auth_factor_status_consumer.h"
#include "content/public/test/browser_test.h"

namespace ash {

using testing::_;
using testing::AnyNumber;
using testing::Invoke;

// This test currently uses mocks as consumers.
// Once there is an AuthPanel implementation / In-Session auth dialog,
// test should be migrated to use that instead.
class AuthHubTest : public LoginManagerTest {
 public:
  AuthHubTest() {
    login_manager_.AppendRegularUsers(2);
    test_account_id_ = login_manager_.users()[0].account_id;
  }
  ~AuthHubTest() override = default;

  void ExpectAuthenticationStarted() {
    EXPECT_CALL(attempt_consumer_, OnUserAuthAttemptConfirmed(_, _))
        .WillOnce(Invoke([&](AuthHubConnector* connector,
                             raw_ptr<AuthFactorStatusConsumer>& out_consumer) {
          connector_ = connector;
          out_consumer = &status_consumer_;
        }));
    EXPECT_CALL(status_consumer_, InitializeUi(_, _))
        .WillOnce(
            Invoke([&](AuthFactorsSet factors, AuthHubConnector* connector) {
              connector_ = connector;
            }));
    EXPECT_CALL(status_consumer_, OnFactorStatusesChanged(_))
        .Times(AnyNumber());
  }

  void ExpectSuccessfulAuthentication() {
    EXPECT_CALL(status_consumer_, OnFactorAuthSuccess(_));
    EXPECT_CALL(attempt_consumer_, OnUserAuthSuccess(_, _))
        .WillOnce(Invoke([&](AshAuthFactor factor, AuthProofToken token) {
          auth_token_ = token;
        }));
  }

 protected:
  MockAuthAttemptConsumer attempt_consumer_;
  raw_ptr<AuthHubConnector, DanglingUntriaged | ExperimentalAsh> connector_;
  MockAuthFactorStatusConsumer status_consumer_;
  absl::optional<AuthProofToken> auth_token_;

  AccountId test_account_id_;
  CryptohomeMixin cryptohome_mixin_{&mixin_host_};
  LoginManagerMixin login_manager_{&mixin_host_,
                                   {},
                                   nullptr,
                                   &cryptohome_mixin_};
};

IN_PROC_BROWSER_TEST_F(AuthHubTest, LoginScreenWithPasswordOnly) {
  // We're on the login screen.
  ash::AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  base::test::TestFuture<void> future;
  ash::AuthHub::Get()->EnsureInitialized(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // User has a password as a factor.
  cryptohome_mixin_.AddGaiaPassword(test_account_id_, "password");

  // User pod is selected.
  ExpectAuthenticationStarted();
  ash::AuthHub::Get()->StartAuthentication(
      test_account_id_, AuthPurpose::kLogin, &attempt_consumer_);
  base::RunLoop().RunUntilIdle();

  // Password is entered.
  ExpectSuccessfulAuthentication();
  AuthEngineApi::AuthenticateWithPassword(
      connector_, AshAuthFactor::kGaiaPassword, "password");
  base::RunLoop().RunUntilIdle();

  // Check that authentication was successful.
  EXPECT_TRUE(auth_token_.has_value());
  EXPECT_TRUE(AuthSessionStorage::Get()->IsValid(*auth_token_));
  std::unique_ptr<UserContext> context =
      AuthSessionStorage::Get()->BorrowForTests(FROM_HERE, *auth_token_);
  ASSERT_NE(context.get(), nullptr);
  EXPECT_EQ(context->GetAuthorizedIntents(),
            AuthSessionIntents{AuthSessionIntent::kDecrypt});
  EXPECT_EQ(context->GetAccountId(), test_account_id_);

  // Check that authhub can correctly switch to in-session after that.
  ash::AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
  base::RunLoop().RunUntilIdle();
}

IN_PROC_BROWSER_TEST_F(AuthHubTest, LoginScreenAuthenticateWithPin) {
  // We're on the login screen.
  ash::AuthHub::Get()->InitializeForMode(AuthHubMode::kLoginScreen);
  base::test::TestFuture<void> future;
  ash::AuthHub::Get()->EnsureInitialized(future.GetCallback());
  ASSERT_TRUE(future.Wait());

  // User has a password as a factor.
  cryptohome_mixin_.AddGaiaPassword(test_account_id_, "password");

  // User has a PIN as a factor.
  cryptohome_mixin_.AddCryptohomePin(test_account_id_, "123123");

  ASSERT_TRUE(cryptohome_mixin_.HasPinFactor(test_account_id_));

  // User pod is selected.
  ExpectAuthenticationStarted();
  ash::AuthHub::Get()->StartAuthentication(
      test_account_id_, AuthPurpose::kLogin, &attempt_consumer_);
  base::RunLoop().RunUntilIdle();

  // Pin is entered.
  ExpectSuccessfulAuthentication();
  AuthEngineApi::AuthenticateWithPin(connector_, AshAuthFactor::kCryptohomePin,
                                     "123123");
  base::RunLoop().RunUntilIdle();

  // Check that authentication was successful.
  EXPECT_TRUE(auth_token_.has_value());
  EXPECT_TRUE(AuthSessionStorage::Get()->IsValid(*auth_token_));
  std::unique_ptr<UserContext> context =
      AuthSessionStorage::Get()->BorrowForTests(FROM_HERE, *auth_token_);
  ASSERT_NE(context.get(), nullptr);
  EXPECT_EQ(context->GetAuthorizedIntents(),
            AuthSessionIntents{AuthSessionIntent::kDecrypt});
  EXPECT_EQ(context->GetAccountId(), test_account_id_);

  // Check that authhub can correctly switch to in-session after that.
  ash::AuthHub::Get()->InitializeForMode(AuthHubMode::kInSession);
  base::RunLoop().RunUntilIdle();
}

}  // namespace ash
