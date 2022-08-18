// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_session_authenticator.h"

#include <map>
#include <memory>
#include <string>
#include <utility>

#include "ash/components/login/auth/mock_auth_status_consumer.h"
#include "ash/components/login/auth/mock_safe_mode_delegate.h"
#include "ash/components/login/auth/public/auth_failure.h"
#include "ash/components/login/auth/public/cryptohome_key_constants.h"
#include "ash/components/login/auth/public/key.h"
#include "ash/components/login/auth/public/user_context.h"
#include "base/callback_helpers.h"
#include "base/check.h"
#include "base/memory/scoped_refptr.h"
#include "base/test/gmock_callback_support.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/cryptohome/system_salt_getter.h"
#include "chromeos/ash/components/dbus/cryptohome/UserDataAuth.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/key.pb.h"
#include "chromeos/ash/components/dbus/cryptohome/rpc.pb.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user_type.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using cryptohome::KeyData;
using testing::_;
using testing::AllOf;
using testing::AtMost;
using user_data_auth::AddCredentialsReply;
using user_data_auth::AUTH_SESSION_FLAGS_EPHEMERAL_USER;
using user_data_auth::AUTH_SESSION_FLAGS_NONE;
using user_data_auth::AuthenticateAuthSessionReply;
using user_data_auth::CreatePersistentUserReply;
using user_data_auth::PrepareEphemeralVaultReply;
using user_data_auth::PrepareGuestVaultReply;
using user_data_auth::PreparePersistentVaultReply;
using user_data_auth::RemoveReply;
using user_data_auth::StartAuthSessionReply;

namespace ash {

namespace {

constexpr char kEmail[] = "fake-email@example.com";
constexpr char kPassword[] = "pass";
constexpr char kFirstAuthSessionId[] = "123";
constexpr char kSecondAuthSessionId[] = "456";

// Matchers that verify the given cryptohome ...Request protobuf has the
// expected auth_session_id.
MATCHER(WithFirstAuthSessionId, "") {
  return arg.auth_session_id() == kFirstAuthSessionId;
}

MATCHER(WithSecondAuthSessionId, "") {
  return arg.auth_session_id() == kSecondAuthSessionId;
}

// Matcher for `StartAuthSessionRequest` that checks its account_id and flags.
MATCHER_P(WithAccountIdAndFlags, flags, "") {
  return arg.account_id().account_id() == kEmail &&
         arg.flags() == static_cast<unsigned>(flags);
}

// Matchers for `AuthenticateAuthSessionRequest` and `AddCredentialsRequest`
// that verify the key properties.
MATCHER_P(WithPasswordKey, expected_label, "") {
  if (arg.authorization().key().data().type() != KeyData::KEY_TYPE_PASSWORD ||
      arg.authorization().key().data().label() != expected_label) {
    return false;
  }
  // Validate the password is already hashed here.
  EXPECT_NE(arg.authorization().key().secret(), "");
  EXPECT_NE(arg.authorization().key().secret(), kPassword);
  return true;
}

MATCHER(WithKioskKey, "") {
  return arg.authorization().key().data().type() == KeyData::KEY_TYPE_KIOSK &&
         arg.authorization().key().data().label() ==
             kCryptohomePublicMountLabel;
}

// GMock action that runs the callback (which is expected to be the second
// argument in the mocked function) with the given reply.
template <typename ReplyType>
auto ReplyWith(const ReplyType& reply) {
  return base::test::RunOnceCallback<1>(reply);
}

StartAuthSessionReply BuildStartReply(
    const std::string& auth_session_id,
    bool user_exists,
    const std::map<std::string, KeyData>& keys) {
  StartAuthSessionReply reply;
  reply.set_auth_session_id(auth_session_id);
  reply.set_user_exists(user_exists);
  for (const auto& [key, data] : keys)
    (*reply.mutable_key_label_data())[key] = data;
  return reply;
}

AuthenticateAuthSessionReply BuildAuthenticateSuccessReply() {
  AuthenticateAuthSessionReply reply;
  reply.set_authenticated(true);
  return reply;
}

AuthenticateAuthSessionReply BuildAuthenticateFailureReply() {
  AuthenticateAuthSessionReply reply;
  reply.set_authenticated(false);
  reply.set_error(user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_FAILED);
  reply.mutable_error_info()->set_primary_action(
      user_data_auth::PRIMARY_INCORRECT_AUTH);
  return reply;
}

}  // namespace

class AuthSessionAuthenticatorTest : public ::testing::Test {
 protected:
  const AccountId kAccountId = AccountId::FromUserEmail(kEmail);

