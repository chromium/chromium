// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SIMPLE_TASK_EXECUTOR_H_
#define BASE_TASK_SIMPLE_TASK_EXECUTOR_H_

#include "base/task/task_executor.h"
#include "build/build_config.h"

namespace base {
namespace sequence_manager {
class SequenceManager;
}  // namespace sequence_manager

// A simple TaskExecutor with exactly one SingleThreadTaskRunner.
// Must be instantiated and destroyed on the thread that runs tasks for the
// SingleThreadTaskRunner.
class BASE_EXPORT SimpleTaskExecutor : public TaskExecutor {
 public:
  // If |sequence_manager| is null, GetContinuationTaskRunner will always return
  // |task_queue| even if no task is running.
  SimpleTaskExecutor(sequence_manager::SequenceManager* sequence_manager,
                     scoped_refptr<SingleThreadTaskRunner> task_queue);

  ~SimpleTaskExecutor() override;

  bool PostDelayedTask(const Location& from_here,
                       const TaskTraits& traits,
                       OnceClosure task,
                       TimeDelta delay) override;

  scoped_refptr<TaskRunner> CreateTaskRunner(const TaskTraits& traits) override;

  scoped_refptr<SequencedTaskRunner> CreateSequencedTaskRunner(
      const TaskTraits& traits) override;

  scoped_refptr<SingleThreadTaskRunner> CreateSingleThreadTaskRunner(
      const TaskTraits& traits,
      SingleThreadTaskRunnerThreadMode thread_mode) override;

#if defined(OS_WIN)
  scoped_refptr<SingleThreadTaskRunner> CreateCOMSTATaskRunner(
      const TaskTraits& traits,
      SingleThreadTaskRunnerThreadMode thread_mode) override;
#endif  // defined(OS_WIN)

  const scoped_refptr<SequencedTaskRunner>& GetContinuationTaskRunner()
      override;

 protected:
  sequence_manager::SequenceManager* const sequence_manager_;
  const scoped_refptr<SequencedTaskRunner> sequenced_task_queue_;
  const scoped_refptr<SingleThreadTaskRunner> task_queue_;

  // In tests there may already be a TaskExecutor registered for the thread, we
  // keep tack of the previous TaskExecutor and restored it upon destruction.
  TaskExecutor* const previous_task_executor_;
};

}  // namespace base

#endif  // BASE_TASK_SIMPLE_TASK_EXECUTOR_H_
