// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_TASK_GRAPH_RUNNER_H_
#define CC_RASTER_TASK_GRAPH_RUNNER_H_

#include <stddef.h>
#include <stdint.h>

#include "cc/cc_export.h"
#include "cc/raster/task.h"

namespace cc {

// Opaque identifier that defines a namespace of tasks.
class CC_EXPORT NamespaceToken {
 public:
  NamespaceToken() : id_(0) {}
  ~NamespaceToken() {}

  bool IsValid() const { return id_ != 0; }

 private:
  friend class TaskGraphWorkQueue;

  explicit NamespaceToken(int id) : id_(id) {}

  int id_;
};

// A TaskGraphRunner is an object that runs a set of tasks in the
// order defined by a dependency graph. The TaskGraphRunner interface
// provides a way of decoupling task scheduling from the mechanics of
// how each task will be run. TaskGraphRunner provides the following
// guarantees:
//
//   - Scheduled tasks will not run synchronously. That is, the
//     ScheduleTasks() method will not call Task::Run() directly.
//
//   - Scheduled tasks are guaranteed to run in the order defined by
//     dependency graph.
//
//   - Once scheduled, a task is guaranteed to end up in the
//     |completed_tasks| vector populated by CollectCompletedTasks(),
//     even if it later gets canceled by another call to ScheduleTasks().
//
// TaskGraphRunner does not guarantee that a task with high priority
// runs after a task with low priority. The priority is just a hint
// and a valid TaskGraphRunner might ignore it. Also it does not
// guarantee a memory model for shared data between tasks. (In other
// words, you should use your own synchronization/locking primitives if
// you need to share data between tasks.)
//
// Implementations of TaskGraphRunner should be thread-safe in that all
// methods must be safe to call on any thread.
//
// Some theoretical implementations of TaskGraphRunner:
//
//   - A TaskGraphRunner that uses a thread pool to run scheduled tasks.
//
//   - A TaskGraphRunner that has a method Run() that runs each task.
class CC_EXPORT TaskGraphRunner {
 public:
  virtual ~TaskGraphRunner() {}

  // Returns a unique token that can be used to pass a task graph to
  // ScheduleTasks(). Valid tokens are always nonzero.
  virtual NamespaceToken GenerateNamespaceToken() = 0;

  // Schedule running of tasks in |graph|. Tasks previously scheduled but no
  // longer needed will be canceled unless already running. Canceled tasks are
  // moved to |completed_tasks| without being run. The result is that once
  // scheduled, a task is guaranteed to end up in the |completed_tasks| queue
  // even if it later gets canceled by another call to ScheduleTasks().
  virtual void ScheduleTasks(NamespaceToken token, TaskGraph* graph) = 0;

  // Wait for all scheduled tasks to finish running.
  virtual void WaitForTasksToFinishRunning(NamespaceToken token) = 0;

  // Collect all completed tasks in |completed_tasks|.
  virtual void CollectCompletedTasks(NamespaceToken token,
                                     Task::Vector* completed_tasks) = 0;
};

}  // namespace cc

#endif  // CC_RASTER_TASK_GRAPH_RUNNER_H_
