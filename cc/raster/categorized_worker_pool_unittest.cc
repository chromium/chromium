// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/categorized_worker_pool.h"

#include <utility>

#include "base/functional/callback.h"
#include "base/task/sequenced_task_runner.h"
#include "base/test/bind.h"
#include "base/test/sequenced_task_runner_test_template.h"
#include "base/test/task_runner_test_template.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "cc/test/task_graph_runner_test_template.h"

namespace cc {
namespace {

// Number of threads spawned in tests.
const int kNumThreads = 4;

template <class T>
class CategorizedWorkerPoolTestDelegate {
 public:
  CategorizedWorkerPoolTestDelegate() = default;

  void StartTaskRunner() { categorized_worker_pool_->Start(kNumThreads); }

  scoped_refptr<CategorizedWorkerPool> GetTaskRunner() {
    return categorized_worker_pool_;
  }

  void StopTaskRunner() { categorized_worker_pool_->FlushForTesting(); }

  ~CategorizedWorkerPoolTestDelegate() { categorized_worker_pool_->Shutdown(); }

 private:
  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_ =
      base::MakeRefCounted<T>();
};

template <class T>
class CategorizedWorkerPoolSequencedTestDelegate {
 public:
  CategorizedWorkerPoolSequencedTestDelegate() = default;

  void StartTaskRunner() { categorized_worker_pool_->Start(kNumThreads); }

  scoped_refptr<base::SequencedTaskRunner> GetTaskRunner() {
    return categorized_worker_pool_->CreateSequencedTaskRunner();
  }

  void StopTaskRunner() { categorized_worker_pool_->FlushForTesting(); }

  ~CategorizedWorkerPoolSequencedTestDelegate() {
    categorized_worker_pool_->Shutdown();
  }

 private:
  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_ =
      base::MakeRefCounted<T>();
};

template <class T, int NumThreads>
class CategorizedWorkerPoolTaskGraphRunnerTestDelegate {
 public:
  CategorizedWorkerPoolTaskGraphRunnerTestDelegate() = default;

  void StartTaskGraphRunner() { categorized_worker_pool_->Start(kNumThreads); }

  TaskGraphRunner* GetTaskGraphRunner() {
    return categorized_worker_pool_->GetTaskGraphRunner();
  }

  void StopTaskGraphRunner() { categorized_worker_pool_->FlushForTesting(); }

  ~CategorizedWorkerPoolTaskGraphRunnerTestDelegate() {
    categorized_worker_pool_->Shutdown();
  }

 private:
  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_ =
      base::MakeRefCounted<T>();
};

class CategorizedWorkerPoolTest : public testing::TestWithParam<bool> {
 protected:
  CategorizedWorkerPoolTest() = default;
  ~CategorizedWorkerPoolTest() override = default;

  void SetUp() override {
    categorized_worker_pool_ =
        GetParam() ? scoped_refptr<CategorizedWorkerPool>(
                         base::MakeRefCounted<CategorizedWorkerPoolImpl>())
                   : scoped_refptr<CategorizedWorkerPool>(
                         base::MakeRefCounted<CategorizedWorkerPoolJob>());
    categorized_worker_pool_->Start(kNumThreads);
    namespace_token_ = categorized_worker_pool_->GenerateNamespaceToken();
  }

  void TearDown() override {
    Task::Vector completed_tasks;
    categorized_worker_pool_->CollectCompletedTasks(namespace_token_,
                                                    &completed_tasks);
    categorized_worker_pool_->Shutdown();
  }

  scoped_refptr<CategorizedWorkerPool> categorized_worker_pool_;
  NamespaceToken namespace_token_;
};

class ClosureTask : public Task {
 public:
  explicit ClosureTask(base::OnceClosure closure)
      : closure_(std::move(closure)) {}

