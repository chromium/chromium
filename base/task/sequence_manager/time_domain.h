// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
#define BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_

#include "base/check.h"
#include "base/containers/intrusive_heap.h"
#include "base/task/sequence_manager/lazy_now.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/task/sequence_manager/tasks.h"
#include "base/time/tick_clock.h"
#include "base/values.h"
#include "third_party/abseil-cpp/absl/types/optional.h"

namespace base {
namespace sequence_manager {

class SequenceManager;

namespace internal {
class AssociatedThreadId;
class SequenceManagerImpl;
}  // namespace internal

// TimeDomain allows subclasses to enable clock overriding
// (e.g. auto-advancing virtual time, throttled clock, etc).
class BASE_EXPORT TimeDomain : public TickClock {
 public:
  TimeDomain(const TimeDomain&) = delete;
  TimeDomain& operator=(const TimeDomain&) = delete;
  ~TimeDomain() override = default;

  // Returns the desired ready time based on the predetermined `next_wake_up`,
  // is_null() if ready immediately, or is_max() to ignore the wake-up. This is
  // typically aligned with `next_wake_up.time` but virtual time domains may
  // elect otherwise. Can be called from main thread only.
  // TODO(857101): Pass `lazy_now` by reference.
  virtual TimeTicks GetNextDelayedTaskTime(DelayedWakeUp next_wake_up,
                                           LazyNow* lazy_now) const = 0;

  // Invoked when the thread reaches idle. Gives an opportunity to a virtual
  // time domain impl to fast-forward time and return true to indicate that
  // there's more work to run. If RunLoop::QuitWhenIdle has been called then
  // `quit_when_idle_requested` will be true.
  virtual bool MaybeFastForwardToWakeUp(
      absl::optional<DelayedWakeUp> next_wake_up,
      bool quit_when_idle_requested) = 0;

  // Debug info.
  Value AsValue() const;

 protected:
  TimeDomain() = default;

  virtual const char* GetName() const = 0;

  // Tells SequenceManager that internal policy might have changed to
  // re-evaluate GetNextDelayedTaskTime()/MaybeFastForwardToWakeUp().
  void NotifyPolicyChanged();

  // Called when the TimeDomain is assigned to a SequenceManagerImpl.
  // `sequence_manager` is expected to be valid for the duration of TimeDomain's
  // existence. TODO(scheduler-dev): Pass SequenceManager in the constructor.
  void OnAssignedToSequenceManager(
      internal::SequenceManagerImpl* sequence_manager);

 private:
  friend class internal::SequenceManagerImpl;

  internal::SequenceManagerImpl* sequence_manager_ = nullptr;  // Not owned.
};

namespace internal {

// WakeUpQueue is a queue of (wake_up, TaskQueueImpl*) pairs which
// aggregates wake-ups from multiple TaskQueueImpl into a single wake-up, and
// notifies TaskQueueImpls when wake-up times are reached.
class BASE_EXPORT WakeUpQueue {
 public:
  WakeUpQueue(const WakeUpQueue&) = delete;
  WakeUpQueue& operator=(const WakeUpQueue&) = delete;
  virtual ~WakeUpQueue();

  // Returns a wake-up for the next pending delayed task (pending delayed tasks
  // that are ripe may be ignored). If there are no such tasks (immediate tasks
  // don't count) or queues are disabled it returns nullopt.
  absl::optional<DelayedWakeUp> GetNextDelayedWakeUp() const;

  // Debug info.
  Value AsValue(TimeTicks now) const;

  bool has_pending_high_resolution_tasks() const {
    return pending_high_res_wake_up_count_;
  }

  // Returns true if there are no pending delayed tasks.
  bool empty() const { return wake_up_queue_.empty(); }

  // Moves ready delayed tasks in TaskQueues to delayed WorkQueues, consuming
  // expired wake-ups in the process.
  void MoveReadyDelayedTasksToWorkQueues(LazyNow* lazy_now);