  AuthSessionAuthenticatorTest() {
    CryptohomeMiscClient::InitializeFake();
    chromeos::SystemSaltGetter::Initialize();
    EXPECT_CALL(auth_status_consumer_, OnAuthSuccess(_))
        .Times(AtMost(1))
        .WillOnce([this](const UserContext& user_context) {
          on_auth_success_future_.SetValue(user_context);
        });
    EXPECT_CALL(auth_status_consumer_, OnAuthFailure(_))
        .Times(AtMost(1))
        .WillOnce([this](const AuthFailure& error) {
          on_auth_failure_future_.SetValue(error);
        });
    EXPECT_CALL(auth_status_consumer_, OnPasswordChangeDetected(_))
        .Times(AtMost(1))
        .WillOnce([this](const UserContext& user_context) {
          on_password_change_detected_future_.SetValue(user_context);
        });
    EXPECT_CALL(auth_status_consumer_, OnOffTheRecordAuthSuccess())
        .Times(AtMost(1))
        .WillOnce([this]() {
          on_off_the_record_auth_success_future_.SetValue(true);
        });
  }

  ~AuthSessionAuthenticatorTest() override {
    chromeos::SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

  void CreateAuthenticator(bool is_ephemeral_mount_enforced) {
    auto owned_safe_mode_delegate = std::make_unique<MockSafeModeDelegate>();
    safe_mode_delegate_ = owned_safe_mode_delegate.get();
    authenticator_ = base::MakeRefCounted<AuthSessionAuthenticator>(
        &auth_status_consumer_, std::move(owned_safe_mode_delegate),
        /*user_recorder=*/base::DoNothing(), is_ephemeral_mount_enforced);
  }

  MockUserDataAuthClient& userdataauth() { return userdataauth_; }

  Authenticator& authenticator() {
    DCHECK(authenticator_);
    return *authenticator_;
  }

  base::test::TestFuture<UserContext>& on_auth_success_future() {
    return on_auth_success_future_;
  }
  base::test::TestFuture<AuthFailure>& on_auth_failure_future() {
    return on_auth_failure_future_;
  }
  base::test::TestFuture<UserContext>& on_password_change_detected_future() {
    return on_password_change_detected_future_;
  }
  base::test::TestFuture<bool>& on_off_the_record_auth_success_future() {
    return on_off_the_record_auth_success_future_;
  }

 private:
  base::test::SingleThreadTaskEnvironment task_environment_;
  base::test::TestFuture<UserContext> on_auth_success_future_;
  base::test::TestFuture<AuthFailure> on_auth_failure_future_;
  base::test::TestFuture<UserContext> on_password_change_detected_future_;
  base::test::TestFuture<bool> on_off_the_record_auth_success_future_;
  MockUserDataAuthClient userdataauth_;
  MockAuthStatusConsumer auth_status_consumer_{
      /*quit_closure=*/base::DoNothing()};
  scoped_refptr<AuthSessionAuthenticator> authenticator_;
  // Unowned (points to the object owned by `authenticator_`).
  MockSafeModeDelegate* safe_mode_delegate_ = nullptr;
};

// Test the `CompleteLogin()` method in the new regular user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginRegularNew) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false,
                                          /*keys=*/{})));
  EXPECT_CALL(userdataauth(), CreatePersistentUser(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(CreatePersistentUserReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));
  EXPECT_CALL(userdataauth(),
              AddCredentials(AllOf(WithFirstAuthSessionId(),
                                   WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                             _))
      .WillOnce(ReplyWith(AddCredentialsReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the existing regular user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginRegularExisting) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(
          BuildStartReply(kFirstAuthSessionId, /*user_exists=*/true,
                          /*keys=*/{{kCryptohomeGaiaKeyLabel, KeyData()}})));
  EXPECT_CALL(
      userdataauth(),
      AuthenticateAuthSession(AllOf(WithFirstAuthSessionId(),
                                    WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                              _))
      .WillOnce(ReplyWith(BuildAuthenticateSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the password change scenario for the
// existing regular user.
TEST_F(AuthSessionAuthenticatorTest,
       CompleteLoginRegularExistingPasswordChange) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(
          BuildStartReply(kFirstAuthSessionId,
                          /*user_exists=*/true,
                          /*keys=*/{{kCryptohomeGaiaKeyLabel, KeyData()}})));
  // Set up the cryptohome authentication request to return a failure, since
  // we're simulating the case when it only knows about the old password.
  EXPECT_CALL(
      userdataauth(),
      AuthenticateAuthSession(AllOf(WithFirstAuthSessionId(),
                                    WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                              _))
      .WillOnce(ReplyWith(BuildAuthenticateFailureReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context =
      on_password_change_detected_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the ephemeral user scenario.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginEphemeral) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(userdataauth(),
              StartAuthSession(
                  WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false, /*keys=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
  EXPECT_CALL(userdataauth(),
              AddCredentials(AllOf(WithFirstAuthSessionId(),
                                   WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                             _))
      .WillOnce(ReplyWith(AddCredentialsReply()));

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `CompleteLogin()` method in the scenario when an ephemeral login is
// requested while having stale persistent data for the same user.
TEST_F(AuthSessionAuthenticatorTest, CompleteLoginEphemeralStaleData) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  {
    testing::InSequence seq;
    EXPECT_CALL(
        userdataauth(),
        StartAuthSession(
            WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
        .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                            /*user_exists=*/true, /*keys=*/{})))
        .RetiresOnSaturation();
    EXPECT_CALL(userdataauth(), Remove(WithFirstAuthSessionId(), _))
        .WillOnce(ReplyWith(RemoveReply()));
    EXPECT_CALL(
        userdataauth(),
        StartAuthSession(
            WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
        .WillOnce(ReplyWith(BuildStartReply(
            kSecondAuthSessionId, /*user_exists=*/false, /*keys=*/{})));
    EXPECT_CALL(userdataauth(),
                PrepareEphemeralVault(WithSecondAuthSessionId(), _))
        .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
    EXPECT_CALL(userdataauth(),
                AddCredentials(AllOf(WithSecondAuthSessionId(),
                                     WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                               _))
        .WillOnce(ReplyWith(AddCredentialsReply()));
  }

  // Act.
  authenticator().CompleteLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kSecondAuthSessionId);
}

// Test the `AuthenticateToLogin()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToLogin) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(
          BuildStartReply(kFirstAuthSessionId,
                          /*user_exists=*/true,
                          /*keys=*/{{kCryptohomeGaiaKeyLabel, KeyData()}})));
  EXPECT_CALL(
      userdataauth(),
      AuthenticateAuthSession(AllOf(WithFirstAuthSessionId(),
                                    WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                              _))
      .WillOnce(ReplyWith(BuildAuthenticateSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().AuthenticateToLogin(std::move(user_context));
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `AuthenticateToLogin()` method in the authentication failure
// scenario.
TEST_F(AuthSessionAuthenticatorTest, AuthenticateToLoginAuthFailure) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  auto user_context = std::make_unique<UserContext>(
      user_manager::USER_TYPE_REGULAR, kAccountId);
  user_context->SetKey(Key(kPassword));
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(
          BuildStartReply(kFirstAuthSessionId, /*user_exists=*/true,
                          /*keys=*/{{kCryptohomeGaiaKeyLabel, KeyData()}})));
  EXPECT_CALL(
      userdataauth(),
      AuthenticateAuthSession(AllOf(WithFirstAuthSessionId(),
                                    WithPasswordKey(kCryptohomeGaiaKeyLabel)),
                              _))
      .WillOnce(ReplyWith(BuildAuthenticateFailureReply()));

  // Act.
  authenticator().AuthenticateToLogin(std::move(user_context));
  const AuthFailure auth_failure = on_auth_failure_future().Get();

  // Assert.
  EXPECT_EQ(auth_failure.reason(), AuthFailure::COULD_NOT_MOUNT_CRYPTOHOME);
}

// Test the `LoginOffTheRecord()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginOffTheRecord) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  EXPECT_CALL(userdataauth(), PrepareGuestVault(_, _))
      .WillOnce(ReplyWith(PrepareGuestVaultReply()));

  // Act.
  authenticator().LoginOffTheRecord();
  EXPECT_TRUE(on_off_the_record_auth_success_future().Wait());
}

// Test the `LoginAsPublicSession()` method in the successful scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginAsPublicSession) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  UserContext user_context(user_manager::USER_TYPE_PUBLIC_ACCOUNT, kAccountId);
  EXPECT_CALL(userdataauth(),
              StartAuthSession(
                  WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false, /*keys=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));

  // Act.
  authenticator().LoginAsPublicSession(user_context);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when the kiosk
// homedir needs to be created.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountNew) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(
          ReplyWith(BuildStartReply(kFirstAuthSessionId, /*user_exists=*/false,
                                    /*keys=*/{})));
  EXPECT_CALL(userdataauth(), CreatePersistentUser(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(CreatePersistentUserReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));
  EXPECT_CALL(
      userdataauth(),
      AddCredentials(AllOf(WithFirstAuthSessionId(), WithKioskKey()), _))
      .WillOnce(ReplyWith(AddCredentialsReply()));

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when the kiosk
// homedir already exists.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountExisting) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/false);
  KeyData key_data;
  key_data.set_type(KeyData::KEY_TYPE_KIOSK);
  EXPECT_CALL(
      userdataauth(),
      StartAuthSession(WithAccountIdAndFlags(AUTH_SESSION_FLAGS_NONE), _))
      .WillOnce(ReplyWith(
          BuildStartReply(kFirstAuthSessionId, /*user_exists=*/true,
                          /*keys=*/{{kCryptohomePublicMountLabel, key_data}})));
  EXPECT_CALL(userdataauth(),
              AuthenticateAuthSession(
                  AllOf(WithFirstAuthSessionId(), WithKioskKey()), _))
      .WillOnce(ReplyWith(BuildAuthenticateSuccessReply()));
  EXPECT_CALL(userdataauth(),
              PreparePersistentVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PreparePersistentVaultReply()));

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the ephemeral kiosk scenario.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountEphemeral) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  EXPECT_CALL(userdataauth(),
              StartAuthSession(
                  WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
      .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                          /*user_exists=*/false, /*keys=*/{})));
  EXPECT_CALL(userdataauth(),
              PrepareEphemeralVault(WithFirstAuthSessionId(), _))
      .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kFirstAuthSessionId);
}

