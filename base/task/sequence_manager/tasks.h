// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TASKS_H_
#define BASE_TASK_SEQUENCE_MANAGER_TASKS_H_

#include "base/pending_task.h"
#include "base/sequenced_task_runner.h"
#include "base/task/sequence_manager/enqueue_order.h"

namespace base {
namespace sequence_manager {

using TaskType = uint8_t;
constexpr TaskType kTaskTypeNone = 0;

namespace internal {

// Wrapper around PostTask method arguments and the assigned task type.
// Eventually it becomes a PendingTask once accepted by a TaskQueueImpl.
struct BASE_EXPORT PostedTask {
  explicit PostedTask(scoped_refptr<SequencedTaskRunner> task_runner,
                      OnceClosure callback = OnceClosure(),
                      Location location = Location(),
                      TimeDelta delay = TimeDelta(),
                      Nestable nestable = Nestable::kNestable,
                      TaskType task_type = kTaskTypeNone);
  PostedTask(PostedTask&& move_from) noexcept;
  PostedTask(const PostedTask&) = delete;
  PostedTask& operator=(const PostedTask&) = delete;
  ~PostedTask();

  OnceClosure callback;
  Location location;
  TimeDelta delay;
  Nestable nestable;
  TaskType task_type;
  // The task runner this task is running on. Can be used by task runners that
  // support posting back to the "current sequence".
  scoped_refptr<SequencedTaskRunner> task_runner;
  // The time at which the task was queued.
  TimeTicks queue_time;
};

}  // namespace internal

enum class WakeUpResolution { kLow, kHigh };

// Represents a time at which a task wants to run.
struct DelayedWakeUp {
  TimeTicks time;
  WakeUpResolution resolution;

  bool operator!=(const DelayedWakeUp& other) const {
    return time != other.time || resolution != other.resolution;
  }

  bool operator==(const DelayedWakeUp& other) const {
    return !(*this != other);
  }

  bool operator<=(const DelayedWakeUp& other) const {
    if (time == other.time) {
      if (resolution == other.resolution)
        return true;

      return resolution < other.resolution;
    }
    return time < other.time;
  }
};

// PendingTask with extra metadata for SequenceManager.
struct BASE_EXPORT Task : public PendingTask {
  Task(internal::PostedTask posted_task,
       TimeTicks delayed_run_time,
       EnqueueOrder sequence_order,
       EnqueueOrder enqueue_order = EnqueueOrder(),
       WakeUpResolution wake_up_resolution = WakeUpResolution::kLow);
  Task(Task&& move_from);
  ~Task();
  Task& operator=(Task&& other);

  // SequenceManager is particularly sensitive to enqueue order,
  // so we have accessors for safety.
  EnqueueOrder enqueue_order() const {
    DCHECK(enqueue_order_);
    return enqueue_order_;
  }

  void set_enqueue_order(EnqueueOrder enqueue_order) {
    DCHECK(!enqueue_order_);
    enqueue_order_ = enqueue_order;
  }

  bool enqueue_order_set() const { return enqueue_order_; }

  // OK to dispatch from a nested loop.
  Nestable nestable = Nestable::kNonNestable;

  // Needs high resolution timers.
  bool is_high_res = false;

  TaskType task_type;

  // The task runner this task is running on. Can be used by task runners that
  // support posting back to the "current sequence".
  scoped_refptr<SequencedTaskRunner> task_runner;

#if DCHECK_IS_ON()
  bool cross_thread_;
#endif

 private:
  // Similar to |sequence_num|, but ultimately the |enqueue_order| is what
  // the scheduler uses for task ordering. For immediate tasks |enqueue_order|
  // is set when posted, but for delayed tasks it's not defined until they are
  // enqueued. This is because otherwise delayed tasks could run before
  // an immediate task posted after the delayed task.
  EnqueueOrder enqueue_order_;
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TASKS_H_
