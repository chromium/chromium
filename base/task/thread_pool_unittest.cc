// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/thread_pool.h"

#include "base/functional/bind.h"
#include "base/location.h"
#include "base/run_loop.h"
#include "base/task/bind_post_task.h"
#include "base/task/single_thread_task_executor.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/test/bind.h"
#include "base/test/task_environment.h"
#include "base/test/test_waitable_event.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ThreadPool, PostTaskAndReplyWithResultThreeArgs) {
  base::test::TaskEnvironment env;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, base::BindOnce([] { return 3; }),
      base::OnceCallback<void(int)>(
          base::BindLambdaForTesting([&run_loop](int x) {
            EXPECT_EQ(x, 3);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST(ThreadPool, PostTaskAndReplyWithResultFourArgs) {
  base::test::TaskEnvironment env;

  base::RunLoop run_loop;
  base::ThreadPool::PostTaskAndReplyWithResult(
      FROM_HERE, /*traits=*/{}, base::BindOnce([] { return 3; }),
      base::OnceCallback<void(int)>(
          base::BindLambdaForTesting([&run_loop](int x) {
            EXPECT_EQ(x, 3);
            run_loop.Quit();
          })));
  run_loop.Run();
}

TEST(ThreadPool, BindPostTaskFizzlesOnShutdown) {
  // Tests that a callback bound to a BLOCK_SHUTDOWN sequence doesn't trigger a
  // DCHECK when it's deleted without running.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "BindPostTaskFizzlesOnShutdownTest");

  {
    // Creating this callback and deleting it after the thread pool is shutdown
    // used to trigger a DCHECK in task_tracker because the
    // BindPostTaskTrampoline destructor has to delete its state on the sequence
    // it's bound to. There is a DCHECK that ensures BLOCK_SHUTDOWN tasks aren't
    // posted after shutdown, but BindPostTaskTrampoline uses
    // base::ThreadPoolInstance::ScopedFizzleBlockShutdownTasks to avoid
    // triggering it.
    auto bound_callback =
        base::BindPostTask(base::ThreadPool::CreateSequencedTaskRunner(
                               {TaskShutdownBehavior::BLOCK_SHUTDOWN}),
                           base::BindOnce([] { ADD_FAILURE(); }));
    base::ThreadPoolInstance::Get()->Shutdown();
  }

  ThreadPoolInstance::Get()->JoinForTesting();
  ThreadPoolInstance::Set(nullptr);
}

TEST(ThreadPool, PostTaskAndReplyFizzlesOnShutdown) {
  // Tests that a PostTaskAndReply from a BLOCK_SHUTDOWN sequence doesn't
  // trigger a DCHECK when it's not run at shutdown.
  base::ThreadPoolInstance::CreateAndStartWithDefaultParams(
      "PostTaskAndReplyFizzlesOnShutdown");

  {
    base::SingleThreadTaskExecutor executor;
    auto blocking_task_runner = base::ThreadPool::CreateSequencedTaskRunner(
        {TaskShutdownBehavior::BLOCK_SHUTDOWN});

    base::RunLoop run_loop;

    // The setup that this test is exercising is as follows:
    // - A task is posted using PostTaskAndReply from a BLOCK_SHUTDOWN sequence
    // to the main thread
    // - The task is not run on the main thread
    // - Shutdown happens, the ThreadPool is shutdown
    // - The main thread destroys its un-run tasks
    // - ~PostTaskAndReplyRelay calls `DeleteSoon` to delete its reply's state
    // on the sequence it's bound to
    // - TaskTracker ensures that no BLOCK_SHUTDOWN tasks can be posted after
    // shutdown. ~PostTaskAndReplyRelay avoids triggering this DCHECK by using a
    // base::ThreadPoolInstance::ScopedFizzleBlockShutdownTasks

    // Post a task to the BLOCK_SHUTDOWN thread pool sequence to setup the test.
    base::TestWaitableEvent event;
    blocking_task_runner->PostTask(
        FROM_HERE, base::BindLambdaForTesting([&] {
          // Enqueue a task whose only purpose is to exit the run loop, ensuring
          // the following task is never run.
          executor.task_runner()->PostTask(FROM_HERE,
                                           base::BindLambdaForTesting([&] {
                                             event.Wait();
                                             run_loop.Quit();
                                           }));

          // Post the task for which the reply will trigger the `DeleteSoon`
          // from `~PostTaskAndReplyRelay`.
          executor.task_runner()->PostTaskAndReply(
              FROM_HERE, base::BindOnce([] { ADD_FAILURE(); }),
              base::BindOnce([] { ADD_FAILURE(); }));

          event.Signal();
        }));

    // Run until the first task posted to the SingleThreadTaskExecutor quits the
    // run loop, resulting in the `PostTaskAndReply` being queued but not run.
    run_loop.Run();

    base::ThreadPoolInstance::Get()->Shutdown();
  }

  ThreadPoolInstance::Get()->JoinForTesting();
  ThreadPoolInstance::Set(nullptr);
}

}  // namespace base
