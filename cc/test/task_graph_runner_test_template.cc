// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/test/task_graph_runner_test_template.h"

namespace cc {

const int TaskGraphRunnerTestBase::kNamespaceCount;

void TaskGraphRunnerTestBase::SetTaskGraphRunner(
    TaskGraphRunner* task_graph_runner) {
  task_graph_runner_ = task_graph_runner;
}

void TaskGraphRunnerTestBase::ResetIds(int namespace_index) {
  run_task_ids_[namespace_index].clear();
  on_task_completed_ids_[namespace_index].clear();
}

void TaskGraphRunnerTestBase::RunAllTasks(int namespace_index) {
  task_graph_runner_->WaitForTasksToFinishRunning(
      namespace_token_[namespace_index]);

  Task::Vector completed_tasks;
  task_graph_runner_->CollectCompletedTasks(namespace_token_[namespace_index],
                                            &completed_tasks);
  for (Task::Vector::const_iterator it = completed_tasks.begin();
       it != completed_tasks.end(); ++it) {
    FakeTaskImpl* task = static_cast<FakeTaskImpl*>(it->get());
    task->OnTaskCompleted();
  }
}

void TaskGraphRunnerTestBase::RunTaskOnWorkerThread(int namespace_index,
                                                    unsigned id) {
  base::AutoLock lock(run_task_ids_lock_);
  run_task_ids_[namespace_index].push_back(id);
}

void TaskGraphRunnerTestBase::OnTaskCompleted(int namespace_index,
                                              unsigned id) {
  on_task_completed_ids_[namespace_index].push_back(id);
}

const std::vector<unsigned>& TaskGraphRunnerTestBase::run_task_ids(
    int namespace_index) {
  return run_task_ids_[namespace_index];
}

const std::vector<unsigned>& TaskGraphRunnerTestBase::on_task_completed_ids(
    int namespace_index) {
  return on_task_completed_ids_[namespace_index];
}

void TaskGraphRunnerTestBase::ScheduleTasks(
    int namespace_index,
    const std::vector<TaskInfo>& tasks) {
  Task::Vector new_tasks;
  Task::Vector new_dependents;
  TaskGraph new_graph;

  for (auto it = tasks.begin(); it != tasks.end(); ++it) {
    scoped_refptr<FakeTaskImpl> new_task(
        new FakeTaskImpl(this, it->namespace_index, it->id));
    new_graph.nodes.push_back(
        TaskGraph::Node(new_task.get(), it->category, it->priority, 0u));
    for (unsigned i = 0; i < it->dependent_count; ++i) {
      scoped_refptr<FakeDependentTaskImpl> new_dependent_task(
          new FakeDependentTaskImpl(this, it->namespace_index,
                                    it->dependent_id));
      new_graph.nodes.push_back(TaskGraph::Node(
          new_dependent_task.get(), it->category, it->priority, 1u));
      new_graph.edges.push_back(
          TaskGraph::Edge(new_task.get(), new_dependent_task.get()));

      new_dependents.push_back(new_dependent_task.get());
    }

    new_tasks.push_back(new_task.get());
  }

  task_graph_runner_->ScheduleTasks(namespace_token_[namespace_index],
                                    &new_graph);

  dependents_[namespace_index].swap(new_dependents);
  tasks_[namespace_index].swap(new_tasks);
}

void TaskGraphRunnerTestBase::FakeTaskImpl::RunOnWorkerThread() {
  test_->RunTaskOnWorkerThread(namespace_index_, id_);
}

void TaskGraphRunnerTestBase::FakeTaskImpl::OnTaskCompleted() {
  test_->OnTaskCompleted(namespace_index_, id_);
}

// These suites are instantiated in binaries that use //cc:test_support.
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(TaskGraphRunnerTest);
GTEST_ALLOW_UNINSTANTIATED_PARAMETERIZED_TEST(SingleThreadTaskGraphRunnerTest);

}  // namespace cc
