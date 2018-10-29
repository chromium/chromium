// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_SEQUENCED_TASK_SOURCE_H_
#define BASE_TASK_SEQUENCE_MANAGER_SEQUENCED_TASK_SOURCE_H_

#include "base/optional.h"
#include "base/pending_task.h"
#include "base/task/sequence_manager/lazy_now.h"

namespace base {
namespace sequence_manager {
namespace internal {

// Interface to pass tasks to ThreadController.
class SequencedTaskSource {
 public:
  virtual ~SequencedTaskSource() = default;

  // Returns the next task to run from this source or nullopt if
  // there're no more tasks ready to run. If a task is returned,
  // DidRunTask() must be invoked before the next call to TakeTask().
  virtual Optional<PendingTask> TakeTask() = 0;

  // Notifies this source that the task previously obtained
  // from TakeTask() has been completed.
  virtual void DidRunTask() = 0;

  // Returns the delay till the next task or TimeDelta::Max()
  // if there are no tasks left.
  virtual TimeDelta DelayTillNextTask(LazyNow* lazy_now) = 0;

  // Return true if there are any pending tasks in the task source which require
  // high resolution timing.
  virtual bool HasPendingHighResolutionTasks() = 0;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_SEQUENCED_TASK_SOURCE_H_
