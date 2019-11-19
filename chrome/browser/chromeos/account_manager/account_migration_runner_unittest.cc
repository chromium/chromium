// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/account_manager/account_migration_runner.h"

#include <memory>
#include <string>
#include <utility>

#include "base/bind.h"
#include "base/macros.h"
#include "base/memory/weak_ptr.h"
#include "base/run_loop.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace chromeos {

namespace {

class AlwaysSucceeds : public AccountMigrationRunner::Step {
 public:
  AlwaysSucceeds(const std::string& id, base::RepeatingClosure closure)
      : AccountMigrationRunner::Step(id), closure_(closure) {}
  ~AlwaysSucceeds() override = default;

  void Run() override {
    closure_.Run();
    FinishWithSuccess();
  }

 private:
  base::RepeatingClosure closure_;
  DISALLOW_COPY_AND_ASSIGN(AlwaysSucceeds);
};

class AlwaysFails : public AccountMigrationRunner::Step {
 public:
  AlwaysFails(const std::string& id, base::RepeatingClosure closure)
      : AccountMigrationRunner::Step(id), closure_(closure) {}
  ~AlwaysFails() override = default;

  void Run() override {
    closure_.Run();
    FinishWithFailure();
  }

 private:
  base::RepeatingClosure closure_;
  DISALLOW_COPY_AND_ASSIGN(AlwaysFails);
};

class MustNeverRun : public AccountMigrationRunner::Step {
 public:
  explicit MustNeverRun(const std::string& id)
      : AccountMigrationRunner::Step(id) {}
  ~MustNeverRun() override = default;

  void Run() override { EXPECT_FALSE(true); }

 private:
  DISALLOW_COPY_AND_ASSIGN(MustNeverRun);
};

}  // namespace

class AccountMigrationRunnerTest : public testing::Test {
 protected:
  AccountMigrationRunnerTest() {
    increment_num_steps_executed_ = base::BindRepeating(
        &AccountMigrationRunnerTest::IncrementNumStepsExecuted,
        weak_factory_.GetWeakPtr());
  }

  ~AccountMigrationRunnerTest() override = default;

  AccountMigrationRunner::MigrationResult RunMigration() {
    AccountMigrationRunner::MigrationResult migration_result;
    base::RunLoop run_loop;
    AccountMigrationRunner::OnMigrationDone callback = base::BindOnce(
        [](AccountMigrationRunner::MigrationResult* result,
           base::OnceClosure quit_closure,
           const AccountMigrationRunner::MigrationResult& returned_result) {
          *result = returned_result;
          std::move(quit_closure).Run();
        },
        &migration_result, run_loop.QuitClosure());

    migration_runner_.Run(std::move(callback));
    // Wait for callback from |migration_runner_|.
    run_loop.Run();

    return migration_result;
  }

  // Check base/test/task_environment.h. This must be the first member /
  // declared before any member that cares about tasks.
  base::test::TaskEnvironment task_environment_;

  AccountMigrationRunner migration_runner_;

  int num_steps_executed_ = 0;

  base::RepeatingClosure increment_num_steps_executed_;

 private:
  void IncrementNumStepsExecuted() { ++num_steps_executed_; }

  base::WeakPtrFactory<AccountMigrationRunnerTest> weak_factory_{this};
  DISALLOW_COPY_AND_ASSIGN(AccountMigrationRunnerTest);
};

TEST_F(AccountMigrationRunnerTest, DoesNotRunUntilRunIsCalled) {
  EXPECT_EQ(AccountMigrationRunner::Status::kNotStarted,
            migration_runner_.GetStatus());
}

TEST_F(AccountMigrationRunnerTest, RunsSuccessfullyForZeroSteps) {
  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_result.final_status);
  EXPECT_EQ(0, num_steps_executed_);
}

TEST_F(AccountMigrationRunnerTest, RunsSuccessfullyForOneStep) {
  migration_runner_.AddStep(
      std::make_unique<AlwaysSucceeds>("Step1", increment_num_steps_executed_));

  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_result.final_status);
  EXPECT_EQ(1, num_steps_executed_);
}

TEST_F(AccountMigrationRunnerTest, FailsForOneFailingStep) {
  migration_runner_.AddStep(
      std::make_unique<AlwaysFails>("Step1", increment_num_steps_executed_));

  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_result.final_status);
  EXPECT_EQ("Step1", migration_result.failed_step_id);
  EXPECT_EQ(1, num_steps_executed_);
}

TEST_F(AccountMigrationRunnerTest, RunsSuccessfullyForMoreThanOneStep) {
  migration_runner_.AddStep(
      std::make_unique<AlwaysSucceeds>("Step1", increment_num_steps_executed_));
  migration_runner_.AddStep(
      std::make_unique<AlwaysSucceeds>("Step2", increment_num_steps_executed_));
  migration_runner_.AddStep(
      std::make_unique<AlwaysSucceeds>("Step3", increment_num_steps_executed_));

  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kSuccess,
            migration_result.final_status);
  EXPECT_EQ(3, num_steps_executed_);
}

TEST_F(AccountMigrationRunnerTest, FailsIfFirstStepFails) {
  migration_runner_.AddStep(
      std::make_unique<AlwaysFails>("Step1", increment_num_steps_executed_));
  migration_runner_.AddStep(std::make_unique<MustNeverRun>("Step2"));

  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_result.final_status);
  EXPECT_EQ("Step1", migration_result.failed_step_id);
  EXPECT_EQ(1, num_steps_executed_);
}

TEST_F(AccountMigrationRunnerTest, FailsIfIntermediateStepFails) {
  migration_runner_.AddStep(
      std::make_unique<AlwaysSucceeds>("Step1", increment_num_steps_executed_));
  migration_runner_.AddStep(
      std::make_unique<AlwaysFails>("Step2", increment_num_steps_executed_));
  migration_runner_.AddStep(std::make_unique<MustNeverRun>("Step3"));

  AccountMigrationRunner::MigrationResult migration_result = RunMigration();
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_runner_.GetStatus());
  EXPECT_EQ(AccountMigrationRunner::Status::kFailure,
            migration_result.final_status);
  EXPECT_EQ("Step2", migration_result.failed_step_id);
  EXPECT_EQ(2, num_steps_executed_);
}

}  // namespace chromeos
