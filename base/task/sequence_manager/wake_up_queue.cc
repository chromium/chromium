// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/task/sequence_manager/wake_up_queue.h"

#include <optional>

#include "base/task/sequence_manager/associated_thread_id.h"
#include "base/task/sequence_manager/sequence_manager_impl.h"
#include "base/task/sequence_manager/task_queue_impl.h"
#include "base/threading/thread_checker.h"

namespace base {
namespace sequence_manager {
namespace internal {

WakeUpQueue::WakeUpQueue(
    scoped_refptr<const internal::AssociatedThreadId> associated_thread)
    : associated_thread_(std::move(associated_thread)) {}

WakeUpQueue::~WakeUpQueue() {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
}

void WakeUpQueue::RemoveAllCanceledDelayedTasksFromFront(LazyNow* lazy_now) {
  // Repeatedly trim the front of the top queue until it stabilizes. This is
  // needed because a different queue can become the top one once you remove the
  // canceled tasks.
  while (!wake_up_queue_.empty()) {
    auto* top_queue = wake_up_queue_.top().queue;

    // If no tasks are removed from the top queue, then it means the top queue
    // cannot change anymore.
    if (!top_queue->RemoveAllCanceledDelayedTasksFromFront(lazy_now))
      break;
  }
}

// TODO(kraynov): https://crbug.com/857101 Consider making an interface
// for SequenceManagerImpl which will expose SetNextDelayedDoWork and
// MaybeScheduleImmediateWork methods to make the functions below pure-virtual.

void WakeUpQueue::SetNextWakeUpForQueue(internal::TaskQueueImpl* queue,
                                        LazyNow* lazy_now,
                                        std::optional<WakeUp> wake_up) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  DCHECK_EQ(queue->wake_up_queue(), this);
  DCHECK(queue->IsQueueEnabled() || !wake_up);

  std::optional<WakeUp> previous_wake_up = GetNextDelayedWakeUp();
  std::optional<WakeUpResolution> previous_queue_resolution;
  if (queue->heap_handle().IsValid()) {
    previous_queue_resolution =
        wake_up_queue_.at(queue->heap_handle()).wake_up.resolution;
  }

  if (wake_up) {
    // Insert a new wake-up into the heap.
    if (queue->heap_handle().IsValid()) {
      // O(log n)
      wake_up_queue_.Replace(queue->heap_handle(), {wake_up.value(), queue});
    } else {
      // O(log n)
      wake_up_queue_.insert({wake_up.value(), queue});
    }
  } else {
    // Remove a wake-up from heap if present.
    if (queue->heap_handle().IsValid())
      wake_up_queue_.erase(queue->heap_handle());
  }

  std::optional<WakeUp> new_wake_up = GetNextDelayedWakeUp();

  if (previous_queue_resolution &&
      *previous_queue_resolution == WakeUpResolution::kHigh) {
    pending_high_res_wake_up_count_--;
  }
  if (wake_up && wake_up->resolution == WakeUpResolution::kHigh)
    pending_high_res_wake_up_count_++;
  DCHECK_GE(pending_high_res_wake_up_count_, 0);

