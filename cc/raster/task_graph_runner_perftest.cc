// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include <stddef.h>
#include <stdint.h>

#include <memory>
#include <vector>

#include "base/memory/ptr_util.h"
#include "base/time/time.h"
#include "base/timer/lap_timer.h"
#include "cc/base/completion_event.h"
#include "cc/raster/synchronous_task_graph_runner.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace cc {
namespace {

static const int kTimeLimitMillis = 2000;
static const int kWarmupRuns = 5;
static const int kTimeCheckInterval = 10;

class PerfTaskImpl : public Task {
 public:
  typedef std::vector<scoped_refptr<PerfTaskImpl>> Vector;

  PerfTaskImpl() = default;
  PerfTaskImpl(const PerfTaskImpl&) = delete;
  PerfTaskImpl& operator=(const PerfTaskImpl&) = delete;

  // Overridden from Task:
  void RunOnWorkerThread() override {}

  void Reset() { state().Reset(); }

 private:
  ~PerfTaskImpl() override = default;
};

class TaskGraphRunnerPerfTest : public testing::Test {
 public:
  TaskGraphRunnerPerfTest()
      : timer_(kWarmupRuns,
               base::Milliseconds(kTimeLimitMillis),
               kTimeCheckInterval) {}

  // Overridden from testing::Test:
  void SetUp() override {
    task_graph_runner_ = base::WrapUnique(new SynchronousTaskGraphRunner);
    namespace_token_ = task_graph_runner_->GenerateNamespaceToken();
  }
  void TearDown() override { task_graph_runner_ = nullptr; }