  // Schedule `queue` to wake up at certain time. Repeating calls with the same
  // `queue` invalidate previous requests. Nullopt `wake_up` cancels a
  // previously set wake up for `queue`.
  void SetNextWakeUpForQueue(internal::TaskQueueImpl* queue,
                             LazyNow* lazy_now,
                             absl::optional<DelayedWakeUp> wake_up);

  // Remove the TaskQueue from any internal data structures.
  virtual void UnregisterQueue(internal::TaskQueueImpl* queue) = 0;

  // Removes all canceled delayed tasks from the front of the queue. After
  // calling this, GetNextDelayedWakeUp() is guaranteed to return a wake up time
  // for a non-canceled task.
  void RemoveAllCanceledDelayedTasksFromFront(LazyNow* lazy_now);

 protected:
  explicit WakeUpQueue(
      scoped_refptr<internal::AssociatedThreadId> associated_thread);

  // Called every time the next `next_wake_up` changes. absl::nullopt is used to
  // cancel the next wake-up. Subclasses may use this to tell SequenceManager to
  // schedule the next wake-up at the given time.
  virtual void OnNextDelayedWakeUpChanged(
      LazyNow* lazy_now,
      absl::optional<DelayedWakeUp> next_wake_up) = 0;

  virtual const char* GetName() const = 0;

 private:
  friend class MockWakeUpQueue;

  struct ScheduledDelayedWakeUp {
    DelayedWakeUp wake_up;
    internal::TaskQueueImpl* queue;

    bool operator>(const ScheduledDelayedWakeUp& other) const {
      return wake_up > other.wake_up;
    }

    void SetHeapHandle(HeapHandle handle) {
      DCHECK(handle.IsValid());
      queue->set_heap_handle(handle);
    }

    void ClearHeapHandle() {
      DCHECK(queue->heap_handle().IsValid());
      queue->set_heap_handle(HeapHandle());
    }

    HeapHandle GetHeapHandle() const { return queue->heap_handle(); }
  };

  IntrusiveHeap<ScheduledDelayedWakeUp, std::greater<>> wake_up_queue_;
  int pending_high_res_wake_up_count_ = 0;

  scoped_refptr<internal::AssociatedThreadId> associated_thread_;
};

// Default WakeUpQueue implementation that forwards wake-ups to
// `sequence_manager_`.
class BASE_EXPORT DefaultWakeUpQueue : public WakeUpQueue {
 public:
  DefaultWakeUpQueue(
      scoped_refptr<internal::AssociatedThreadId> associated_thread,
      internal::SequenceManagerImpl* sequence_manager);
  ~DefaultWakeUpQueue() override;

 private:
  // WakeUpQueue implementation:
  void OnNextDelayedWakeUpChanged(
      LazyNow* lazy_now,
      absl::optional<DelayedWakeUp> wake_up) override;
  const char* GetName() const override;
  void UnregisterQueue(internal::TaskQueueImpl* queue) override;

  internal::SequenceManagerImpl* sequence_manager_;  // Not owned.
};

// WakeUpQueue implementation that doesn't sends wake-ups to
// any SequenceManager, such that task queues don't cause wake-ups.
class BASE_EXPORT NonWakingWakeUpQueue : public WakeUpQueue {
 public:
  explicit NonWakingWakeUpQueue(
      scoped_refptr<internal::AssociatedThreadId> associated_thread);
  ~NonWakingWakeUpQueue() override;

 private:
  // WakeUpQueue implementation:
  void OnNextDelayedWakeUpChanged(
      LazyNow* lazy_now,
      absl::optional<DelayedWakeUp> wake_up) override;
  const char* GetName() const override;
  void UnregisterQueue(internal::TaskQueueImpl* queue) override;
};

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base

#endif  // BASE_TASK_SEQUENCE_MANAGER_TIME_DOMAIN_H_
