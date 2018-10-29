// Copyright 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_TASK_SCHEDULER_TASK_H_
#define BASE_TASK_TASK_SCHEDULER_TASK_H_

#include "base/base_export.h"
#include "base/callback.h"
#include "base/location.h"
#include "base/macros.h"
#include "base/memory/ref_counted.h"
#include "base/pending_task.h"
#include "base/sequenced_task_runner.h"
#include "base/single_thread_task_runner.h"
#include "base/time/time.h"

namespace base {
namespace internal {

// A task is a unit of work inside the task scheduler. Support for tracing and
// profiling inherited from PendingTask.
struct BASE_EXPORT Task : public PendingTask {
  // |posted_from| is the site the task was posted from. |task| is the closure
  // to run. |delay| is a delay that must expire before the Task runs.
  Task(const Location& posted_from,
       OnceClosure task,
       TimeDelta delay);

  // Task is move-only to avoid mistakes that cause reference counts to be
  // accidentally bumped.
  Task(Task&& other) noexcept;

  ~Task();

  Task& operator=(Task&& other);

  // The delay that must expire before the task runs.
  TimeDelta delay;

  // The time at which the task was inserted in its sequence. For an undelayed
  // task, this happens at post time. For a delayed task, this happens some
  // time after the task's delay has expired. If the task hasn't been inserted
  // in a sequence yet, this defaults to a null TimeTicks.
  TimeTicks sequenced_time;

  // A reference to the SequencedTaskRunner or SingleThreadTaskRunner that
  // posted this task, if any. Used to set ThreadTaskRunnerHandle and/or
  // SequencedTaskRunnerHandle while the task is running.
  // Note: this creates an ownership cycle
  //   Sequence -> Task -> TaskRunner -> Sequence -> ...
  // but that's okay as it's broken when the Task is popped from its Sequence
  // after being executed which means this cycle forces the TaskRunner to stick
  // around until all its tasks have been executed which is a requirement to
  // support TaskRunnerHandles.
  scoped_refptr<SequencedTaskRunner> sequenced_task_runner_ref;
  scoped_refptr<SingleThreadTaskRunner> single_thread_task_runner_ref;

 private:
  DISALLOW_COPY_AND_ASSIGN(Task);
};

}  // namespace internal
}  // namespace base

#endif  // BASE_TASK_TASK_SCHEDULER_TASK_H_