  void RunBuildTaskGraphTest(const std::string& test_name,
                             int num_top_level_tasks,
                             int num_tasks,
                             int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph.
    TaskGraph graph;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    CancelTasks(leaf_tasks);
    CancelTasks(tasks);
    CancelTasks(top_level_tasks);

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("build_task_graph" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

  void RunScheduleTasksTest(const std::string& test_name,
                            int num_top_level_tasks,
                            int num_tasks,
                            int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph and
    // completed tasks vector.
    TaskGraph graph;
    Task::Vector completed_tasks;

    timer_.Reset();
    do {
      graph.Reset();
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      task_graph_runner_->ScheduleTasks(namespace_token_, &graph);
      // Shouldn't be any tasks to collect as we reschedule the same set
      // of tasks.
      DCHECK_EQ(0u, CollectCompletedTasks(&completed_tasks));
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
    CollectCompletedTasks(&completed_tasks);

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("schedule_tasks" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

  void RunScheduleAlternateTasksTest(const std::string& test_name,
                                     int num_top_level_tasks,
                                     int num_tasks,
                                     int num_leaf_tasks) {
    const size_t kNumVersions = 2;
    PerfTaskImpl::Vector top_level_tasks[kNumVersions];
    PerfTaskImpl::Vector tasks[kNumVersions];
    PerfTaskImpl::Vector leaf_tasks[kNumVersions];
    for (size_t i = 0; i < kNumVersions; ++i) {
      CreateTasks(num_top_level_tasks, &top_level_tasks[i]);
      CreateTasks(num_tasks, &tasks[i]);
      CreateTasks(num_leaf_tasks, &leaf_tasks[i]);
    }

    // Avoid unnecessary heap allocations by reusing the same graph and
    // completed tasks vector.
    TaskGraph graph;
    Task::Vector completed_tasks;

    size_t count = 0;
    timer_.Reset();
    do {
      size_t current_version = count % kNumVersions;
      graph.Reset();
      // Reset tasks as we are not letting them execute, they get cancelled
      // when next ScheduleTasks() happens.
      ResetTasks(top_level_tasks[current_version]);
      ResetTasks(tasks[current_version]);
      ResetTasks(leaf_tasks[current_version]);
      BuildTaskGraph(top_level_tasks[current_version], tasks[current_version],
                     leaf_tasks[current_version], &graph);
      task_graph_runner_->ScheduleTasks(namespace_token_, &graph);
      CollectCompletedTasks(&completed_tasks);
      completed_tasks.clear();
      ++count;
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    TaskGraph empty;
    task_graph_runner_->ScheduleTasks(namespace_token_, &empty);
    CollectCompletedTasks(&completed_tasks);

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("schedule_alternate_tasks" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

  void RunScheduleAndExecuteTasksTest(const std::string& test_name,
                                      int num_top_level_tasks,
                                      int num_tasks,
                                      int num_leaf_tasks) {
    PerfTaskImpl::Vector top_level_tasks;
    PerfTaskImpl::Vector tasks;
    PerfTaskImpl::Vector leaf_tasks;
    CreateTasks(num_top_level_tasks, &top_level_tasks);
    CreateTasks(num_tasks, &tasks);
    CreateTasks(num_leaf_tasks, &leaf_tasks);

    // Avoid unnecessary heap allocations by reusing the same graph and
    // completed tasks vector.
    TaskGraph graph;
    Task::Vector completed_tasks;

    timer_.Reset();
    do {
      graph.Reset();
      // Tasks run have finished state. Reset them to be considered as new for
      // scheduling again.
      ResetTasks(top_level_tasks);
      ResetTasks(tasks);
      ResetTasks(leaf_tasks);
      BuildTaskGraph(top_level_tasks, tasks, leaf_tasks, &graph);
      task_graph_runner_->ScheduleTasks(namespace_token_, &graph);
      task_graph_runner_->RunUntilIdle();
      CollectCompletedTasks(&completed_tasks);
      completed_tasks.clear();
      timer_.NextLap();
    } while (!timer_.HasTimeLimitExpired());

    perf_test::PerfResultReporter reporter = SetUpReporter(test_name);
    reporter.AddResult("execute_tasks" + TestModifierString(),
                       timer_.LapsPerSecond());
  }

 private:
  static std::string TestModifierString() {
    return std::string("_task_graph_runner");
  }

  void CreateTasks(int num_tasks, PerfTaskImpl::Vector* tasks) {
    for (int i = 0; i < num_tasks; ++i)
      tasks->push_back(base::MakeRefCounted<PerfTaskImpl>());
  }

  void CancelTasks(const PerfTaskImpl::Vector& tasks) {
    for (auto& task : tasks)
      task->state().DidCancel();
  }

  void ResetTasks(const PerfTaskImpl::Vector& tasks) {
    for (auto& task : tasks)
      task->Reset();
  }

  void BuildTaskGraph(const PerfTaskImpl::Vector& top_level_tasks,
                      const PerfTaskImpl::Vector& tasks,
                      const PerfTaskImpl::Vector& leaf_tasks,
                      TaskGraph* graph) {
    DCHECK(graph->nodes.empty());
    DCHECK(graph->edges.empty());

    uint32_t leaf_task_count = static_cast<uint32_t>(leaf_tasks.size());
    for (auto& task : tasks) {
      for (const auto& leaf_task : leaf_tasks)
        graph->edges.emplace_back(leaf_task.get(), task.get());

      for (const auto& top_level_task : top_level_tasks)
        graph->edges.emplace_back(task.get(), top_level_task.get());

      graph->nodes.emplace_back(task, 0u, 0u, leaf_task_count);
    }

    for (auto& leaf_task : leaf_tasks)
      graph->nodes.emplace_back(leaf_task, 0u, 0u, 0u);

    uint32_t task_count = static_cast<uint32_t>(tasks.size());
    for (auto& top_level_task : top_level_tasks)
      graph->nodes.emplace_back(top_level_task, 0u, 0u, task_count);
  }

  size_t CollectCompletedTasks(Task::Vector* completed_tasks) {
    DCHECK(completed_tasks->empty());
    task_graph_runner_->CollectCompletedTasks(namespace_token_,
                                              completed_tasks);
    return completed_tasks->size();
  }

  perf_test::PerfResultReporter SetUpReporter(const std::string& story_name) {
    perf_test::PerfResultReporter reporter("", story_name);
    reporter.RegisterImportantMetric("build_task_graph" + TestModifierString(),
                                     "runs/s");
    reporter.RegisterImportantMetric("schedule_tasks" + TestModifierString(),
                                     "runs/s");
    reporter.RegisterImportantMetric(
        "schedule_alternate_tasks" + TestModifierString(), "runs/s");
    reporter.RegisterImportantMetric("execute_tasks" + TestModifierString(),
                                     "runs/s");
    return reporter;
  }

  // Test uses SynchronousTaskGraphRunner, as this implementation introduces
  // minimal additional complexity over the TaskGraphWorkQueue helpers.
  std::unique_ptr<SynchronousTaskGraphRunner> task_graph_runner_;
  NamespaceToken namespace_token_;
  base::LapTimer timer_;
};

TEST_F(TaskGraphRunnerPerfTest, BuildTaskGraph) {
  RunBuildTaskGraphTest("0_1_0", 0, 1, 0);
  RunBuildTaskGraphTest("0_32_0", 0, 32, 0);
  RunBuildTaskGraphTest("2_1_0", 2, 1, 0);
  RunBuildTaskGraphTest("2_32_0", 2, 32, 0);
  RunBuildTaskGraphTest("2_1_1", 2, 1, 1);
  RunBuildTaskGraphTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleTasks) {
  RunScheduleTasksTest("0_1_0", 0, 1, 0);
  RunScheduleTasksTest("0_32_0", 0, 32, 0);
  RunScheduleTasksTest("2_1_0", 2, 1, 0);
  RunScheduleTasksTest("2_32_0", 2, 32, 0);
  RunScheduleTasksTest("2_1_1", 2, 1, 1);
  RunScheduleTasksTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleAlternateTasks) {
  RunScheduleAlternateTasksTest("0_1_0", 0, 1, 0);
  RunScheduleAlternateTasksTest("0_32_0", 0, 32, 0);
  RunScheduleAlternateTasksTest("2_1_0", 2, 1, 0);
  RunScheduleAlternateTasksTest("2_32_0", 2, 32, 0);
  RunScheduleAlternateTasksTest("2_1_1", 2, 1, 1);
  RunScheduleAlternateTasksTest("2_32_1", 2, 32, 1);
}

TEST_F(TaskGraphRunnerPerfTest, ScheduleAndExecuteTasks) {
  RunScheduleAndExecuteTasksTest("0_1_0", 0, 1, 0);
  RunScheduleAndExecuteTasksTest("0_32_0", 0, 32, 0);
  RunScheduleAndExecuteTasksTest("2_1_0", 2, 1, 0);
  RunScheduleAndExecuteTasksTest("2_32_0", 2, 32, 0);
  RunScheduleAndExecuteTasksTest("2_1_1", 2, 1, 1);
  RunScheduleAndExecuteTasksTest("2_32_1", 2, 32, 1);
}

}  // namespace
}  // namespace cc
