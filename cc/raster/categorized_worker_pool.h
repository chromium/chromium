// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_RASTER_CATEGORIZED_WORKER_POOL_H_
#define CC_RASTER_CATEGORIZED_WORKER_POOL_H_

#include <memory>
#include <optional>
#include <vector>

#include "base/containers/span.h"
#include "base/functional/callback.h"
#include "base/memory/raw_ptr.h"
#include "base/synchronization/condition_variable.h"
#include "base/task/post_job.h"
#include "base/task/sequenced_task_runner.h"
#include "base/task/task_runner.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "cc/cc_export.h"
#include "cc/raster/task_category.h"
#include "cc/raster/task_graph_runner.h"
#include "cc/raster/task_graph_work_queue.h"

namespace cc {

// A pool of threads used to run categorized work. The work can be scheduled on
// the threads using different interfaces.
// 1. The pool itself implements TaskRunner interface and tasks posted via that
//    interface might run in parallel.
// 2. The pool also implements TaskGraphRunner interface which allows to
//    schedule a graph of tasks with their dependencies.
// 3. CreateSequencedTaskRunner() creates a sequenced task runner that might run
//    in parallel with other instances of sequenced task runners.
class CC_EXPORT CategorizedWorkerPool : public base::TaskRunner,
                                        public TaskGraphRunner {
 public:
  class CC_EXPORT Delegate {
   public:
    virtual ~Delegate() = default;

    // Called on the delegate with a worker pool thread ID as soon as the
    // thread is created.
    virtual void NotifyThreadWillRun(base::PlatformThreadId tid) = 0;
  };

  CategorizedWorkerPool();

  // Get or create the singleton worker pool. This object lives forever. If
  // `delegate` is non-null, it must also live forever.
  static CategorizedWorkerPool* GetOrCreate(Delegate* delegate = nullptr);

  // Overridden from TaskGraphRunner:
  NamespaceToken GenerateNamespaceToken() override;
  void WaitForTasksToFinishRunning(NamespaceToken token) override;
  void CollectCompletedTasks(NamespaceToken token,
                             Task::Vector* completed_tasks) override;

  virtual void FlushForTesting() = 0;

  virtual void Start(int max_concurrency_foreground) = 0;

  // Finish running all the posted tasks (and nested task posted by those tasks)
  // of all the associated task runners.
  // Once all the tasks are executed the method blocks until the threads are
  // terminated.
  virtual void Shutdown() = 0;

  TaskGraphRunner* GetTaskGraphRunner() { return this; }

  // Create a new sequenced task graph runner.
  scoped_refptr<base::SequencedTaskRunner> CreateSequencedTaskRunner();

 protected:
  class CategorizedWorkerPoolSequencedTaskRunner;
  friend class CategorizedWorkerPoolSequencedTaskRunner;

  ~CategorizedWorkerPool() override;

  // Simple Task for the TaskGraphRunner that wraps a closure.
  // This class is used to schedule TaskRunner tasks on the
  // |task_graph_runner_|.
  class ClosureTask : public Task {
   public:
    explicit ClosureTask(base::OnceClosure closure);

    ClosureTask(const ClosureTask&) = delete;
    ClosureTask& operator=(const ClosureTask&) = delete;

    // Overridden from Task:
    void RunOnWorkerThread() override;

   protected:
    ~ClosureTask() override;

   private:
    base::OnceClosure closure_;
  };

  void CollectCompletedTasksWithLockAcquired(NamespaceToken token,
                                             Task::Vector* completed_tasks)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Determines if we should run a new task for the given category. This factors
  // in whether a task is available and whether the count of running tasks is
  // low enough to start a new one.
  bool ShouldRunTaskForCategoryWithLockAcquired(TaskCategory category)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Lock to exclusively access all the following members that are used to
  // implement the TaskRunner and TaskGraphRunner interfaces.
  mutable base::Lock lock_;
  // Stores the tasks to be run, sorted by priority.
  TaskGraphWorkQueue work_queue_ GUARDED_BY(lock_);
  // Namespace used to schedule tasks in the task graph runner.
  const NamespaceToken namespace_token_;
  // List of tasks currently queued up for execution.
  Task::Vector tasks_ GUARDED_BY(lock_);
  // Graph object used for scheduling tasks.
  TaskGraph graph_ GUARDED_BY(lock_);
  // Cached vector to avoid allocation when getting the list of complete
  // tasks.
  Task::Vector completed_tasks_ GUARDED_BY(lock_);
  // Condition variable that is waited on by origin threads until a namespace
  // has finished running all associated tasks.
  base::ConditionVariable has_namespaces_with_finished_running_tasks_cv_;
};

class CC_EXPORT CategorizedWorkerPoolImpl : public CategorizedWorkerPool {
 public:
  explicit CategorizedWorkerPoolImpl(Delegate* delegate = nullptr);

