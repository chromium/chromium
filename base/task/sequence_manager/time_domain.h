// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
#define BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_

#include <map>

#include "base/callback.h"
#include "base/logging.h"
#include "base/macros.h"
#include "base/task/common/intrusive_heap.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/time/time.h"

namespace base {
namespace sequence_manager {

class SequenceManager;

namespace internal {
struct AssociatedThreadId;
class SequenceManagerImpl;
class TaskQueueImpl;
}  // namespace internal

// TimeDomain wakes up TaskQueues when their delayed tasks are due to run.
// This class allows overrides to enable clock overriding on some TaskQueues
// (e.g. auto-advancing virtual time, throttled clock, etc).
//
// TaskQueue maintains its own next wake-up time and communicates it
// to the TimeDomain, which aggregates wake-ups across registered TaskQueues
// into a global wake-up, which ultimately gets passed to the ThreadController.
class BASE_EXPORT TimeDomain {
 public:
  virtual ~TimeDomain();

  // Returns LazyNow in TimeDomain's time.
  // Can be called from any thread.
  // TODO(alexclarke): Make this main thread only.
  virtual LazyNow CreateLazyNow() const = 0;

  // Evaluates TimeDomain's time.
  // Can be called from any thread.
  // TODO(alexclarke): Make this main thread only.
  virtual TimeTicks Now() const = 0;

  // Computes the delay until the time when TimeDomain needs to wake up
  // some TaskQueue. Specific time domains (e.g. virtual or throttled) may
  // return TimeDelata() if TaskQueues have any delayed tasks they deem
  // eligible to run. It's also allowed to advance time domains's internal
  // clock when this method is called.
  // Can be called from main thread only.
  // NOTE: |lazy_now| and the return value are in the SequenceManager's time.
  virtual Optional<TimeDelta> DelayTillNextTask(LazyNow* lazy_now) = 0;

  void AsValueInto(trace_event::TracedValue* state) const;
  bool HasPendingHighResolutionTasks() const;

 protected:
  TimeDomain();

  SequenceManager* sequence_manager() const;

  // Returns the earliest scheduled wake up in the TimeDomain's time.
  Optional<TimeTicks> NextScheduledRunTime() const;

  size_t NumberOfScheduledWakeUps() const {
    return delayed_wake_up_queue_.size();
  }

  // Tells SequenceManager to schedule delayed work, use TimeTicks::Max()
  // to unschedule. Also cancels any previous requests.
  // May be overriden to control wake ups manually.
  virtual void SetNextDelayedDoWork(LazyNow* lazy_now, TimeTicks run_time);

  // Tells SequenceManager to schedule immediate work.
  // May be overriden to control wake ups manually.
  virtual void RequestDoWork();

  // For implementation-specific tracing.
  virtual void AsValueIntoInternal(trace_event::TracedValue* state) const;
  virtual const char* GetName() const = 0;

 private:
  friend class internal::TaskQueueImpl;
  friend class internal::SequenceManagerImpl;
  friend class TestTimeDomain;

  // Called when the TimeDomain is registered.
  // TODO(kraynov): Pass SequenceManager in the constructor.
  void OnRegisterWithSequenceManager(
      internal::SequenceManagerImpl* sequence_manager);

  // Schedule TaskQueue to wake up at certain time, repeating calls with
  // the same |queue| invalidate previous requests.
  // Nullopt |wake_up| cancels a previously set wake up for |queue|.
  // NOTE: |lazy_now| is provided in TimeDomain's time.
  void SetNextWakeUpForQueue(internal::TaskQueueImpl* queue,
                             Optional<internal::DelayedWakeUp> wake_up,
                             internal::WakeUpResolution resolution,
                             LazyNow* lazy_now);

  // Remove the TaskQueue from any internal data sctructures.
  void UnregisterQueue(internal::TaskQueueImpl* queue);

  // Wake up each TaskQueue where the delay has elapsed.
  void WakeUpReadyDelayedQueues(LazyNow* lazy_now);

  struct ScheduledDelayedWakeUp {
    internal::DelayedWakeUp wake_up;
    internal::WakeUpResolution resolution;
    internal::TaskQueueImpl* queue;

    bool operator<=(const ScheduledDelayedWakeUp& other) const {
      if (wake_up == other.wake_up) {
        return static_cast<int>(resolution) <=
               static_cast<int>(other.resolution);
      }
      return wake_up <= other.wake_up;
    }

    void SetHeapHandle(base::internal::HeapHandle handle) {
      DCHECK(handle.IsValid());
      queue->set_heap_handle(handle);
    }

    void ClearHeapHandle() {
      DCHECK(queue->heap_handle().IsValid());
      queue->set_heap_handle(base::internal::HeapHandle());
    }
  };

  internal::SequenceManagerImpl* sequence_manager_;  // Not owned.
  base::internal::IntrusiveHeap<ScheduledDelayedWakeUp> delayed_wake_up_queue_;
  int pending_high_res_wake_up_count_ = 0;

  scoped_refptr<internal::AssociatedThreadId> associated_thread_;
  DISALLOW_COPY_AND_ASSIGN(TimeDomain);
};

}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
