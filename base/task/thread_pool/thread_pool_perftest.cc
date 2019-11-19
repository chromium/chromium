// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <atomic>
#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/bind.h"
#include "base/bind_helpers.h"
#include "base/callback.h"
#include "base/optional.h"
#include "base/synchronization/waitable_event.h"
#include "base/task/post_task.h"
#include "base/task/thread_pool/thread_pool_instance.h"
#include "base/threading/simple_thread.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_test.h"

namespace base {
namespace internal {

namespace {

enum class ExecutionMode {
  // Allows tasks to start running while tasks are being posted by posting
  // threads.
  kPostAndRun,
  // Uses an execution fence to wait for all posting threads to be done before
  // running tasks that were posted.
  kPostThenRun,
};

// A thread that waits for the caller to signal an event before proceeding to
// call action.Run().
class PostingThread : public SimpleThread {
 public:
  // Creates a PostingThread that waits on |start_event| before calling
  // action.Run().
  PostingThread(WaitableEvent* start_event,
                base::OnceClosure action,
                base::OnceClosure completion)
      : SimpleThread("PostingThread"),
        start_event_(start_event),
        action_(std::move(action)),
        completion_(std::move(completion)) {
    Start();
  }

  void Run() override {
    start_event_->Wait();
    std::move(action_).Run();
    std::move(completion_).Run();
  }

 private:
  WaitableEvent* const start_event_;
  base::OnceClosure action_;
  base::OnceClosure completion_;

  DISALLOW_COPY_AND_ASSIGN(PostingThread);
};

class ThreadPoolPerfTest : public testing::Test {
 public:
  // Posting actions:

  void ContinuouslyBindAndPostNoOpTasks(size_t num_tasks) {
    scoped_refptr<TaskRunner> task_runner = CreateTaskRunner({ThreadPool()});
    for (size_t i = 0; i < num_tasks; ++i) {
      ++num_tasks_pending_;
      ++num_posted_tasks_;
      task_runner->PostTask(FROM_HERE,
                            base::BindOnce(
                                [](std::atomic_size_t* num_task_pending) {
                                  (*num_task_pending)--;
                                },
                                &num_tasks_pending_));
    }
  }

  void ContinuouslyPostNoOpTasks(size_t num_tasks) {
    scoped_refptr<TaskRunner> task_runner = CreateTaskRunner({ThreadPool()});
    base::RepeatingClosure closure = base::BindRepeating(
        [](std::atomic_size_t* num_task_pending) { (*num_task_pending)--; },
        &num_tasks_pending_);
    for (size_t i = 0; i < num_tasks; ++i) {
      ++num_tasks_pending_;
      ++num_posted_tasks_;
      task_runner->PostTask(FROM_HERE, closure);
    }
  }

  void ContinuouslyPostBusyWaitTasks(size_t num_tasks,
                                     base::TimeDelta duration) {
    scoped_refptr<TaskRunner> task_runner = CreateTaskRunner({ThreadPool()});
    base::RepeatingClosure closure = base::BindRepeating(
        [](std::atomic_size_t* num_task_pending, base::TimeDelta duration) {
          base::TimeTicks end_time = base::TimeTicks::Now() + duration;
          while (base::TimeTicks::Now() < end_time)
            ;
          (*num_task_pending)--;
        },
        Unretained(&num_tasks_pending_), duration);
    for (size_t i = 0; i < num_tasks; ++i) {
      ++num_tasks_pending_;
      ++num_posted_tasks_;
      task_runner->PostTask(FROM_HERE, closure);
    }
  }

 protected:
  ThreadPoolPerfTest() { ThreadPoolInstance::Create("PerfTest"); }

  ~ThreadPoolPerfTest() override { ThreadPoolInstance::Set(nullptr); }

  void StartThreadPool(size_t num_running_threads,
                       size_t num_posting_threads,
                       base::RepeatingClosure post_action) {
    ThreadPoolInstance::Get()->Start({num_running_threads});

    base::RepeatingClosure done = BarrierClosure(
        num_posting_threads,
        base::BindOnce(&ThreadPoolPerfTest::OnCompletePostingTasks,
                       base::Unretained(this)));

    for (size_t i = 0; i < num_posting_threads; ++i) {
      threads_.emplace_back(std::make_unique<PostingThread>(
          &start_posting_tasks_, post_action, done));
    }
  }

  void OnCompletePostingTasks() { complete_posting_tasks_.Signal(); }

