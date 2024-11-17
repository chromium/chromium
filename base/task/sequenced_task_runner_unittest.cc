// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequenced_task_runner.h"

#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/gtest_prod_util.h"
#include "base/location.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/scoped_refptr.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/null_task_runner.h"
#include "base/test/task_environment.h"
#include "base/test/test_mock_time_task_runner.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class FlagOnDelete {
 public:
  FlagOnDelete(bool* deleted,
               scoped_refptr<SequencedTaskRunner> expected_deletion_sequence)
      : deleted_(deleted),
        expected_deletion_sequence_(std::move(expected_deletion_sequence)) {}
  FlagOnDelete(const FlagOnDelete&) = delete;
  FlagOnDelete& operator=(const FlagOnDelete&) = delete;

  ~FlagOnDelete() {
    EXPECT_FALSE(*deleted_);
    *deleted_ = true;
    if (expected_deletion_sequence_)
      EXPECT_TRUE(expected_deletion_sequence_->RunsTasksInCurrentSequence());
  }

 private:
  raw_ptr<bool> deleted_;
  const scoped_refptr<SequencedTaskRunner> expected_deletion_sequence_;
};

class SequencedTaskRunnerTest : public testing::Test {
 public:
  SequencedTaskRunnerTest(const SequencedTaskRunnerTest&) = delete;
  SequencedTaskRunnerTest& operator=(const SequencedTaskRunnerTest&) = delete;

 protected:
  SequencedTaskRunnerTest() : foreign_thread_("foreign") {}

  void SetUp() override {
    foreign_thread_.Start();
    foreign_runner_ = foreign_thread_.task_runner();
  }

  scoped_refptr<SequencedTaskRunner> foreign_runner_;

  Thread foreign_thread_;

 private:
  test::TaskEnvironment task_environment_;
};

}  // namespace

using SequenceBoundUniquePtr =
    std::unique_ptr<FlagOnDelete, OnTaskRunnerDeleter>;

TEST_F(SequencedTaskRunnerTest, OnTaskRunnerDeleterOnMainThread) {
  bool deleted_on_main_thread = false;
  SequenceBoundUniquePtr ptr(
      new FlagOnDelete(&deleted_on_main_thread,
                       SequencedTaskRunner::GetCurrentDefault()),
      OnTaskRunnerDeleter(SequencedTaskRunner::GetCurrentDefault()));
  EXPECT_FALSE(deleted_on_main_thread);
  foreign_runner_->PostTask(FROM_HERE, DoNothingWithBoundArgs(std::move(ptr)));

  {
    RunLoop run_loop;
    foreign_runner_->PostTaskAndReply(FROM_HERE, BindOnce([] {}),
                                      run_loop.QuitClosure());
    run_loop.Run();
  }
  EXPECT_TRUE(deleted_on_main_thread);
}

TEST_F(SequencedTaskRunnerTest, OnTaskRunnerDeleterTargetStoppedEarly) {
  bool deleted_on_main_thread = false;
  FlagOnDelete* raw = new FlagOnDelete(
      &deleted_on_main_thread, SequencedTaskRunner::GetCurrentDefault());
  SequenceBoundUniquePtr ptr(raw, OnTaskRunnerDeleter(foreign_runner_));
  EXPECT_FALSE(deleted_on_main_thread);

  // Stopping the target ahead of deleting |ptr| should make its
  // OnTaskRunnerDeleter no-op.
  foreign_thread_.Stop();
  ptr = nullptr;
  EXPECT_FALSE(deleted_on_main_thread);

  delete raw;
  EXPECT_TRUE(deleted_on_main_thread);
}

TEST_F(SequencedTaskRunnerTest, DelayedTaskHandle_RunTask) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();

  bool task_ran = false;
  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
          BindLambdaForTesting([&task_ran] { task_ran = true; }), Seconds(1));
  EXPECT_TRUE(delayed_task_handle.IsValid());
  EXPECT_TRUE(task_runner->HasPendingTask());

  // Run the delayed task.
  task_runner->FastForwardUntilNoTasksRemain();

  EXPECT_FALSE(delayed_task_handle.IsValid());
  EXPECT_FALSE(task_runner->HasPendingTask());
  EXPECT_TRUE(task_ran);
}

TEST_F(SequencedTaskRunnerTest, DelayedTaskHandle_CancelTask) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();

  bool task_ran = false;
  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
          BindLambdaForTesting([&task_ran] { task_ran = true; }), Seconds(1));
  EXPECT_TRUE(delayed_task_handle.IsValid());
  EXPECT_TRUE(task_runner->HasPendingTask());

  // Cancel the delayed task.
  delayed_task_handle.CancelTask();

  EXPECT_FALSE(delayed_task_handle.IsValid());
  EXPECT_FALSE(task_runner->HasPendingTask());
  EXPECT_FALSE(task_ran);
}

TEST_F(SequencedTaskRunnerTest, DelayedTaskHandle_DestroyTask) {
  auto task_runner = MakeRefCounted<TestMockTimeTaskRunner>();

  bool task_ran = false;
  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
          BindLambdaForTesting([&task_ran] { task_ran = true; }), Seconds(1));
  EXPECT_TRUE(delayed_task_handle.IsValid());
  EXPECT_TRUE(task_runner->HasPendingTask());

  // Destroy the pending task.
  task_runner->ClearPendingTasks();

  EXPECT_FALSE(delayed_task_handle.IsValid());
  EXPECT_FALSE(task_runner->HasPendingTask());
  EXPECT_FALSE(task_ran);
}

