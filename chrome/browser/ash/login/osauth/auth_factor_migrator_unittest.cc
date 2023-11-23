// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"
#include "base/functional/bind.h"

#include <memory>

#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace ash {
namespace {

using AuthOperationTestFuture =
    base::test::TestFuture<std::unique_ptr<UserContext>,
                           absl::optional<AuthenticationError>>;

class FakeMigration : public AuthFactorMigration {
 public:
  // Provided error will be returned from `Run`.
  explicit FakeMigration(absl::optional<AuthenticationError> error,
                         base::OnceClosure callback)
      : error_(error), callback_(std::move(callback)) {}
  ~FakeMigration() override = default;

  FakeMigration(const FakeMigration&) = delete;
  FakeMigration& operator=(const FakeMigration&) = delete;

  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback) override {
    std::move(callback).Run(std::move(context), error_);
    std::move(callback_).Run();
  }

 private:
  absl::optional<AuthenticationError> error_;
  base::OnceClosure callback_;
};

}  // namespace

class AuthFactorMigratorTest : public testing::Test {
 public:
  AuthFactorMigratorTest() {}

  AuthFactorMigratorTest(const AuthFactorMigratorTest&) = delete;
  AuthFactorMigratorTest& operator=(const AuthFactorMigratorTest&) = delete;

  ~AuthFactorMigratorTest() override = default;

  void SetUp() override {}

  void TearDown() override {
    context_ = nullptr;
    migration_list_.clear();
  }

  std::unique_ptr<UserContext> CreateContext() {
    auto context = std::make_unique<UserContext>();
    context->SetAuthSessionIds("123", "broadcast");
    context->AddAuthorizedIntent(AuthSessionIntent::kDecrypt);
    context_ = context.get();
    return context;
  }

  // Creates `AuthFactorMigrator` with the list of fake migrations from the
  // provided results.
  std::unique_ptr<AuthFactorMigrator> CreateMigrator(
      std::vector<absl::optional<AuthenticationError>> migration_results) {
    auto list = std::vector<std::unique_ptr<AuthFactorMigration>>();
    int index = 0;
    for (auto result : migration_results) {
      migration_list_[index] = false;
      list.emplace_back(std::make_unique<FakeMigration>(
          result, base::BindOnce(&AuthFactorMigratorTest::OnMigrationCalled,
                                 base::Unretained(this), index)));
      index++;
    }
    return std::make_unique<AuthFactorMigrator>(std::move(list));
  }

  bool WasMigrationCalled(int index) { return migration_list_[index]; }

  raw_ptr<UserContext> context_ = nullptr;

 private:
  void OnMigrationCalled(int index) { migration_list_[index] = true; }

  std::map<int, bool> migration_list_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AuthFactorMigratorTest, SingleSuccessRun) {
  auto migrator = CreateMigrator({absl::nullopt});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());
  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), absl::nullopt);
  ASSERT_TRUE(WasMigrationCalled(0));
}

TEST_F(AuthFactorMigratorTest, MultipleSuccessRun) {
  auto migrator = CreateMigrator({absl::nullopt, absl::nullopt, absl::nullopt});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());
  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), absl::nullopt);
  ASSERT_TRUE(WasMigrationCalled(0));
  ASSERT_TRUE(WasMigrationCalled(1));
  ASSERT_TRUE(WasMigrationCalled(2));
}

TEST_F(AuthFactorMigratorTest, FailureRun) {
  auto migrator = CreateMigrator({absl::nullopt,
                                  AuthenticationError{AuthFailure::TPM_ERROR},
                                  absl::nullopt});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());
  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_NE(future.Get<1>(), absl::nullopt);
  ASSERT_EQ(future.Get<1>()->get_resolved_failure(), AuthFailure::TPM_ERROR);
  ASSERT_TRUE(WasMigrationCalled(0));
  ASSERT_TRUE(WasMigrationCalled(1));
  ASSERT_FALSE(WasMigrationCalled(2));
}

}  // namespace ash
