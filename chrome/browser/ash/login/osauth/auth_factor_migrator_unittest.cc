// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/login/osauth/auth_factor_migrator.h"

#include <memory>
#include <optional>

#include "base/functional/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/task_environment.h"
#include "base/test/test_future.h"
#include "chrome/browser/ash/login/osauth/auth_factor_migration.h"
#include "chromeos/ash/components/login/auth/public/user_context.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace ash {
namespace {

using AuthOperationTestFuture =
    base::test::TestFuture<std::unique_ptr<UserContext>,
                           std::optional<AuthenticationError>>;

struct FakeMigrationInput {
  std::optional<AuthenticationError> error = std::nullopt;
  bool should_skip = false;
};

class FakeMigration : public AuthFactorMigration {
 public:
  // Provided error will be returned from `Run`.
  explicit FakeMigration(const FakeMigrationInput& input,
                         base::OnceClosure callback)
      : error_(input.error),
        should_skip_(input.should_skip),
        callback_(std::move(callback)) {}
  ~FakeMigration() override = default;

  FakeMigration(const FakeMigration&) = delete;
  FakeMigration& operator=(const FakeMigration&) = delete;

  void Run(std::unique_ptr<UserContext> context,
           AuthOperationCallback callback) override {
    std::move(callback).Run(std::move(context), error_);
    std::move(callback_).Run();
  }

  bool WasSkipped() override { return should_skip_; }

  MigrationName GetName() override {
    return MigrationName::kRecoveryFactorHsmPubkeyMigration;
  }

 private:
  std::optional<AuthenticationError> error_;
  bool should_skip_;
  base::OnceClosure callback_;
};

}  // namespace

class AuthFactorMigratorTest : public testing::Test {
 public:
  AuthFactorMigratorTest() {}

  AuthFactorMigratorTest(const AuthFactorMigratorTest&) = delete;
  AuthFactorMigratorTest& operator=(const AuthFactorMigratorTest&) = delete;

  ~AuthFactorMigratorTest() override = default;

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
      std::vector<FakeMigrationInput> migration_results) {
    auto list = std::vector<std::unique_ptr<AuthFactorMigration>>();
    int index = 0;
    for (const auto& result : migration_results) {
      migration_list_[index] = false;
      list.emplace_back(std::make_unique<FakeMigration>(
          result, base::BindOnce(&AuthFactorMigratorTest::OnMigrationCalled,
                                 base::Unretained(this), index)));
      index++;
    }
    return std::make_unique<AuthFactorMigrator>(std::move(list));
  }

  bool WasMigrationCalled(int index) { return migration_list_[index]; }

  void ExpectTotalMetricsCount(base::HistogramTester* histogram_tester,
                               int number) {
    histogram_tester->ExpectTotalCount(
        "Ash.OSAuth.Login.AuthFactorMigrationResult."
        "RecoveryFactorHsmPubkeyMigration",
        number);
  }

  void ExpectMigrationResult(base::HistogramTester* histogram_tester,
                             AuthFactorMigrator::MigrationResult result,
                             int number) {
    histogram_tester->ExpectBucketCount(
        "Ash.OSAuth.Login.AuthFactorMigrationResult."
        "RecoveryFactorHsmPubkeyMigration",
        static_cast<int>(result), number);
  }

  raw_ptr<UserContext> context_ = nullptr;

 private:
  void OnMigrationCalled(int index) { migration_list_[index] = true; }

  std::map<int, bool> migration_list_;

  base::test::SingleThreadTaskEnvironment task_environment_;
};

TEST_F(AuthFactorMigratorTest, EmptyListRun) {
  base::HistogramTester histogram_tester;
  auto migrator = CreateMigrator({});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());

  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), std::nullopt);

  ExpectTotalMetricsCount(&histogram_tester, 0);
}

TEST_F(AuthFactorMigratorTest, SingleSuccessRun) {
  base::HistogramTester histogram_tester;
  auto migrator = CreateMigrator({FakeMigrationInput{.error = std::nullopt}});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());

  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), std::nullopt);
  ASSERT_TRUE(WasMigrationCalled(0));

  ExpectTotalMetricsCount(&histogram_tester, 1);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kSuccess, 1);
}

TEST_F(AuthFactorMigratorTest, MultipleSuccessRun) {
  base::HistogramTester histogram_tester;
  auto migrator = CreateMigrator({FakeMigrationInput{.error = std::nullopt},
                                  FakeMigrationInput{.error = std::nullopt},
                                  FakeMigrationInput{.error = std::nullopt}});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());

  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), std::nullopt);
  ASSERT_TRUE(WasMigrationCalled(0));
  ASSERT_TRUE(WasMigrationCalled(1));
  ASSERT_TRUE(WasMigrationCalled(2));

  ExpectTotalMetricsCount(&histogram_tester, 3);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kSuccess, 3);
}

TEST_F(AuthFactorMigratorTest, FailureRun) {
  base::HistogramTester histogram_tester;
  auto migrator = CreateMigrator(
      {FakeMigrationInput{.error = std::nullopt},
       FakeMigrationInput{.error = AuthenticationError{AuthFailure::TPM_ERROR}},
       FakeMigrationInput{
           .error = AuthenticationError{AuthFailure::LOGIN_TIMED_OUT}},
       FakeMigrationInput{.error = std::nullopt}});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());

  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_NE(future.Get<1>(), std::nullopt);
  ASSERT_EQ(future.Get<1>()->get_resolved_failure(), AuthFailure::TPM_ERROR);
  ASSERT_TRUE(WasMigrationCalled(0));
  ASSERT_TRUE(WasMigrationCalled(1));
  ASSERT_FALSE(WasMigrationCalled(2));
  ASSERT_FALSE(WasMigrationCalled(3));

  ExpectTotalMetricsCount(&histogram_tester, 4);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kSuccess, 1);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kFailed, 1);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kNotRun, 2);
}

TEST_F(AuthFactorMigratorTest, SkippedRun) {
  base::HistogramTester histogram_tester;
  auto migrator = CreateMigrator(
      {FakeMigrationInput{.error = std::nullopt, .should_skip = true},
       FakeMigrationInput{.error = std::nullopt}});
  AuthOperationTestFuture future;
  migrator->Run(CreateContext(), future.GetCallback());

  ASSERT_EQ(future.Get<0>().get(), context_);
  context_ = nullptr;  // Release ptr.
  ASSERT_EQ(future.Get<1>(), std::nullopt);
  ASSERT_TRUE(WasMigrationCalled(0));
  ASSERT_TRUE(WasMigrationCalled(1));

  ExpectTotalMetricsCount(&histogram_tester, 2);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kSuccess, 1);
  ExpectMigrationResult(&histogram_tester,
                        AuthFactorMigrator::MigrationResult::kSkipped, 1);
}

}  // namespace ash