  void Benchmark(const std::string& trace, ExecutionMode execution_mode) {
    base::Optional<ThreadPoolInstance::ScopedExecutionFence> execution_fence;
    if (execution_mode == ExecutionMode::kPostThenRun) {
      execution_fence.emplace();
    }
    TimeTicks tasks_run_start = TimeTicks::Now();
    start_posting_tasks_.Signal();
    complete_posting_tasks_.Wait();
    post_task_duration_ = TimeTicks::Now() - tasks_run_start;

    if (execution_mode == ExecutionMode::kPostThenRun) {
      tasks_run_start = TimeTicks::Now();
      execution_fence.reset();
    }

    // Wait for no pending tasks.
    ThreadPoolInstance::Get()->FlushForTesting();
    tasks_run_duration_ = TimeTicks::Now() - tasks_run_start;
    ASSERT_EQ(0U, num_tasks_pending_);

    for (auto& thread : threads_)
      thread->Join();
    ThreadPoolInstance::Get()->JoinForTesting();

    perf_test::PrintResult(
        "Posting tasks throughput", "", trace,
        num_posted_tasks_ /
            static_cast<double>(post_task_duration_.InMilliseconds()),
        "tasks/ms", true);
    perf_test::PrintResult(
        "Running tasks throughput", "", trace,
        num_posted_tasks_ /
            static_cast<double>(tasks_run_duration_.InMilliseconds()),
        "tasks/ms", true);
    perf_test::PrintResult("Num tasks posted", "", trace, num_posted_tasks_,
                           "tasks", true);
  }

 private:
  WaitableEvent start_posting_tasks_;
  WaitableEvent complete_posting_tasks_;

  TimeDelta post_task_duration_;
  TimeDelta tasks_run_duration_;

  std::atomic_size_t num_tasks_pending_{0};
  std::atomic_size_t num_posted_tasks_{0};

  std::vector<std::unique_ptr<PostingThread>> threads_;

  DISALLOW_COPY_AND_ASSIGN(ThreadPoolPerfTest);
};

}  // namespace

TEST_F(ThreadPoolPerfTest, BindPostThenRunNoOpTasks) {
  StartThreadPool(
      1, 1,
      BindRepeating(&ThreadPoolPerfTest::ContinuouslyBindAndPostNoOpTasks,
                    Unretained(this), 10000));
  Benchmark("Bind+Post-then-run no-op tasks", ExecutionMode::kPostThenRun);
}

TEST_F(ThreadPoolPerfTest, PostThenRunNoOpTasks) {
  StartThreadPool(1, 1,
                  BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostNoOpTasks,
                                Unretained(this), 10000));
  Benchmark("Post-then-run no-op tasks", ExecutionMode::kPostThenRun);
}

TEST_F(ThreadPoolPerfTest, PostThenRunNoOpTasksManyThreads) {
  StartThreadPool(4, 4,
                  BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostNoOpTasks,
                                Unretained(this), 10000));
  Benchmark("Post-then-run no-op tasks many threads",
            ExecutionMode::kPostThenRun);
}

TEST_F(ThreadPoolPerfTest, PostThenRunNoOpTasksMorePostingThanRunningThreads) {
  StartThreadPool(1, 4,
                  BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostNoOpTasks,
                                Unretained(this), 10000));
  Benchmark("Post-then-run no-op tasks more posting than running threads",
            ExecutionMode::kPostThenRun);
}

TEST_F(ThreadPoolPerfTest, PostRunNoOpTasks) {
  StartThreadPool(1, 1,
                  BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostNoOpTasks,
                                Unretained(this), 10000));
  Benchmark("Post/run no-op tasks", ExecutionMode::kPostAndRun);
}

TEST_F(ThreadPoolPerfTest, PostRunNoOpTasksManyThreads) {
  StartThreadPool(4, 4,
                  BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostNoOpTasks,
                                Unretained(this), 10000));
  Benchmark("Post/run no-op tasks many threads", ExecutionMode::kPostAndRun);
}

TEST_F(ThreadPoolPerfTest, PostRunBusyTasksManyThreads) {
  StartThreadPool(
      4, 4,
      BindRepeating(&ThreadPoolPerfTest::ContinuouslyPostBusyWaitTasks,
                    Unretained(this), 10000,
                    base::TimeDelta::FromMicroseconds(200)));
  Benchmark("Post/run busy tasks many threads", ExecutionMode::kPostAndRun);
}

}  // namespace internal
}  // namespace base
