// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/threading/sequenced_task_runner_handle.h"

#include <memory>
#include <utility>

#include "base/bind.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/memory/ref_counted.h"
#include "base/run_loop.h"
#include "base/sequence_checker_impl.h"
#include "base/sequenced_task_runner.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/test/task_environment.h"
#include "base/test/test_simple_task_runner.h"
#include "base/threading/thread_task_runner_handle.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

class SequencedTaskRunnerHandleTest : public ::testing::Test {
 protected:
  // Verifies that the context it runs on has a SequencedTaskRunnerHandle
  // and that posting to it results in the posted task running in that same
  // context (sequence).
  static void VerifyCurrentSequencedTaskRunner() {
    ASSERT_TRUE(SequencedTaskRunnerHandle::IsSet());
    scoped_refptr<SequencedTaskRunner> task_runner =
        SequencedTaskRunnerHandle::Get();
    ASSERT_TRUE(task_runner);

    // Use SequenceCheckerImpl to make sure it's not a no-op in Release builds.
    std::unique_ptr<SequenceCheckerImpl> sequence_checker(
        new SequenceCheckerImpl);
    task_runner->PostTask(
        FROM_HERE,
        base::BindOnce(&SequencedTaskRunnerHandleTest::CheckValidSequence,
                       std::move(sequence_checker)));
  }

  static void CheckValidSequence(
      std::unique_ptr<SequenceCheckerImpl> sequence_checker) {
    EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  }

  base::test::TaskEnvironment task_environment_;
};

TEST_F(SequencedTaskRunnerHandleTest, FromTaskEnvironment) {
  VerifyCurrentSequencedTaskRunner();
  RunLoop().RunUntilIdle();
}

TEST_F(SequencedTaskRunnerHandleTest, FromThreadPoolSequencedTask) {
  base::CreateSequencedTaskRunner({ThreadPool()})
      ->PostTask(FROM_HERE,
                 base::BindOnce(&SequencedTaskRunnerHandleTest::
                                    VerifyCurrentSequencedTaskRunner));
  task_environment_.RunUntilIdle();
}

TEST_F(SequencedTaskRunnerHandleTest, NoHandleFromUnsequencedTask) {
  base::PostTask(base::BindOnce(
      []() { EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet()); }));
  task_environment_.RunUntilIdle();
}

TEST(SequencedTaskRunnerHandleTestWithoutTaskEnvironment, FromHandleInScope) {
  scoped_refptr<SequencedTaskRunner> test_task_runner(new TestSimpleTaskRunner);
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
  {
    SequencedTaskRunnerHandle handle(test_task_runner);
    EXPECT_TRUE(SequencedTaskRunnerHandle::IsSet());
    EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
    EXPECT_EQ(test_task_runner, SequencedTaskRunnerHandle::Get());
  }
  EXPECT_FALSE(SequencedTaskRunnerHandle::IsSet());
  EXPECT_FALSE(ThreadTaskRunnerHandle::IsSet());
}

}  // namespace
}  // namespace base