// Test the `LoginAsKioskAccount()` method in the scenario when an ephemeral
// kiosk is requested while having stale persistent data for the same user.
TEST_F(AuthSessionAuthenticatorTest, LoginAsKioskAccountEphemeralStaleData) {
  // Arrange.
  CreateAuthenticator(/*is_ephemeral_mount_enforced=*/true);
  {
    testing::InSequence seq;
    EXPECT_CALL(
        userdataauth(),
        StartAuthSession(
            WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
        .WillOnce(ReplyWith(BuildStartReply(kFirstAuthSessionId,
                                            /*user_exists=*/true, /*keys=*/{})))
        .RetiresOnSaturation();
    EXPECT_CALL(userdataauth(), Remove(WithFirstAuthSessionId(), _))
        .WillOnce(ReplyWith(RemoveReply()));
    EXPECT_CALL(
        userdataauth(),
        StartAuthSession(
            WithAccountIdAndFlags(AUTH_SESSION_FLAGS_EPHEMERAL_USER), _))
        .WillOnce(ReplyWith(BuildStartReply(
            kSecondAuthSessionId, /*user_exists=*/false, /*keys=*/{})));
    EXPECT_CALL(userdataauth(),
                PrepareEphemeralVault(WithSecondAuthSessionId(), _))
        .WillOnce(ReplyWith(PrepareEphemeralVaultReply()));
  }

  // Act.
  authenticator().LoginAsKioskAccount(kAccountId);
  const UserContext got_user_context = on_auth_success_future().Get();

  // Assert.
  EXPECT_EQ(got_user_context.GetAccountId(), kAccountId);
  EXPECT_EQ(got_user_context.GetAuthSessionId(), kSecondAuthSessionId);
}

}  // namespace ash
