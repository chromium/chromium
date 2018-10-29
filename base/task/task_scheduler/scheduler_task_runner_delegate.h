// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_SCHEDULER_TASK_RUNNER_DELEGATE_H_
#define BASE_TASK_TASK_SCHEDULER_SCHEDULER_TASK_RUNNER_DELEGATE_H_

#include "base/base_export.h"
#include "base/task/task_scheduler/sequence.h"
#include "base/task/task_scheduler/task.h"
#include "base/task/task_traits.h"

namespace base {
namespace internal {

// Delegate interface for SchedulerParallelTaskRunner and
// SchedulerSequencedTaskRunner.
class BASE_EXPORT SchedulerTaskRunnerDelegate {
 public:
  SchedulerTaskRunnerDelegate();
  virtual ~SchedulerTaskRunnerDelegate();

  // Returns true if a SchedulerTaskRunnerDelegate instance exists in the
  // process. This is needed in case of unit tests wherein a TaskRunner
  // outlives the TaskScheduler that created it.
  static bool Exists();

  // Invoked when a |task| is posted to the SchedulerParallelTaskRunner or
  // SchedulerSequencedTaskRunner. The implementation must post |task| to
  // |sequence| within the appropriate priority queue, depending on |sequence|
  // traits. Returns true if task was successfully posted.
  virtual bool PostTaskWithSequence(Task task,
                                    scoped_refptr<Sequence> sequence) = 0;

  // Invoked when RunsTasksInCurrentSequence() is called on a
  // SchedulerParallelTaskRunner. Returns true if the worker pool used by the
  // SchedulerParallelTaskRunner (as determined by |traits|) is running on
  // this thread.
  virtual bool IsRunningPoolWithTraits(const TaskTraits& traits) const = 0;
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_SCHEDULER_TASK_RUNNER_DELEGATE_H_
