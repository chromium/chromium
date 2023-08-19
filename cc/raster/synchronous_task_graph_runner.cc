// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/raster/synchronous_task_graph_runner.h"

#include <stdint.h>

#include <utility>

#include "base/ranges/algorithm.h"
#include "base/threading/simple_thread.h"
#include "base/trace_event/heap_profiler.h"
#include "base/trace_event/trace_event.h"
#include "base/trace_event/typed_macros.h"
namespace cc {

SynchronousTaskGraphRunner::SynchronousTaskGraphRunner() = default;

SynchronousTaskGraphRunner::~SynchronousTaskGraphRunner() {
  DCHECK(!work_queue_.HasReadyToRunTasks());
}

NamespaceToken SynchronousTaskGraphRunner::GenerateNamespaceToken() {
  return work_queue_.GenerateNamespaceToken();
}

void SynchronousTaskGraphRunner::ScheduleTasks(NamespaceToken token,
                                               TaskGraph* graph) {
  TRACE_EVENT2("cc", "SynchronousTaskGraphRunner::ScheduleTasks", "num_nodes",
               graph->nodes.size(), "num_edges", graph->edges.size());

  DCHECK(token.IsValid());
  DCHECK(!TaskGraphWorkQueue::DependencyMismatch(graph));

  work_queue_.ScheduleTasks(token, graph);
}

void SynchronousTaskGraphRunner::WaitForTasksToFinishRunning(
    NamespaceToken token) {
  TRACE_EVENT0("cc", "SynchronousTaskGraphRunner::WaitForTasksToFinishRunning");

  DCHECK(token.IsValid());
  auto* task_namespace = work_queue_.GetNamespaceForToken(token);

  if (!task_namespace)
    return;

  while (!work_queue_.HasFinishedRunningTasksInNamespace(task_namespace)) {
    bool succeeded = RunTask();
    DCHECK(succeeded);
  }
}

void SynchronousTaskGraphRunner::CollectCompletedTasks(
    NamespaceToken token,
    Task::Vector* completed_tasks) {
  TRACE_EVENT0("cc", "SynchronousTaskGraphRunner::CollectCompletedTasks");

  DCHECK(token.IsValid());
  work_queue_.CollectCompletedTasks(token, completed_tasks);
}

void SynchronousTaskGraphRunner::RunUntilIdle() {
  while (RunTask()) {
  }
}

bool SynchronousTaskGraphRunner::RunTask() {
  // Since we do not have posted from location for tasks, record the context for
  // tasks as "cc" in heap profiler.
  TRACE_HEAP_PROFILER_API_SCOPED_TASK_EXECUTION scoped_event("cc");
  TRACE_EVENT0("toplevel", "SynchronousTaskGraphRunner::RunTask");

  // Find the first category with any tasks to run. This task graph runner
  // treats categories as an additional priority.
  const auto& ready_to_run_namespaces = work_queue_.ready_to_run_namespaces();
  auto found = base::ranges::find_if_not(
      ready_to_run_namespaces,
      &TaskGraphWorkQueue::TaskNamespace::Vector::empty,
      &TaskGraphWorkQueue::ReadyNamespaces::value_type::second);

  if (found == ready_to_run_namespaces.cend()) {
    return false;
  }

  const uint16_t category = found->first;
  auto prioritized_task = work_queue_.GetNextTaskToRun(category);
  prioritized_task.task->RunOnWorkerThread();

  TRACE_EVENT("toplevel", "cc::SynchronousTaskGraphRunner::RunTask",
              perfetto::TerminatingFlow::Global(
                  prioritized_task.task->trace_task_id()));

  work_queue_.CompleteTask(std::move(prioritized_task));

  return true;
}

}  // namespace cc
