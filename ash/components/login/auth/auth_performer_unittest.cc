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
#include "chromeos/dbus/userdataauth/cryptohome_misc_client.h"
#include "chromeos/dbus/userdataauth/mock_userdataauth_client.h"
#include "chromeos/dbus/userdataauth/userdataauth_client.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

using ::testing::_;

namespace ash {
namespace {

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
  ::testing::StrictMock<chromeos::MockUserDataAuthClient> mock_client_;
  std::unique_ptr<UserContext> context_;
};

// Checks that AuthenticateUsingKnowledgeKey (which will be called with "gaia"
// label after online authentication) correctly falls back to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyCorrectLabelFallback) {
  // Keys as listed by StartAuthSession
  std::vector<cryptohome::KeyDefinition> keys;
  keys.push_back(cryptohome::KeyDefinition::CreateForPassword(
      "secret", "legacy-0", /*privileges=*/0));
  AuthFactorsData data(keys);
  context_->SetAuthFactorsData(data);
  // Knowledge key in user context.
  *context_->GetKey() = Key("secret");
  context_->GetKey()->SetLabel("gaia");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthSession(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthSessionRequest& request,
             UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
            EXPECT_EQ(request.authorization().key().data().label(), "legacy-0");
            // just fail the operation
            std::move(callback).Run(absl::nullopt);
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  ASSERT_TRUE(result.Get<1>().has_value());
}

// Checks that AuthenticateUsingKnowledgeKey called with "pin" key does not
// fallback to "legacy-0" label.
TEST_F(AuthPerformerTest, KnowledgeKeyNoFallbackOnPin) {
  // Keys as listed by StartAuthSession
  std::vector<cryptohome::KeyDefinition> keys;
  keys.push_back(cryptohome::KeyDefinition::CreateForPassword(
      "secret", "legacy-0", /*privileges=*/0));
  AuthFactorsData data(keys);
  context_->SetAuthFactorsData(data);
  // Knowledge key in user context.
  *context_->GetKey() =
      Key(Key::KEY_TYPE_SALTED_PBKDF2_AES256_1234, "salt", /*secret=*/"123456");
  context_->GetKey()->SetLabel("pin");

  AuthPerformer performer(&mock_client_);

  EXPECT_CALL(mock_client_, AuthenticateAuthSession(_, _))
      .WillOnce(
          [](const ::user_data_auth::AuthenticateAuthSessionRequest& request,
             UserDataAuthClient::AuthenticateAuthSessionCallback callback) {
            EXPECT_EQ(request.authorization().key().data().label(), "pin");
            // just fail the operation
            std::move(callback).Run(absl::nullopt);
          });
  base::test::TestFuture<std::unique_ptr<UserContext>,
                         absl::optional<CryptohomeError>>
      result;
  performer.AuthenticateUsingKnowledgeKey(std::move(context_),
                                          result.GetCallback());
  ASSERT_TRUE(result.Get<1>().has_value());
}

}  // namespace
}  // namespace ash
