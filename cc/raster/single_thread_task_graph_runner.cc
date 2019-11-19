// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/single_thread_task_graph_runner.h"

#include <stdint.h>

#include <string>

#include "base/threading/simple_thread.h"
#include "base/trace_event/trace_event.h"

namespace cc {

SingleThreadTaskGraphRunner::SingleThreadTaskGraphRunner()
    : lock_(),
      has_ready_to_run_tasks_cv_(&lock_),
      has_namespaces_with_finished_running_tasks_cv_(&lock_),
      shutdown_(false) {
  has_ready_to_run_tasks_cv_.declare_only_used_while_idle();
}

SingleThreadTaskGraphRunner::~SingleThreadTaskGraphRunner() = default;

void SingleThreadTaskGraphRunner::Start(
    const std::string& thread_name,
    const base::SimpleThread::Options& thread_options) {
  thread_.reset(
      new base::DelegateSimpleThread(this, thread_name, thread_options));
  thread_->StartAsync();
}

void SingleThreadTaskGraphRunner::Shutdown() {
  {
    base::AutoLock lock(lock_);

    DCHECK(!work_queue_.HasReadyToRunTasks());
    DCHECK(!work_queue_.HasAnyNamespaces());

    DCHECK(!shutdown_);
    shutdown_ = true;

    // Wake up the worker so it knows it should exit.
    has_ready_to_run_tasks_cv_.Signal();
  }
  thread_->Join();
}

NamespaceToken SingleThreadTaskGraphRunner::GenerateNamespaceToken() {
  base::AutoLock lock(lock_);
  return work_queue_.GenerateNamespaceToken();
}

void SingleThreadTaskGraphRunner::ScheduleTasks(NamespaceToken token,
                                                TaskGraph* graph) {
  TRACE_EVENT2("cc", "SingleThreadTaskGraphRunner::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());

  DCHECK(token.IsValid());
  DCHECK(!TaskGraphWorkQueue::DependencyMismatch(graph));

  {
    base::AutoLock lock(lock_);

    DCHECK(!shutdown_);

    work_queue_.ScheduleTasks(token, graph);

    // If there is more work available, wake up the worker thread.
    if (work_queue_.HasReadyToRunTasks())
      has_ready_to_run_tasks_cv_.Signal();
  }
}

void SingleThreadTaskGraphRunner::WaitForTasksToFinishRunning(
    NamespaceToken token) {
  TRACE_EVENT0("cc",
               "SingleThreadTaskGraphRunner::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);

    auto* task_namespace = work_queue_.GetNamespaceForToken(token);

    if (!task_namespace)
      return;

    while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
      has_namespaces_with_finished_running_tasks_cv_.Wait();

    // There may be other namespaces that have finished running tasks, so wake
    // up another origin thread.
    has_namespaces_with_finished_running_tasks_cv_.Signal();
  }
}

void SingleThreadTaskGraphRunner::CollectCompletedTasks(
    NamespaceToken token,
    Task::Vector* completed_tasks) {
  TRACE_EVENT0("cc", "SingleThreadTaskGraphRunner::CollectCompletedTasks");

  DCHECK(token.IsValid());

  {
    base::AutoLock lock(lock_);
    work_queue_.CollectCompletedTasks(token, completed_tasks);
  }
}

void SingleThreadTaskGraphRunner::Run() {
  base::AutoLock lock(lock_);

  while (true) {
    if (!RunTaskWithLockAcquired()) {
      // Exit when shutdown is set and no more tasks are pending.
      if (shutdown_)
        break;

      // Wait for more tasks.
      has_ready_to_run_tasks_cv_.Wait();
      continue;
    }
  }
}

bool SingleThreadTaskGraphRunner::RunTaskWithLockAcquired() {
  TRACE_EVENT0("toplevel",
               "SingleThreadTaskGraphRunner::RunTaskWithLockAcquired");

  lock_.AssertAcquired();

  // Find the first category with any tasks to run. This task graph runner
  // treats categories as an additional priority.
  const auto& ready_to_run_namespaces = work_queue_.ready_to_run_namespaces();
  auto found = std::find_if(
      ready_to_run_namespaces.cbegin(), ready_to_run_namespaces.cend(),
      [](const std::pair<const uint16_t,
                         TaskGraphWorkQueue::TaskNamespace::Vector>& pair) {
        return !pair.second.empty();
      });

  if (found == ready_to_run_namespaces.cend()) {
    return false;
  }

  const uint16_t category = found->first;
  auto prioritized_task = work_queue_.GetNextTaskToRun(category);

  {
    base::AutoUnlock unlock(lock_);
    prioritized_task.task->RunOnWorkerThread();
  }

  auto* task_namespace = prioritized_task.task_namespace;
  work_queue_.CompleteTask(std::move(prioritized_task));

  // If namespace has finished running all tasks, wake up origin thread.
  if (work_queue_.HasFinishedRunningTasksInNamespace(task_namespace))
    has_namespaces_with_finished_running_tasks_cv_.Signal();

  return true;
}

}  // namespace cc
