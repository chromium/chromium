// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/recovery_id_migration.h"

#include <memory>
#include <optional>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chromeos/ash/components/dbus/userdataauth/fake_userdataauth_client.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "google_apis/gaia/gaia_id.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using AuthOperationTestFuture =
    base::test::TestFuture<std::unique_ptr<UserContext>,
                           std::optional<AuthenticationError>>;

}  // namespace

class RecoveryIdMigrationTest : public testing::Test {
 public:
  RecoveryIdMigrationTest() = default;
  ~RecoveryIdMigrationTest() override = default;

  void SetUp() override {
    UserDataAuthClient::InitializeFake();
    migrator_ =
        std::make_unique<RecoveryIdMigration>(FakeUserDataAuthClient::Get());
  }

  void TearDown() override {
    migrator_.reset();
    UserDataAuthClient::Shutdown();
  }

 protected:
  std::unique_ptr<UserContext> CreateContext(
      bool generate_fresh_recovery_id,
      bool has_recovery_factor) {
    auto context = std::make_unique<UserContext>();
    context->SetAccountId(test_account_id_);
    context->SetGenerateFreshRecoveryId(generate_fresh_recovery_id);
    if (has_recovery_factor) {
      cryptohome::AuthFactorRef ref(cryptohome::AuthFactorType::kRecovery,
                                    cryptohome::KeyLabel("recovery"));
      cryptohome::AuthFactor factor(ref, /*common_metadata=*/{});
      AuthFactorsConfiguration config({std::move(factor)}, /*supported_factors=*/{});
      context->SetAuthFactorsConfiguration(std::move(config));
    } else {
      context->SetAuthFactorsConfiguration(AuthFactorsConfiguration{});
    }
    return context;
  }

  const AccountId test_account_id_ =
      AccountId::FromUserEmailGaiaId("test@test.com", GaiaId("12345"));
  std::unique_ptr<RecoveryIdMigration> migrator_;
  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(RecoveryIdMigrationTest, SkippedNoRecoveryFactor) {
  AuthOperationTestFuture future;
  migrator_->Run(CreateContext(/*generate_fresh_recovery_id=*/true,
                               /*has_recovery_factor=*/false),
                 future.GetCallback());
  EXPECT_EQ(future.Get<1>(), std::nullopt);
  EXPECT_TRUE(migrator_->WasSkipped());
  EXPECT_FALSE(
      FakeUserDataAuthClient::Get()
          ->WasCalled<
              FakeUserDataAuthClient::Operation::kGenerateFreshRecoveryId>());
}

TEST_F(RecoveryIdMigrationTest, SkippedNoGenerateFlag) {
  AuthOperationTestFuture future;
  migrator_->Run(CreateContext(/*generate_fresh_recovery_id=*/false,
                               /*has_recovery_factor=*/true),
                 future.GetCallback());
  EXPECT_EQ(future.Get<1>(), std::nullopt);
  EXPECT_TRUE(migrator_->WasSkipped());
  EXPECT_FALSE(
      FakeUserDataAuthClient::Get()
          ->WasCalled<
              FakeUserDataAuthClient::Operation::kGenerateFreshRecoveryId>());
}

TEST_F(RecoveryIdMigrationTest, Success) {
  AuthOperationTestFuture future;
  migrator_->Run(CreateContext(/*generate_fresh_recovery_id=*/true,
                               /*has_recovery_factor=*/true),
                 future.GetCallback());
  EXPECT_EQ(future.Get<1>(), std::nullopt);
  EXPECT_FALSE(migrator_->WasSkipped());
  EXPECT_TRUE(
      FakeUserDataAuthClient::Get()
          ->WasCalled<
              FakeUserDataAuthClient::Operation::kGenerateFreshRecoveryId>());
  EXPECT_FALSE(future.Get<0>()->GenerateFreshRecoveryId());
}

TEST_F(RecoveryIdMigrationTest, FailureIsIgnored) {
  FakeUserDataAuthClient::Get()->SetNextOperationError(
      FakeUserDataAuthClient::Operation::kGenerateFreshRecoveryId,
      cryptohome::ErrorWrapper::CreateFromErrorCodeOnly(
          user_data_auth::CRYPTOHOME_ERROR_ACCOUNT_NOT_FOUND));
  AuthOperationTestFuture future;
  migrator_->Run(CreateContext(/*generate_fresh_recovery_id=*/true,
                               /*has_recovery_factor=*/true),
                 future.GetCallback());
  EXPECT_EQ(future.Get<1>(), std::nullopt);
  EXPECT_FALSE(migrator_->WasSkipped());
  EXPECT_TRUE(
      FakeUserDataAuthClient::Get()
          ->WasCalled<
              FakeUserDataAuthClient::Operation::kGenerateFreshRecoveryId>());
  EXPECT_FALSE(future.Get<0>()->GenerateFreshRecoveryId());
}

}  // namespace ash