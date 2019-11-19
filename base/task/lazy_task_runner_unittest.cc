// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/lazy_task_runner.h"

#include "base/bind.h"
#include "base/bind_helpers.h"
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

LazySequencedTaskRunner g_sequenced_task_runner_user_visible =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_VISIBLE));
LazySequencedTaskRunner g_sequenced_task_runner_user_blocking =
    LAZY_SEQUENCED_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_BLOCKING));

LazySingleThreadTaskRunner g_single_thread_task_runner_user_visible =
    LAZY_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_VISIBLE),
        SingleThreadTaskRunnerThreadMode::SHARED);
LazySingleThreadTaskRunner g_single_thread_task_runner_user_blocking =
    LAZY_SINGLE_THREAD_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_BLOCKING),
        SingleThreadTaskRunnerThreadMode::SHARED);

#if defined(OS_WIN)
LazyCOMSTATaskRunner g_com_sta_task_runner_user_visible =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_VISIBLE),
        SingleThreadTaskRunnerThreadMode::SHARED);
LazyCOMSTATaskRunner g_com_sta_task_runner_user_blocking =
    LAZY_COM_STA_TASK_RUNNER_INITIALIZER(
        TaskTraits(ThreadPool(), TaskPriority::USER_BLOCKING),
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

class LazyTaskRunnerEnvironmentTest : public testing::Test {
 protected:
  LazyTaskRunnerEnvironmentTest() = default;

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
  DISALLOW_COPY_AND_ASSIGN(LazyTaskRunnerEnvironmentTest);
};

}  // namespace

TEST_F(LazyTaskRunnerEnvironmentTest, LazySequencedTaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_sequenced_task_runner_user_visible.Get(), false,
                            TaskPriority::USER_VISIBLE);
}

TEST_F(LazyTaskRunnerEnvironmentTest, LazySequencedTaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_sequenced_task_runner_user_blocking.Get(), false,
                            TaskPriority::USER_BLOCKING);
}

TEST_F(LazyTaskRunnerEnvironmentTest, LazySingleThreadTaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_single_thread_task_runner_user_visible.Get(),
                            true, TaskPriority::USER_VISIBLE);
}

TEST_F(LazyTaskRunnerEnvironmentTest, LazySingleThreadTaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_single_thread_task_runner_user_blocking.Get(),
                            true, TaskPriority::USER_BLOCKING);
}

#if defined(OS_WIN)
TEST_F(LazyTaskRunnerEnvironmentTest, LazyCOMSTATaskRunnerUserVisible) {
  TestTaskRunnerEnvironment(g_com_sta_task_runner_user_visible.Get(), true,
                            TaskPriority::USER_VISIBLE, true);
}

TEST_F(LazyTaskRunnerEnvironmentTest, LazyCOMSTATaskRunnerUserBlocking) {
  TestTaskRunnerEnvironment(g_com_sta_task_runner_user_blocking.Get(), true,
                            TaskPriority::USER_BLOCKING, true);
}
#endif  // defined(OS_WIN)

TEST(LazyTaskRunnerTest, LazySequencedTaskRunnerReset) {
  for (int i = 0; i < 2; ++i) {
    test::TaskEnvironment task_environment;
    // If the TaskRunner isn't released when the test::TaskEnvironment
    // goes out of scope, the second invocation of the line below will access a
    // deleted ThreadPoolInstance and crash.
    g_sequenced_task_runner_user_visible.Get()->PostTask(FROM_HERE,
                                                         DoNothing());
  }
}

TEST(LazyTaskRunnerTest, LazySingleThreadTaskRunnerReset) {
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
TEST(LazyTaskRunnerTest, LazyCOMSTATaskRunnerReset) {
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
