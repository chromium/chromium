// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/lazy_thread_pool_task_runner.h"

#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/sequence_checker_impl.h"
#include "base/task/scoped_set_task_priority_for_current_thread.h"
#include "base/test/task_environment.h"
#include "base/threading/thread_checker_impl.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if defined(OS_WIN)
#include "base/win/com_init_util.h"
#endif

namespace base {

namespace {

LazyThreadPoolSequencedTaskRunner g_sequenced_task_runner_user_visible =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_VISIBLE));
LazyThreadPoolSequencedTaskRunner g_sequenced_task_runner_user_blocking =
    LAZY_THREAD_POOL_SEQUENCED_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_BLOCKING));

LazyThreadPoolSingleThreadTaskRunner g_single_thread_task_runner_user_visible =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_VISIBLE),
        SingleThreadTaskRunnerThreadMode::SHARED);
LazyThreadPoolSingleThreadTaskRunner g_single_thread_task_runner_user_blocking =
    LAZY_THREAD_POOL_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_BLOCKING),
        SingleThreadTaskRunnerThreadMode::SHARED);

#if defined(OS_WIN)
LazyThreadPoolCOMSTATaskRunner g_com_sta_task_runner_user_visible =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_VISIBLE),
        SingleThreadTaskRunnerThreadMode::SHARED);
LazyThreadPoolCOMSTATaskRunner g_com_sta_task_runner_user_blocking =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        TaskTraits(TaskPriority::USER_BLOCKING),
        SingleThreadTaskRunnerThreadMode::SHARED);
#endif  // defined(OS_WIN)

void InitCheckers(SequenceCheckerImpl* sequence_checker,
                  ThreadCheckerImpl* thread_checker) {
  sequence_checker->DetachFromSequence();
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  thread_checker->DetachFromThread();
  EXPECT_TRUE(thread_checker->CalledOnValidThread());
}

void ExpectSequencedEnvironment(SequenceCheckerImpl* sequence_checker,
                                ThreadCheckerImpl* thread_checker,
                                TaskPriority expected_priority) {
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  EXPECT_FALSE(thread_checker->CalledOnValidThread());
  EXPECT_EQ(expected_priority, internal::GetTaskPriorityForCurrentThread());
}

void ExpectSingleThreadEnvironment(SequenceCheckerImpl* sequence_checker,
                                   ThreadCheckerImpl* thread_checker,
                                   TaskPriority expected_priority
#if defined(OS_WIN)
                                   ,
                                   bool expect_com_sta = false
#endif
) {
  EXPECT_TRUE(sequence_checker->CalledOnValidSequence());
  EXPECT_TRUE(thread_checker->CalledOnValidThread());
  EXPECT_EQ(expected_priority, internal::GetTaskPriorityForCurrentThread());

#if defined(OS_WIN)
  if (expect_com_sta)
    win::AssertComApartmentType(win::ComApartmentType::STA);
#endif
}

class LazyThreadPoolTaskRunnerEnvironmentTest : public testing::Test {
 protected:
  LazyThreadPoolTaskRunnerEnvironmentTest() = default;

  void TestTaskRunnerEnvironment(scoped_refptr<SequencedTaskRunner> task_runner,
                                 bool expect_single_thread,
                                 TaskPriority expected_priority
#if defined(OS_WIN)
                                 ,
                                 bool expect_com_sta = false
#endif
  ) {
    SequenceCheckerImpl sequence_checker;
    ThreadCheckerImpl thread_checker;
    task_runner->PostTask(FROM_HERE,
                          BindOnce(&InitCheckers, Unretained(&sequence_checker),
                                   Unretained(&thread_checker)));
    task_environment_.RunUntilIdle();

    OnceClosure task =
        expect_single_thread
            ? BindOnce(&ExpectSingleThreadEnvironment,
                       Unretained(&sequence_checker),
                       Unretained(&thread_checker), expected_priority
#if defined(OS_WIN)
                       ,
                       expect_com_sta
#endif
                       )
            : BindOnce(&ExpectSequencedEnvironment,
                       Unretained(&sequence_checker),
                       Unretained(&thread_checker), expected_priority);
    task_runner->PostTask(FROM_HERE, std::move(task));
    task_environment_.RunUntilIdle();
  }

  test::TaskEnvironment task_environment_;

 private:
  DISALLOW_COPY_AND_ASSIGN(LazyThreadPoolTaskRunnerEnvironmentTest);
};

}  // namespace

TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolSequencedTaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_sequenced_task_runner_user_visible.Get(), false,
                            TaskPriority::USER_VISIBLE);
}

TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolSequencedTaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_sequenced_task_runner_user_blocking.Get(), false,
                            TaskPriority::USER_BLOCKING);
}

TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolSingleThreadTaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_single_thread_task_runner_user_visible.Get(),
                            true, TaskPriority::USER_VISIBLE);
}

TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolSingleThreadTaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_single_thread_task_runner_user_blocking.Get(),
                            true, TaskPriority::USER_BLOCKING);
}

#if defined(OS_WIN)
TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolCOMSTATaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_com_sta_task_runner_user_visible.Get(), true,
                            TaskPriority::USER_VISIBLE, true);
}

TEST_F(LazyThreadPoolTaskRunnerEnvironmentTest,
       LazyThreadPoolCOMSTATaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_com_sta_task_runner_user_blocking.Get(), true,
                            TaskPriority::USER_BLOCKING, true);
}
#endif  // defined(OS_WIN)

TEST(LazyThreadPoolTaskRunnerTest, LazyThreadPoolSequencedTaskRunnerReset) {
  for (int i = 0; i < 2; ++i) {
    test::TaskEnvironment task_environment;
    // If the TaskRunner isn't released when the test::TaskEnvironment
    // goes out of scope, the second invocation of the line below will access a
    // deleted ThreadPoolInstance and crash.
    g_sequenced_task_runner_user_visible.Get()->PostTask(FROM_HERE,
                                                         DoNothing());
  }
}

TEST(LazyThreadPoolTaskRunnerTest, LazyThreadPoolSingleThreadTaskRunnerReset) {
  for (int i = 0; i < 2; ++i) {
    test::TaskEnvironment task_environment;
    // If the TaskRunner isn't released when the test::TaskEnvironment
    // goes out of scope, the second invocation of the line below will access a
    // deleted ThreadPoolInstance and crash.
    g_single_thread_task_runner_user_visible.Get()->PostTask(FROM_HERE,
                                                             DoNothing());
  }
}

#if defined(OS_WIN)
TEST(LazyThreadPoolTaskRunnerTest, LazyThreadPoolCOMSTATaskRunnerReset) {
  for (int i = 0; i < 2; ++i) {
    test::TaskEnvironment task_environment;
    // If the TaskRunner isn't released when the test::TaskEnvironment
    // goes out of scope, the second invocation of the line below will access a
    // deleted ThreadPoolInstance and crash.
    g_com_sta_task_runner_user_visible.Get()->PostTask(FROM_HERE, DoNothing());
  }
}
#endif  // defined(OS_WIN)

}  // namespace base
