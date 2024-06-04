// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_

#include "base/base_export.h"
#include "base/debug/stack_trace.h"
#include "base/observer_list_types.h"
#include "base/time/time.h"

namespace base {
namespace sequence_manager {

// TaskTimeObserver provides an API for observing completion of tasks.
class BASE_EXPORT TaskTimeObserver : public CheckedObserver {
 public:
  TaskTimeObserver();
  TaskTimeObserver(const TaskTimeObserver&) = delete;
  TaskTimeObserver& operator=(const TaskTimeObserver&) = delete;
  ~TaskTimeObserver() override;

  // To be called when task is about to start.
  virtual void WillProcessTask(TimeTicks start_time) = 0;

  // To be called when task is completed.
  virtual void DidProcessTask(TimeTicks start_time, TimeTicks end_time) = 0;

 private:
  // TODO(crbug.com/337200890): Remove this before shipping to beta; it exists
  // only for gathering data for the ongoing investigation, we should not be
  // unwinding the stack on something so contentious.
  const base::debug::StackTrace alloc_stack_;
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASK_TIME_OBSERVER_H_
