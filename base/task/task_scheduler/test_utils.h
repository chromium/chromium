// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_
#define BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_

#include "base/task/task_scheduler/delayed_task_manager.h"
#include "base/task/task_scheduler/scheduler_task_runner_delegate.h"
#include "base/task/task_scheduler/scheduler_worker_observer.h"
#include "base/task/task_scheduler/scheduler_worker_pool.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task_tracker.h"
#include "base/task/task_traits.h"
#include "base/task_runner.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace base {
namespace internal {

struct Task;

namespace test {

class MockSchedulerWorkerObserver : public SchedulerWorkerObserver {
 public:
  MockSchedulerWorkerObserver();
  ~MockSchedulerWorkerObserver();

  MOCK_METHOD0(OnSchedulerWorkerMainEntry, void());
  MOCK_METHOD0(OnSchedulerWorkerMainExit, void());

 private:
  DISALLOW_COPY_AND_ASSIGN(MockSchedulerWorkerObserver);
};

class MockSchedulerTaskRunnerDelegate : public SchedulerTaskRunnerDelegate {
 public:
  MockSchedulerTaskRunnerDelegate(TrackedRef<TaskTracker> task_tracker,
                                  DelayedTaskManager* delayed_task_manager);
  ~MockSchedulerTaskRunnerDelegate() override;

  // SchedulerTaskRunnerDelegate:
  bool PostTaskWithSequence(Task task,
                            scoped_refptr<Sequence> sequence) override;
  bool IsRunningPoolWithTraits(const TaskTraits& traits) const override;

  void SetWorkerPool(SchedulerWorkerPool* worker_pool);

 private:
  const TrackedRef<TaskTracker> task_tracker_;
  DelayedTaskManager* const delayed_task_manager_;
  SchedulerWorkerPool* worker_pool_ = nullptr;
};

// An enumeration of possible task scheduler TaskRunner types. Used to
// parametrize relevant task_scheduler tests.
enum class ExecutionMode { PARALLEL, SEQUENCED, SINGLE_THREADED };

// Creates a Sequence with given |traits| and pushes |task| to it. Returns that
// Sequence.
scoped_refptr<Sequence> CreateSequenceWithTask(Task task,
                                               const TaskTraits& traits);

// Creates a TaskRunner that posts tasks to the worker pool owned by
// |scheduler_task_runner_delegate| with the |execution_mode| execution mode
// and the WithBaseSyncPrimitives() trait.
// Caveat: this does not support ExecutionMode::SINGLE_THREADED.
scoped_refptr<TaskRunner> CreateTaskRunnerWithExecutionMode(
    test::ExecutionMode execution_mode,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate);

scoped_refptr<TaskRunner> CreateTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate);

scoped_refptr<SequencedTaskRunner> CreateSequencedTaskRunnerWithTraits(
    const TaskTraits& traits,
    MockSchedulerTaskRunnerDelegate* mock_scheduler_task_runner_delegate);

}  // namespace test
}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_TEST_UTILS_H_
