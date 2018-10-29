// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_TASK_RUNNER_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_TASK_RUNNER_H_

#include "base/single_thread_task_runner.h"

namespace base {
namespace sequence_manager {
namespace internal {

class TaskQueueProxy;

// TODO(kraynov): Post tasks to a TaskQueue solely using this task runner and
// drop SingleThreadTaskRunner implementation in the TaskQueue class.
// See https://crbug.com/865411.
class BASE_EXPORT TaskQueueTaskRunner : public SingleThreadTaskRunner {
 public:
  TaskQueueTaskRunner(scoped_refptr<TaskQueueProxy> task_queue_proxy,
                      int task_type);

  bool PostDelayedTask(const Location& location,
                       OnceClosure callback,
                       TimeDelta delay) override;
  bool PostNonNestableDelayedTask(const Location& location,
                                  OnceClosure callback,
                                  TimeDelta delay) override;
  bool RunsTasksInCurrentSequence() const override;

 private:
  ~TaskQueueTaskRunner() override;  // Ref-counted.

  const scoped_refptr<TaskQueueProxy> task_queue_proxy_;
  const int task_type_;

  DISALLOW_COPY_AND_ASSIGN(TaskQueueTaskRunner);
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_QUEUE_TASK_RUNNER_H_