// Tests that if PostCancelableDelayedTask() fails, the returned handle will be
// invalid.
TEST_F(SequencedTaskRunnerTest, DelayedTaskHandle_PostTaskFailed) {
  auto task_runner = MakeRefCounted<NullTaskRunner>();

  bool task_ran = false;
  DelayedTaskHandle delayed_task_handle =
      task_runner->PostCancelableDelayedTask(
          subtle::PostDelayedTaskPassKeyForTesting(), FROM_HERE,
          BindLambdaForTesting([&task_ran] { task_ran = true; }), Seconds(1));
  EXPECT_FALSE(delayed_task_handle.IsValid());
  EXPECT_FALSE(task_ran);
}

namespace {

// Tests for the SequencedTaskRunner::CurrentDefaultHandle machinery.
class SequencedTaskRunnerCurrentDefaultHandleTest : public ::testing::Test {
 protected:
  // Verifies that the context it runs on has a
  // SequencedTaskRunner::CurrentDefaultHandle and that posting to it results in
  // the posted task running in that same context (sequence).
  static void VerifyCurrentSequencedTaskRunner() {
    ASSERT_TRUE(SequencedTaskRunner::HasCurrentDefault());
    scoped_refptr<SequencedTaskRunner> task_runner =
        SequencedTaskRunner::GetCurrentDefault();
    ASSERT_TRUE(task_runner);

    // Use SequenceCheckerImpl to make sure it's not a no-op in Release builds.
    std::unique_ptr<SequenceCheckerImpl> sequence_checker =
        std::make_unique<SequenceCheckerImpl>();
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(
            &SequencedTaskRunnerCurrentDefaultHandleTest::CheckValidSequence,
            std::move(sequence_checker)));
  }

  static void CheckValidSequence(
      std::unique_ptr<SequenceCheckerImpl> sequence_checker) {
    EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  }

  test::TaskEnvironment task_environment_;
};

}  // namespace

TEST_F(SequencedTaskRunnerCurrentDefaultHandleTest, FromTaskEnvironment) {
  VerifyCurrentSequencedTaskRunner();
  RunLoop().RunUntilIdle();
}

TEST_F(SequencedTaskRunnerCurrentDefaultHandleTest,
       FromThreadPoolSequencedTask) {
  base::ThreadPool::CreateSequencedTaskRunner({})->PostTask(
      FROM_HERE, base::BindOnce(&SequencedTaskRunnerCurrentDefaultHandleTest::
                                    VerifyCurrentSequencedTaskRunner));
  task_environment_.RunUntilIdle();
}

TEST_F(SequencedTaskRunnerCurrentDefaultHandleTest,
       NoHandleFromUnsequencedTask) {
  base::ThreadPool::PostTask(base::BindOnce(
      [] { EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault()); }));
  task_environment_.RunUntilIdle();
}

// Verify that `CurrentDefaultHandle` can be used to set the current default
// `SequencedTaskRunner` to null in a scope that already has a default.
TEST_F(SequencedTaskRunnerCurrentDefaultHandleTest, OverrideWithNull) {
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  auto tr1 = SequencedTaskRunner::GetCurrentDefault();
  EXPECT_TRUE(tr1);

  {
    SequencedTaskRunner::CurrentDefaultHandle handle(
        nullptr, SequencedTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
    EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault());
    EXPECT_CHECK_DEATH(
        { auto tr2 = SequencedTaskRunner::GetCurrentDefault(); });
  }

  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());
}

// Verify that `CurrentDefaultHandle` can be used to set the current default
// `SequencedTaskRunner` to a non-null value in a scope that already has a
// default.
TEST_F(SequencedTaskRunnerCurrentDefaultHandleTest, OverrideWithNonNull) {
  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  auto tr1 = SequencedTaskRunner::GetCurrentDefault();
  EXPECT_TRUE(tr1);

  {
    auto tr2 = MakeRefCounted<TestSimpleTaskRunner>();
    SequencedTaskRunner::CurrentDefaultHandle handle(
        tr2, SequencedTaskRunner::CurrentDefaultHandle::MayAlreadyExist{});
    EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
    EXPECT_EQ(tr2, SequencedTaskRunner::GetCurrentDefault());
  }

  EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_EQ(tr1, SequencedTaskRunner::GetCurrentDefault());
}

TEST(SequencedTaskRunnerCurrentDefaultHandleTestWithoutTaskEnvironment,
     FromHandleInScope) {
  scoped_refptr<SequencedTaskRunner> test_task_runner =
      MakeRefCounted<TestSimpleTaskRunner>();
  EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
  {
    SequencedTaskRunner::CurrentDefaultHandle current_default(test_task_runner);
    EXPECT_TRUE(SequencedTaskRunner::HasCurrentDefault());
    EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
    EXPECT_EQ(test_task_runner, SequencedTaskRunner::GetCurrentDefault());
  }
  EXPECT_FALSE(SequencedTaskRunner::HasCurrentDefault());
  EXPECT_FALSE(SingleThreadTaskRunner::HasCurrentDefault());
}

}  // namespace base
