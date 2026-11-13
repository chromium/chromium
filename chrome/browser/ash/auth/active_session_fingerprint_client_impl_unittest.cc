// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/auth/active_session_fingerprint_client_impl.h"

#include <memory>
#include <optional>
#include <utility>

#include "ash/public/cpp/auth/active_session_fingerprint_client.h"
#include "base/functional/callback_helpers.h"
#include "base/scoped_observation.h"
#include "base/test/repeating_test_future.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/cryptohome_parameters.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/authentication_error.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "components/account_id/account_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

class NoOpFingerprintAuthObserver
    : public UserDataAuthClient::FingerprintAuthObserver {
 public:
  NoOpFingerprintAuthObserver() {
    observation_.Observe(UserDataAuthClient::Get());
  }
  ~NoOpFingerprintAuthObserver() override = default;

 private:
  base::ScopedObservation<UserDataAuthClient,
                          UserDataAuthClient::FingerprintAuthObserver>
      observation_{this};
};

class ActiveSessionFingerprintClientImplTest : public testing::Test {
 protected:
  ActiveSessionFingerprintClientImplTest() {
    UserDataAuthClient::InitializeFake();
    client_ = std::make_unique<ActiveSessionFingerprintClientImpl>();
  }

  ~ActiveSessionFingerprintClientImplTest() override {
    client_.reset();
    UserDataAuthClient::Shutdown();
  }

  base::test::SingleThreadTaskEnvironment task_environment_;
  std::unique_ptr<ActiveSessionFingerprintClientImpl> client_;
};

TEST_F(ActiveSessionFingerprintClientImplTest,
       PrepareFingerprintAuthFailedThenSucceeded) {
  // FakeUserDataAuthClient::PrepareAuthFactor requires at least one observer in
  // its list before preparing a legacy fingerprint factor.
  NoOpFingerprintAuthObserver dummy_observer;

  // First attempt: fails because the auth session ID does not exist.
  auto failed_context = std::make_unique<UserContext>();
  failed_context->SetAuthSessionIds("invalid-session-id", "broadcast-id");

  base::test::TestFuture<std::unique_ptr<UserContext>,
                         std::optional<AuthenticationError>>
      first_prepare_future;
  client_->PrepareFingerprintAuth(std::move(failed_context),
                                  first_prepare_future.GetCallback(),
                                  base::DoNothing());

  auto [returned_failed_context, first_error] = first_prepare_future.Take();
  EXPECT_TRUE(first_error.has_value());

  // Second attempt: succeeds with a valid auth session and does not crash from
  // adding the observer a second time.
  const AccountId account_id = AccountId::FromUserEmail("test@example.com");
  const cryptohome::AccountIdentifier account_identifier =
      cryptohome::CreateAccountIdentifierFromAccountId(account_id);
  FakeUserDataAuthClient::TestApi::Get()->AddExistingUser(account_identifier);
  auto [session_id, broadcast_id] =
      FakeUserDataAuthClient::TestApi::Get()->AddSession(
          account_identifier, /*authenticated=*/false);

  auto valid_context = std::make_unique<UserContext>();
  valid_context->SetAuthSessionIds(session_id, broadcast_id);

  base::test::TestFuture<std::unique_ptr<UserContext>,
                         std::optional<AuthenticationError>>
      second_prepare_future;
  base::test::RepeatingTestFuture<FingerprintAuthScanResult> scan_future;
  client_->PrepareFingerprintAuth(std::move(valid_context),
                                  second_prepare_future.GetCallback(),
                                  scan_future.GetCallback());

  auto [prepared_context, second_error] = second_prepare_future.Take();
  EXPECT_FALSE(second_error.has_value());

  FakeUserDataAuthClient::TestApi::Get()->SendLegacyFPAuthSignal(
      user_data_auth::FINGERPRINT_SCAN_RESULT_SUCCESS);
  EXPECT_EQ(scan_future.Take(), FingerprintAuthScanResult::kSuccess);

  base::test::TestFuture<std::unique_ptr<UserContext>,
                         std::optional<AuthenticationError>>
      terminate_future;
  client_->TerminateFingerprintAuth(std::move(prepared_context),
                                    terminate_future.GetCallback());
  auto [terminated_context, terminate_error] = terminate_future.Take();
  EXPECT_FALSE(terminate_error.has_value());
}

}  // namespace
}  // namespace ash