  if (new_wake_up != previous_wake_up)
    OnNextWakeUpChanged(lazy_now, GetNextDelayedWakeUp());
}

void WakeUpQueue::MoveReadyDelayedTasksToWorkQueues(
    LazyNow* lazy_now,
    EnqueueOrder enqueue_order) {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  bool update_needed = false;
  while (!wake_up_queue_.empty() &&
         wake_up_queue_.top().wake_up.earliest_time() <= lazy_now->Now()) {
    internal::TaskQueueImpl* queue = wake_up_queue_.top().queue;
    // OnWakeUp() is expected to update the next wake-up for this queue with
    // SetNextWakeUpForQueue(), thus allowing us to make progress.
    queue->OnWakeUp(lazy_now, enqueue_order);
    update_needed = true;
  }

  if (!update_needed || wake_up_queue_.empty())
    return;
  // If any queue was notified, possibly update following queues. This ensures
  // the wake up is up to date, which is necessary because calling OnWakeUp() on
  // a throttled queue may affect state that is shared between other related
  // throttled queues. The wake up for an affected queue might be pushed back
  // and needs to be updated. This is done lazily only once the related queue
  // becomes the next one to wake up, since that wake up can't be moved up.
  // `wake_up_queue_` is non-empty here, per the condition above.
  internal::TaskQueueImpl* queue = wake_up_queue_.top().queue;
  queue->UpdateWakeUp(lazy_now);
  while (!wake_up_queue_.empty()) {
    internal::TaskQueueImpl* old_queue =
        std::exchange(queue, wake_up_queue_.top().queue);
    if (old_queue == queue)
      break;
    queue->UpdateWakeUp(lazy_now);
  }
}

std::optional<WakeUp> WakeUpQueue::GetNextDelayedWakeUp() const {
  DCHECK_CALLED_ON_VALID_THREAD(associated_thread_->thread_checker);
  if (wake_up_queue_.empty())
    return std::nullopt;
  WakeUp wake_up = wake_up_queue_.top().wake_up;
  // `wake_up.resolution` is not meaningful since it may be different from
  // has_pending_high_resolution_tasks(). Return WakeUpResolution::kLow here to
  // simplify comparison between wake ups.
  // TODO(crbug.com/40158967): Drive resolution by DelayPolicy and return
  // has_pending_high_resolution_tasks() here.
  wake_up.resolution = WakeUpResolution::kLow;
  return wake_up;
}

Value::Dict WakeUpQueue::AsValue(TimeTicks now) const {
  Value::Dict state;
  state.Set("name", GetName());
  // TODO(crbug.com/40228085): Make base::Value able to store an int64_t and
  // remove this cast.
  state.Set("registered_delay_count", checked_cast<int>(wake_up_queue_.size()));
  if (!wake_up_queue_.empty()) {
    TimeDelta delay = wake_up_queue_.top().wake_up.time - now;
    state.Set("next_delay_ms", delay.InMillisecondsF());
  }
  return state;
}

DefaultWakeUpQueue::DefaultWakeUpQueue(
    scoped_refptr<internal::AssociatedThreadId> associated_thread,
    internal::SequenceManagerImpl* sequence_manager)
    : WakeUpQueue(std::move(associated_thread)),
      sequence_manager_(sequence_manager) {}

DefaultWakeUpQueue::~DefaultWakeUpQueue() = default;

void DefaultWakeUpQueue::OnNextWakeUpChanged(LazyNow* lazy_now,
                                             std::optional<WakeUp> wake_up) {
  sequence_manager_->SetNextWakeUp(lazy_now, wake_up);
}

void DefaultWakeUpQueue::UnregisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK_EQ(queue->wake_up_queue(), this);
  LazyNow lazy_now(sequence_manager_->main_thread_clock());
  SetNextWakeUpForQueue(queue, &lazy_now, std::nullopt);
}

const char* DefaultWakeUpQueue::GetName() const {
  return "DefaultWakeUpQueue";
}

NonWakingWakeUpQueue::NonWakingWakeUpQueue(
    scoped_refptr<internal::AssociatedThreadId> associated_thread)
    : WakeUpQueue(std::move(associated_thread)) {}

NonWakingWakeUpQueue::~NonWakingWakeUpQueue() = default;

void NonWakingWakeUpQueue::OnNextWakeUpChanged(LazyNow* lazy_now,
                                               std::optional<WakeUp> wake_up) {}

const char* NonWakingWakeUpQueue::GetName() const {
  return "NonWakingWakeUpQueue";
}

void NonWakingWakeUpQueue::UnregisterQueue(internal::TaskQueueImpl* queue) {
  DCHECK_EQ(queue->wake_up_queue(), this);
  SetNextWakeUpForQueue(queue, nullptr, std::nullopt);
}

}  // namespace internal
}  // namespace sequence_manager
}  // namespace base
