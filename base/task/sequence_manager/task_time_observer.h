// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_

#include "base/base_export.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

namespace base {
namespace sequence_manager {

// TaskTimeObserver provides an API for observing completion of tasks.
class BASE_EXPORT TaskTimeObserver : public CheckedObserver {
 public:
  TaskTimeObserver() = default;
  TaskTimeObserver(const TaskTimeObserver&) = delete;
  TaskTimeObserver& operator=(const TaskTimeObserver&) = delete;
  ~TaskTimeObserver() override;

  // To be called when task is about to start.
  virtual void WillProcessTask(TimeTicks start_time) = 0;

  // To be called when task is completed. `desired_execution_time` is the time
  // the task was scheduled to run: its enqueue time for an immediate task, or
  // its run time (enqueue time + delay) for a delayed task (see
  // TaskMetadata::GetDesiredExecutionTime()). It is null when the sequence
  // manager does not stamp these times.
  virtual void DidProcessTask(TimeTicks start_time,
                              TimeTicks end_time,
                              TimeTicks desired_execution_time) = 0;
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_
