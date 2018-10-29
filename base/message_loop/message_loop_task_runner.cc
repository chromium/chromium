// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_loop_task_runner.h"

#include <utility>

#include "base/location.h"
#include "base/logging.h"
#include "base/metrics/histogram_macros.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

#if DCHECK_IS_ON()
// Delays larger than this are often bogus, and a warning should be emitted in
// debug builds to warn developers.  http://crbug.com/450045
constexpr TimeDelta kTaskDelayWarningThreshold = TimeDelta::FromDays(14);
#endif

TimeTicks CalculateDelayedRuntime(const Location& from_here, TimeDelta delay) {
  DLOG_IF(WARNING, delay > kTaskDelayWarningThreshold)
      << "Requesting super-long task delay period of " << delay.InSeconds()
      << " seconds from here: " << from_here.ToString();

  DCHECK_GE(delay, TimeDelta()) << "delay should not be negative";

  return delay > TimeDelta() ? TimeTicks::Now() + delay : TimeTicks();
}

}  // namespace

MessageLoopTaskRunner::MessageLoopTaskRunner(
    std::unique_ptr<SequencedTaskSource::Observer> task_source_observer)
    : task_source_observer_(std::move(task_source_observer)) {}

void MessageLoopTaskRunner::BindToCurrentThread() {
  AutoLock lock(valid_thread_id_lock_);
  DCHECK_EQ(kInvalidThreadId, valid_thread_id_);
  valid_thread_id_ = PlatformThread::CurrentId();
}

void MessageLoopTaskRunner::Shutdown() {
  AutoLock lock(incoming_queue_lock_);
  accept_new_tasks_ = false;
}

bool MessageLoopTaskRunner::PostDelayedTask(const Location& from_here,
                                            OnceClosure task,
                                            base::TimeDelta delay) {
  return AddToIncomingQueue(from_here, std::move(task), delay,
                            Nestable::kNestable);
}

bool MessageLoopTaskRunner::PostNonNestableDelayedTask(
    const Location& from_here,
    OnceClosure task,
    base::TimeDelta delay) {
  return AddToIncomingQueue(from_here, std::move(task), delay,
                            Nestable::kNonNestable);
}

bool MessageLoopTaskRunner::RunsTasksInCurrentSequence() const {
  AutoLock lock(valid_thread_id_lock_);
  return valid_thread_id_ == PlatformThread::CurrentId();
}

PendingTask MessageLoopTaskRunner::TakeTask() {
  // Must be called on execution sequence, unless clearing tasks from an unbound
  // MessageLoop.
  DCHECK(RunsTasksInCurrentSequence() || valid_thread_id_ == kInvalidThreadId);

  // HasTasks() will reload the queue if necessary (there should always be
  // pending tasks by contract).
  const bool has_tasks = HasTasks();
  DCHECK(has_tasks);

  PendingTask pending_task = std::move(outgoing_queue_.front());
  outgoing_queue_.pop();
  return pending_task;
}

bool MessageLoopTaskRunner::HasTasks() {
  // Must be called on execution sequence, unless clearing tasks from an unbound
  // MessageLoop.
  DCHECK(RunsTasksInCurrentSequence() || valid_thread_id_ == kInvalidThreadId);

  if (outgoing_queue_.empty()) {
    AutoLock lock(incoming_queue_lock_);
    incoming_queue_.swap(outgoing_queue_);
    outgoing_queue_empty_ = outgoing_queue_.empty();
  }

  return !outgoing_queue_.empty();
}

void MessageLoopTaskRunner::InjectTask(OnceClosure task) {
  // Must be called on execution sequence, unless clearing tasks from an unbound
  // MessageLoop.
  DCHECK(RunsTasksInCurrentSequence() || valid_thread_id_ == kInvalidThreadId);

  bool success = this->PostTask(FROM_HERE, std::move(task));
  DCHECK(success) << "Injected a task in a dead task runner.";
}

void MessageLoopTaskRunner::SetAddQueueTimeToTasks(bool enable) {
  base::subtle::NoBarrier_Store(&add_queue_time_to_tasks_, enable ? 1 : 0);
}

MessageLoopTaskRunner::~MessageLoopTaskRunner() = default;

bool MessageLoopTaskRunner::AddToIncomingQueue(const Location& from_here,
                                               OnceClosure task,
                                               TimeDelta delay,
                                               Nestable nestable) {
  DCHECK(task_source_observer_)
      << "SetObserver() must be called before posting tasks";

  // Use CHECK instead of DCHECK to crash earlier. See http://crbug.com/711167
  // for details.
  CHECK(task);

  PendingTask pending_task(from_here, std::move(task),
                           CalculateDelayedRuntime(from_here, delay), nestable);

  if (base::subtle::NoBarrier_Load(&add_queue_time_to_tasks_)) {
    if (pending_task.delayed_run_time.is_null()) {
      pending_task.queue_time = base::TimeTicks::Now();
    } else {
      pending_task.queue_time = pending_task.delayed_run_time - delay;
    }
  }

#if defined(OS_WIN)
  // We consider the task needs a high resolution timer if the delay is
  // more than 0 and less than 32ms. This caps the relative error to
  // less than 50% : a 33ms wait can wake at 48ms since the default
  // resolution on Windows is between 10 and 15ms.
  if (delay > TimeDelta() &&
      delay.InMilliseconds() < (2 * Time::kMinLowResolutionThresholdMs)) {
    pending_task.is_high_res = true;
  }
#endif

  bool did_queue_task = false;
  bool was_empty;
  {
    AutoLock auto_lock(incoming_queue_lock_);
    if (accept_new_tasks_) {
      // Initialize the sequence number. The sequence number is used for delayed
      // tasks (to facilitate FIFO sorting when two tasks have the same
      // delayed_run_time value) and for identifying the task in about:tracing.
      pending_task.sequence_num = next_sequence_num_++;

      task_source_observer_->WillQueueTask(&pending_task);

      was_empty = outgoing_queue_empty_ && incoming_queue_.empty();
      incoming_queue_.push(std::move(pending_task));

      did_queue_task = true;
    }
  }

  if (!did_queue_task) {
    // Clear the pending task outside of |incoming_queue_lock_| to prevent any
    // chance of self-deadlock if destroying a task also posts a task to this
    // queue.
    pending_task.task.Reset();
    return false;
  }

  // Let |task_source_observer_| know about the task just queued. It's important
  // to do this outside of |incoming_queue_lock_| to avoid blocking tasks
  // incoming from other threads on the resolution of DidQueueTask().
  task_source_observer_->DidQueueTask(was_empty);
  return true;
}

}  // namespace internal

}  // namespace base
