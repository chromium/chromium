// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/components/login/auth/auth_performer.h"

#include <memory>

#include "ash/components/cryptohome/cryptohome_parameters.h"
#include "ash/components/cryptohome/system_salt_getter.h"
#include "ash/components/login/auth/auth_factors_data.h"
#include "ash/components/login/auth/user_context.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/ash/components/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/ash/components/dbus/userdataauth/userdataauth_client.h"
#include "chromeos/dbus/cryptohome/UserDataAuth.pb.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;

namespace ash {
namespace {

void SetupUserWithLegacyPassword(UserContext* context) {
  std::vector<cryptohome::KeyDefinition> keys;
  keys.push_back(cryptohome::KeyDefinition::CreateForPassword(
      "secret", "legacy-0", /*privileges=*/0));
  AuthFactorsData data(keys);
  context->SetAuthFactorsData(data);
}

void ReplyAsSuccess(
    UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
  ::user_data_auth::AuthenticateAuthSessionReply reply;
  reply.set_error(::user_data_auth::CRYPTOHOME_ERROR_NOT_SET);
  reply.set_authenticated(true);
  std::move(callback).Run(reply);
}

void ReplyAsKeyMismatch(
    UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
  ::user_data_auth::AuthenticateAuthSessionReply reply;
  reply.set_error(
      ::user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
  reply.set_authenticated(false);
  std::move(callback).Run(reply);
}

void ExpectKeyLabel(
    const ::user_data_auth::AuthenticateAuthSessionRequest& request,
    const std::string& label) {
  EXPECT_EQ(request.authorization().key().data().label(), label);
}

class AuthPerformerTest : public testing::Test {
 public:
  AuthPerformerTest()
      : task_environment_(
            base::test::SingleThreadTaskEnvironment::MainThreadType::UI) {
    CryptohomeMiscClient::InitializeFake();
    chromeos::SystemSaltGetter::Initialize();
    context_ = std::make_unique<UserContext>();
    context_->SetAuthSessionId("123");
  }

  ~AuthPerformerTest() override {
    chromeos::SystemSaltGetter::Shutdown();
    CryptohomeMiscClient::Shutdown();
  }

 protected:
  base::test::SingleThreadTaskEnvironment task_environment_;
  ::testing::StrictMock<MockUserDataAuthClient> mock_client_;
  std::unique_ptr<UserContext> context_;
};

// Checks that AuthenticateUsingKnowledgeKey (which will be called with "gaia"
// label after online authentication) correctly falls back to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyCorrectLabelFallback) {
  SetupUserWithLegacyPassword(context_.get());
  // Password knowledge key in user context.
  *context_->GetKey() = Key("secret");
  context_->GetKey()->SetLabel("gaia");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthSession(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthSessionRequest& request,
             UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
            EXPECT_EQ(request.authorization().key().data().label(), "legacy-0");
            ReplyAsSuccess(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  // Check for no error, and user context is present
  ASSERT_FALSE(result.Get<1>().has_value());
  ASSERT_TRUE(result.Get<0>());
}

// Checks that AuthenticateUsingKnowledgeKey called with "pin" key does not
// fallback to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyNoFallbackOnPin) {
  SetupUserWithLegacyPassword(context_.get());

  // PIN knowledge key in user context.
  *context_->GetKey() =
      Key(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, "salt", /*secret=*/"123456");
  context_->GetKey()->SetLabel("pin");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthSession(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthSessionRequest& request,
             UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
            ExpectKeyLabel(request, "pin");
            ReplyAsKeyMismatch(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  // Check that the error is present, and user context is passed back.
  ASSERT_TRUE(result.Get<0>());
  ASSERT_TRUE(result.Get<1>().has_value());
  ASSERT_EQ(result.Get<1>().value().error_code,
            user_data_auth::CRYPTOHOME_ERROR_AUTHORIZATION_KEY_NOT_FOUND);
}

TEST_F(AuthPerformerTest, AuthenticateWithPasswordCorrectLabel) {
  SetupUserWithLegacyPassword(context_.get());

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthSession(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthSessionRequest& request,
             UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
            ExpectKeyLabel(request, "legacy-0");
            ReplyAsSuccess(std::move(callback));
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;

  performer.AuthenticateWithPassword("legacy-0", "secret", std::move(context_),
                                     result.GetCallback());
  // Check for no error
  ASSERT_TRUE(result.Get<0>());
  ASSERT_FALSE(result.Get<1>().has_value());
}

TEST_F(AuthPerformerTest, AuthenticateWithPasswordBadLabel) {
  SetupUserWithLegacyPassword(context_.get());

  AuthPerformer performer(&mock_client_);

  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;

  performer.AuthenticateWithPassword("gaia", "secret", std::move(context_),
                                     result.GetCallback());

  // Check that error is triggered
  ASSERT_TRUE(result.Get<0>());
  ASSERT_TRUE(result.Get<1>().has_value());
  ASSERT_EQ(result.Get<1>().value().error_code,
            user_data_auth::CRYPTOHOME_ERROR_KEY_NOT_FOUND);
}

}  // namespace
}  // namespace ash