  void ThreadWillRun(base::PlatformThreadId tid);

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  // Overridden from TaskGraphRunner:
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph) override;

  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority.
  void Run(const std::vector<TaskCategory>& categories,
           base::ConditionVariable* has_ready_to_run_tasks_cv);

  // Overridden from CategorizedWorkerPool:
  void FlushForTesting() override;
  void Start(int max_concurrency_foreground) override;
  void Shutdown() override;

 private:
  ~CategorizedWorkerPoolImpl() override;

  void ScheduleTasksWithLockAcquired(NamespaceToken token, TaskGraph* graph)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority. Returns false if there were no tasks to run.
  bool RunTaskWithLockAcquired(const std::vector<TaskCategory>& categories)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Run next task for the given category. Caller must acquire |lock_| prior to
  // calling this function and make sure at least one task is ready to run.
  void RunTaskInCategoryWithLockAcquired(TaskCategory category)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper function which signals worker threads if tasks are ready to run.
  void SignalHasReadyToRunTasksWithLockAcquired()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  const raw_ptr<Delegate> delegate_;

  // The actual threads where work is done.
  std::vector<std::unique_ptr<base::SimpleThread>> threads_;

  // Condition variables for foreground and background threads.
  base::ConditionVariable has_task_for_normal_priority_thread_cv_;
  base::ConditionVariable has_task_for_background_priority_thread_cv_;

  // Set during shutdown. Tells Run() to return when no more tasks are pending.
  bool shutdown_ GUARDED_BY(lock_);
};

class CC_EXPORT CategorizedWorkerPoolJob : public CategorizedWorkerPool {
 public:
  CategorizedWorkerPoolJob();

  // Overridden from base::TaskRunner:
  bool PostDelayedTask(const base::Location& from_here,
                       base::OnceClosure task,
                       base::TimeDelta delay) override;

  // Overridden from TaskGraphRunner:
  void ScheduleTasks(NamespaceToken token, TaskGraph* graph) override;

  // Runs a task from one of the provided categories. Categories listed first
  // have higher priority.
  void Run(base::span<const TaskCategory> categories,
           base::JobDelegate* job_delegate);

  // Overridden from CategorizedWorkerPool:
  void FlushForTesting() override;
  void Start(int max_concurrency_foreground) override;
  void Shutdown() override;

 private:
  ~CategorizedWorkerPoolJob() override;

  std::optional<TaskGraphWorkQueue::PrioritizedTask>
  GetNextTaskToRunWithLockAcquired(base::span<const TaskCategory> categories);

  base::JobHandle* ScheduleTasksWithLockAcquired(NamespaceToken token,
                                                 TaskGraph* graph)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Helper function which signals worker threads if tasks are ready to run.
  base::JobHandle* GetJobHandleToNotifyWithLockAcquired()
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  size_t GetMaxJobConcurrency(base::span<const TaskCategory> categories) const;

  size_t max_concurrency_foreground_ = 0;

  base::JobHandle background_job_handle_;
  base::JobHandle foreground_job_handle_;
};

}  // namespace cc

#endif  // CC_RASTER_CATEGORIZED_WORKER_POOL_H_