  ClosureTask(const ClosureTask&) = delete;
  ClosureTask& operator=(const ClosureTask&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override { std::move(closure_).Run(); }

 protected:
  ~ClosureTask() override = default;

 private:
  base::OnceClosure closure_;
};

}  // namespace

// Verify that multiple tasks posted with TASK_CATEGORY_BACKGROUND and
// TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY don't run
// concurrently.
TEST_P(CategorizedWorkerPoolTest, BackgroundTasksDontRunConcurrently) {
  Task::Vector tasks;
  TaskGraph graph;
  bool is_running_task = false;

  for (size_t i = 0; i < 100; ++i) {
    tasks.push_back(
        base::MakeRefCounted<ClosureTask>(base::BindLambdaForTesting([&]() {
          // Rely on TSAN to warn if reading |was_running_task| is racy. It
          // shouldn't if only one background task runs at a time.
          EXPECT_FALSE(is_running_task);
          is_running_task = true;
          base::PlatformThread::Sleep(base::Milliseconds(10));
          is_running_task = false;
        })));

    graph.nodes.push_back(TaskGraph::Node(
        tasks.back(),
        i % 2 == 0 ? TASK_CATEGORY_BACKGROUND
                   : TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY,
        /* priority=*/0u, /* dependencies=*/0u));
  }

  categorized_worker_pool_->ScheduleTasks(namespace_token_, &graph);
  categorized_worker_pool_->WaitForTasksToFinishRunning(namespace_token_);
  categorized_worker_pool_->FlushForTesting();
}

// Verify that a TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY task
// doesn't run at background thread priority.
TEST_P(CategorizedWorkerPoolTest,
       AcquiresForegroundResourcesNotBackgroundThreadPriority) {
  Task::Vector tasks;
  TaskGraph graph;

  tasks.push_back(base::MakeRefCounted<ClosureTask>(base::BindOnce([]() {
    EXPECT_NE(base::ThreadType::kBackground,
              base::PlatformThread::GetCurrentThreadType());
  })));
  graph.nodes.push_back(TaskGraph::Node(
      tasks.back(), TASK_CATEGORY_BACKGROUND_WITH_NORMAL_THREAD_PRIORITY,
      /* priority=*/0u, /* dependencies=*/0u));

  categorized_worker_pool_->ScheduleTasks(namespace_token_, &graph);
  categorized_worker_pool_->WaitForTasksToFinishRunning(namespace_token_);
  categorized_worker_pool_->FlushForTesting();
}

INSTANTIATE_TEST_SUITE_P(All,
                         CategorizedWorkerPoolTest,
                         testing::Values(false, true));

}  // namespace cc

// Test suite instantiation needs to be in the same namespace as test suite
// definition.

namespace base {

INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolImpl,
    TaskRunnerTest,
    cc::CategorizedWorkerPoolTestDelegate<cc::CategorizedWorkerPoolImpl>);
INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolJob,
    TaskRunnerTest,
    cc::CategorizedWorkerPoolTestDelegate<cc::CategorizedWorkerPoolJob>);

INSTANTIATE_TYPED_TEST_SUITE_P(CategorizedWorkerPoolImpl,
                               SequencedTaskRunnerTest,
                               cc::CategorizedWorkerPoolSequencedTestDelegate<
                                   cc::CategorizedWorkerPoolImpl>);
INSTANTIATE_TYPED_TEST_SUITE_P(CategorizedWorkerPoolJob,
                               SequencedTaskRunnerTest,
                               cc::CategorizedWorkerPoolSequencedTestDelegate<
                                   cc::CategorizedWorkerPoolJob>);

}  // namespace base

namespace cc {

// Multithreaded tests.
using CategorizedWorkerPoolImplTaskGraphRunnerTestDelegate_1_5 =
    ::testing::Types<CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolImpl,
                         1>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolImpl,
                         2>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolImpl,
                         3>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolImpl,
                         4>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolImpl,
                         5>>;

INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolImpl_1_5_Threads,
    TaskGraphRunnerTest,
    CategorizedWorkerPoolImplTaskGraphRunnerTestDelegate_1_5);
using CategorizedWorkerPoolJobTaskGraphRunnerTestDelegate_1_5 =
    ::testing::Types<CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolJob,
                         1>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolJob,
                         2>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolJob,
                         3>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolJob,
                         4>,
                     CategorizedWorkerPoolTaskGraphRunnerTestDelegate<
                         CategorizedWorkerPoolJob,
                         5>>;
INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolJob_1_5_Threads,
    TaskGraphRunnerTest,
    CategorizedWorkerPoolJobTaskGraphRunnerTestDelegate_1_5);

// Single threaded tests.
using CategorizedWorkerPoolImplTaskGraphRunnerTestDelegate =
    CategorizedWorkerPoolTaskGraphRunnerTestDelegate<CategorizedWorkerPoolImpl,
                                                     1>;
INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolImpl,
    SingleThreadTaskGraphRunnerTest,
    CategorizedWorkerPoolImplTaskGraphRunnerTestDelegate);
using CategorizedWorkerPoolJobTaskGraphRunnerTestDelegate =
    CategorizedWorkerPoolTaskGraphRunnerTestDelegate<CategorizedWorkerPoolJob,
                                                     1>;
INSTANTIATE_TYPED_TEST_SUITE_P(
    CategorizedWorkerPoolJob,
    SingleThreadTaskGraphRunnerTest,
    CategorizedWorkerPoolJobTaskGraphRunnerTestDelegate);

}  // namespace cc
