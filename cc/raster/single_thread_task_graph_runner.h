// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_SINGLE_THREAD_TASK_GRAPH_RUNNER_H_
#define CC_RASTER_SINGLE_THREAD_TASK_GRAPH_RUNNER_H_

#include <memory>
#include <string>

#include "base/synchronization/condition_variable.h"
#include "base/thread_annotations.h"
#include "base/threading/simple_thread.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/task_graph_work_queue.h"

namespace base {
class SimpleThread;
}

namespace cc {

// Runs TaskGraphs asynchronously using a single worker thread.
class CC_EXPORT SingleThreadTaskGraphRunner
    : public TaskGraphRunner,
      public base::DelegateSimpleThread::Delegate {
 public:
  SingleThreadTaskGraphRunner();
  ~SingleThreadTaskGraphRunner() override;

  // Overridden from TaskGraphRunner:
  NamespaceToken GenerateNamespaceToken() override;
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph) override;
  void WaitForTasksToFinishRunning(NamespaceToken token) override;
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks) override;

  // Overridden from base::DelegateSimpleThread::Delegate:
  void Run() override;

  void Start(const std::string& thread_name,
             const base::SimpleThread::Options& thread_options);
  void Shutdown();

 private:
  // Returns true if there was a task to run.
  bool RunTaskWithLockAcquired() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  std::unique_ptr<base::SimpleThread> thread_;

  // Lock to exclusively access all the following members that are used to
  // implement the TaskRunner interfaces.
  base::Lock lock_;

  // Stores the actual tasks to be run by this runner, sorted by priority.
  TaskGraphWorkQueue work_queue_ GUARDED_BY(lock_);

  // Condition variable that is waited on by Run() until new tasks are ready to
  // run or shutdown starts.
  base::ConditionVariable has_ready_to_run_tasks_cv_;

  // Condition variable that is waited on by origin threads until a namespace
  // has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;

  // Set during shutdown. Tells Run() to return when no more tasks are pending.
  bool shutdown_ GUARDED_BY(lock_) = false;
};

}  // namespace cc

#endif  // CC_RASTER_SINGLE_THREAD_TASK_GRAPH_RUNNER_H_
