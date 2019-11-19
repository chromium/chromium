// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/single_thread_task_executor.h"

#include "base/run_loop.h"
#include "base/task/post_task.h"
#include "base/test/bind_test_util.h"
#include "base/test/gtest_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::IsNull;
using ::testing::NotNull;

namespace base {

TEST(SingleThreadTaskExecutorTest, GetTaskExecutorForCurrentThread) {
  EXPECT_THAT(GetTaskExecutorForCurrentThread(), IsNull());

  {
    SingleThreadTaskExecutor single_thread_task_executor;
    EXPECT_THAT(GetTaskExecutorForCurrentThread(), NotNull());
  }

  EXPECT_THAT(GetTaskExecutorForCurrentThread(), IsNull());
}

TEST(SingleThreadTaskExecutorTest,
     GetTaskExecutorForCurrentThreadInPostedTask) {
  SingleThreadTaskExecutor single_thread_task_executor;
  TaskExecutor* task_executor = GetTaskExecutorForCurrentThread();

  EXPECT_THAT(task_executor, NotNull());

  RunLoop run_loop;
  single_thread_task_executor.task_runner()->PostTask(
      FROM_HERE, BindLambdaForTesting([&]() {
        EXPECT_EQ(GetTaskExecutorForCurrentThread(), task_executor);
        run_loop.Quit();
      }));

  run_loop.Run();
}

TEST(SingleThreadTaskExecutorTest, CurrentThread) {
  SingleThreadTaskExecutor single_thread_task_executor;

  EXPECT_EQ(single_thread_task_executor.task_runner(),
            base::CreateSingleThreadTaskRunner({base::CurrentThread()}));

  // There's only one task queue so priority is ignored.
  EXPECT_EQ(single_thread_task_executor.task_runner(),
            base::CreateSingleThreadTaskRunner(
                {base::CurrentThread(), base::TaskPriority::BEST_EFFORT}));
}

TEST(SingleThreadTaskExecutorTest, GetContinuationTaskRunner) {
  SingleThreadTaskExecutor single_thread_task_executor;
  RunLoop run_loop;

  auto task_runner = CreateSingleThreadTaskRunner({CurrentThread()});

  task_runner->PostTask(FROM_HERE, BindLambdaForTesting([&]() {
                          EXPECT_EQ(task_runner, GetContinuationTaskRunner());
                          run_loop.Quit();
                        }));

  run_loop.Run();
}

TEST(SingleThreadTaskExecutorTest, GetCurrentTaskWithNoTaskRunning) {
  SingleThreadTaskExecutor single_thread_task_executor;

  EXPECT_DCHECK_DEATH(GetContinuationTaskRunner());
}

}  // namespace base
