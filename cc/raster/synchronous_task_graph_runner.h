// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_SYNCHRONOUS_TASK_GRAPH_RUNNER_H_
#define CC_RASTER_SYNCHRONOUS_TASK_GRAPH_RUNNER_H_

#include "cc/raster/task_graph_runner.h"
#include "cc/raster/task_graph_work_queue.h"

namespace cc {

// Implements a TaskGraphRunner which runs synchronously. This runner only runs
// tasks when RunUntilIdle is called.
class CC_EXPORT SynchronousTaskGraphRunner : public TaskGraphRunner {
 public:
  SynchronousTaskGraphRunner();
  ~SynchronousTaskGraphRunner() override;

  // Overridden from TaskGraphRunner:
  NamespaceToken GenerateNamespaceToken() override;
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph) override;
  void WaitForTasksToFinishRunning(NamespaceToken token) override;
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks) override;

  // Runs all pending tasks from all namespaces.
  void RunUntilIdle();

  // For test use only.
  bool RunSingleTaskForTesting() { return RunTask(); }

 private:
  // Returns true if there was a task to run.
  bool RunTask();

  // Stores the actual tasks to be run, sorted by priority.
  TaskGraphWorkQueue work_queue_;
};

}  // namespace cc

#endif  // CC_RASTER_SYNCHRONOUS_TASK_GRAPH_RUNNER_H_
