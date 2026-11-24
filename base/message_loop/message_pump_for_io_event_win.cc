// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/message_loop/message_pump_for_io_event_win.h"

#include <windows.h>

#include <stddef.h>

#include <algorithm>
#include <array>
#include <atomic>
#include <memory>
#include <optional>
#include <utility>
#include <vector>

#include "base/check.h"
#include "base/check_op.h"
#include "base/compiler_specific.h"
#include "base/containers/circular_deque.h"
#include "base/containers/span.h"
#include "base/memory/raw_ref.h"
#include "base/message_loop/message_pump_wakeup_counter.h"
#include "base/notreached.h"
#include "base/numerics/safe_conversions.h"
#include "base/sequence_checker.h"
#include "base/synchronization/lock.h"
#include "base/thread_annotations.h"
#include "base/threading/platform_thread.h"
#include "base/threading/simple_thread.h"
#include "base/threading/thread_checker.h"
#include "base/threading/thread_restrictions.h"
#include "base/trace_event/interned_args_helper.h"
#include "base/trace_event/typed_macros.h"
#include "base/win/scoped_handle.h"
#include "third_party/perfetto/protos/perfetto/trace/track_event/chrome_message_pump.pbzero.h"

namespace base {

namespace internal {

// Allows the pump thread to wait for bounded handshakes with its `WaitShard`
// worker threads (see `WaitShard::StopWatching()` and
// `WaitShard::StopAndJoin()`) even on threads that disallow blocking and
// //base sync primitives, e.g. BrowserProcessIOThread. These waits cannot
// deadlock: worker threads never run tasks and never wait on the pump thread;
// they only briefly acquire locks that the pump thread never holds while
// waiting.
class MessagePumpForIOEventScopedAllowBaseSyncPrimitives
    : public ScopedAllowBaseSyncPrimitivesOutsideBlockingScope {};

}  // namespace internal

static_assert(MessagePumpForIOEventImpl::kMaxDirectWatches ==
              MAXIMUM_WAIT_OBJECTS - 2);
static_assert(MessagePumpForIOEventImpl::kMaxShardWatches ==
              MAXIMUM_WAIT_OBJECTS - 1);

// Coordinates overflow I/O event watches that exceed the pump thread's direct
// WaitForMultipleObjects() capacity (`kMaxDirectWatches`).
//
// `ShardState` owns a dynamic list of `WaitShard` worker threads and a thread-
// safe FIFO queue of `PendingCompletion` records. When any `WaitShard` observes
// a signaled I/O event, it disarms that watch locally, appends a
// `PendingCompletion` to `pending_completions_`, and signals the manual-reset
// `completion_available_event_`. The pump thread includes
// `completion_available_handle()` in its own WaitForMultipleObjects() array;
// when woken by it, the pump pops one `PendingCompletion` via
// `TakeCompletion()`, rearms the watch on its owning `WaitShard`, and invokes
// the controller's `IOEventHandler` on the pump thread.
//
// Except for `QueueCompletion()` (which is called on `WaitShard` worker
// threads), all methods are called exclusively on the pump thread.
class MessagePumpForIOEventImpl::ShardState {
 public:
  // A single signaled watch queued by a `WaitShard` for dispatch on the pump
  // thread.
  struct PendingCompletion {
    // The controller whose event was signaled.
    raw_ref<IOEventWatchController> controller;
    // The shard that observed the signal and must be rearmed before the pump
    // dispatches the callback.
    raw_ref<WaitShard> owner;
  };

  ShardState();
  ~ShardState();

  // Returns the number of `WaitShard` instances currently owned.
  size_t shard_count() const {
    DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
    return shards_.size();
  }

  // Returns the number of completions queued for dispatch. Reads
  // `pending_completion_count_` without taking `completion_lock_`, so polling
  // it does not contend with `WaitShard` workers queuing completions.
  size_t pending_completion_count() const {
    return pending_completion_count_.load(std::memory_order_relaxed);
  }

  // Assigns `controller` to the first existing `WaitShard` with spare capacity,
  // or spawns a new `WaitShard` if all existing shards are full.
  void Watch(IOEventWatchController* controller);

  // Stops watching `controller` on whichever `WaitShard` owns it, purges any
  // queued completions for `controller` that have not yet been dispatched, and
  // prunes idle shards using `direct_watch_count` (the current number of
  // watches in the pump's direct wait set).
  void StopWatching(IOEventWatchController* controller,
                    size_t direct_watch_count);

  // Stops and destroys empty `WaitShard` threads once the preceding tier drops
  // to or below half capacity.
  void PruneEmptyShards(size_t direct_watch_count);

  // Pops and returns the oldest queued completion, resetting
  // `completion_available_event_` when the queue becomes empty. Must only be
  // called when at least one completion is queued.
  PendingCompletion TakeCompletion();

  // Synchronously polls armed registrations across all shards. Called by
  // `PollForIOBeforeIdle()` when the run loop would otherwise go idle. The
  // purpose is to check for the case where an IO watched by a `WaitShard` has
  // completed but `completion_available_event_` has not been signaled yet
  // because the `WaitShard` worker thread has not yet woken up and forwarded
  // the event. Without this extra check, extra
  // `DoIdleWork()`/`BeforeWait()` happen during synchronous pipe reads. Returns
  // the signaled controller if found, or `nullptr` if none are signaled.
  //
  // This is best-effort: a worker can consume a shard's auto-reset event and be
  // preempted before queuing the completion, in which case this returns
  // `nullptr`. That is harmless; the pump may run one extra `DoIdleWork()` and
  // then wakes on `completion_available_event_` once the completion is queued.
  IOEventWatchController* PollForCompletion();

  // Returns a manual-reset event handle that remains signaled while
  // `pending_completions_` is non-empty. Waited on by the pump thread in
  // `WaitForIOCompletion()`.
  HANDLE completion_available_handle() const {
    return completion_available_event_.handle();
  }

 private:
  friend class WaitShard;

  // Pops and returns the oldest queued completion if any, resetting
  // `completion_available_event_` when the queue becomes empty.
  std::optional<PendingCompletion> TryTakeCompletion()
      VALID_CONTEXT_REQUIRED(sequence_checker_);

  // Enqueues a signaled completion from a `WaitShard` worker thread and signals
  // `completion_available_event_` to wake the pump thread.
  void QueueCompletion(IOEventWatchController& controller, WaitShard& owner);

  Lock completion_lock_;

  // FIFO queue of signaled completions awaiting dispatch on the pump thread.
  circular_deque<PendingCompletion> pending_completions_
      GUARDED_BY(completion_lock_);

  // Mirrors `pending_completions_.size()`. Written under `completion_lock_`
  // and read without it by `PollForCompletion()` (to skip acquiring the lock
  // when nothing is queued) and by `pending_completion_count()`. A stale zero
  // only delays dispatch until the pump wakes on
  // `completion_available_event_`.
  std::atomic<size_t> pending_completion_count_{0};

  // Manual-reset event that stays signaled whenever `pending_completions_` is
  // non-empty and is reset when `TakeCompletion()` or `StopWatching()` drains
  // the last entry.
  WaitableEvent completion_available_event_{WaitableEvent::ResetPolicy::MANUAL};

  SEQUENCE_CHECKER(sequence_checker_);

  // Active and hysteresis-retained worker shards. Accessed only on the pump
  // thread.
  std::vector<std::unique_ptr<WaitShard>> shards_
      GUARDED_BY_CONTEXT(sequence_checker_);

  // Index of the shard at which the next `PollForCompletion()` scan starts,
  // giving round-robin fairness without reordering `shards_`, whose order
  // `Watch()` and `PruneEmptyShards()` rely on. Taken modulo `shards_.size()`,
  // so it remains valid after shards are pruned.
  size_t next_poll_shard_index_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;
};

// A dedicated worker thread that waits on up to `kMaxShardWatches` (63)
// `IOEventWatchController` events via Win32 `WaitForMultipleObjects()`, plus
// one internal `configuration_changed_event_` in slot 0 used to wake the worker
// when registrations are added, removed, rearmed, or stopped.
//
// When a watched event signals, `Run()` marks its `Registration` as disarmed
// (`is_armed = false`) so the worker will not wait on the caller's event again
// while the completion is in flight, rotates the registration to the back of
// `registrations_` for round-robin fairness, and enqueues the completion on
// `owner_`. The pump thread calls `Rearm()` before invoking the handler so the
// watch is included in subsequent waits unless the handler stops it.
//
// Owned by `ShardState`, which calls `StopAndJoin()` before destruction. Only
// `Run()` executes on the worker thread; all other methods run on the pump
// thread.
class MessagePumpForIOEventImpl::WaitShard final : public SimpleThread {
 public:
  enum class StopWatchingResult {
    kNotFound,
    kRemovedDisarmed,
    kRemovedArmed,
  };

  // The constructor takes an `initial_controller` to register because there is
  // no need to create an empty WaitShard.
  explicit WaitShard(ShardState* owner,
                     IOEventWatchController* initial_controller)
      : SimpleThread("MessagePumpForIOEventShard",
                     Options(PlatformThread::GetCurrentThreadType())),
        owner_(*owner),
        registrations_({Registration(raw_ref(*initial_controller))}),
        // Initializing to 1 since we've added a registration on construction
        watch_count_(1) {
    // Waited on only by `Run()` via raw `WaitForMultipleObjects()`.
    configuration_changed_event_.declare_only_used_while_idle();
  }
  ~WaitShard() override = default;

  // Returns the number of controllers currently registered on this shard.
  size_t WatchCount() const;

  // Registers `controller` on this shard and wakes the worker thread to update
  // its wait array. Returns false without modifying the shard if it already has
  // `kMaxShardWatches` registrations.
  bool Watch(IOEventWatchController* controller);

  // Removes `controller` if it is registered on this shard and blocks until the
  // worker thread has rebuilt its wait array without `controller`'s handle.
  // Returns `kNotFound` if `controller` is not registered on this shard.
  // Otherwise returns `kRemovedDisarmed` if the registration was disarmed when
  // removed, meaning a completion for `controller` may still be queued on
  // `owner_`, or `kRemovedArmed` if it was armed, meaning none can be queued.
  StopWatchingResult StopWatching(IOEventWatchController* controller);

  // Marks `controller` as armed again after its queued completion has been
  // claimed by the pump thread, and wakes the worker thread to resume waiting
  // on `controller`'s event.
  void Rearm(IOEventWatchController* controller);

  // Synchronously polls this shard's armed registrations with a zero-timeout
  // `WaitForMultipleObjects()`. The handles are copied under `lock_` and the
  // wait happens without it, so the worker thread is not blocked during the
  // syscall; `lock_` is reacquired only on a hit, to rotate `registrations_`.
  // Copied handles stay valid because only the pump thread removes
  // registrations. Returns the signaled controller if found, or `nullptr` if
  // none are signaled.
  IOEventWatchController* PollForCompletion();

  // Signals the worker thread to exit, joins it, and detaches any remaining
  // registered controllers from the pump.
  void StopAndJoin();

 private:
  // Per-controller state tracked by this shard.
  struct Registration {
    raw_ref<IOEventWatchController> controller;

    // True when the worker thread should include `controller->event_handle()`
    // in its `WaitForMultipleObjects()` call. Set to false when the worker
    // observes a signal and queues a completion, and restored to true by
    // `Rearm()` before the pump dispatches the completion callback.
    bool is_armed = true;
  };

  // Worker thread main loop: builds the handle array from armed registrations,
  // waits via `WaitForMultipleObjects()`, and forwards signaled events to
  // `owner_->QueueCompletion()`.
  void Run() override;

  SEQUENCE_CHECKER(sequence_checker_);

  // Owning `ShardState` that receives queued completions. Outlives `this`.
  const raw_ref<ShardState> owner_;

  Lock lock_;

  // Controllers currently assigned to this shard, rotated on completion to
  // prevent a busy handle from starving others in the same shard.
  std::vector<Registration> registrations_ GUARDED_BY(lock_);

  // Pump-thread-only mirror of `registrations_.size()` for lock-free count
  // and capacity checks.
  size_t watch_count_ GUARDED_BY_CONTEXT(sequence_checker_) = 0;

  // Set by `StopWatching()` when a registration is removed; cleared by `Run()`
  // once the worker thread has rebuilt its local wait array without the removed
  // handle and signaled `configuration_applied_event_`.
  bool removal_pending_ GUARDED_BY(lock_) = false;

  // Set by `StopAndJoin()` to instruct `Run()` to exit its wait loop.
  bool should_stop_ GUARDED_BY(lock_) = false;

  // Auto-reset event at wait slot 0 that wakes `Run()` whenever
  // `registrations_` or `should_stop_` changes.
  WaitableEvent configuration_changed_event_{
      WaitableEvent::ResetPolicy::AUTOMATIC};

  // Auto-reset event signaled by `Run()` to acknowledge that a pending
  // removal has taken effect and the worker is no longer waiting on the removed
  // handle.
  WaitableEvent configuration_applied_event_{
      WaitableEvent::ResetPolicy::AUTOMATIC};
};

size_t MessagePumpForIOEventImpl::WaitShard::WatchCount() const {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return watch_count_;
}

bool MessagePumpForIOEventImpl::WaitShard::Watch(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (watch_count_ == kMaxShardWatches) {
    return false;
  }
  {
    AutoLock lock(lock_);
    registrations_.push_back({raw_ref(*controller)});
    ++watch_count_;
  }
  configuration_changed_event_.Signal();
  return true;
}

MessagePumpForIOEventImpl::WaitShard::StopWatchingResult
MessagePumpForIOEventImpl::WaitShard::StopWatching(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (watch_count_ == 0) {
    return StopWatchingResult::kNotFound;
  }
  bool is_armed = false;
  {
    AutoLock lock(lock_);
    auto it = std::ranges::find_if(
        registrations_, [controller](const Registration& registration) {
          return &registration.controller.get() == controller;
        });
    if (it == registrations_.end()) {
      return StopWatchingResult::kNotFound;
    }
    is_armed = it->is_armed;
    registrations_.erase(it);
    --watch_count_;
    removal_pending_ = true;
  }
  configuration_changed_event_.Signal();

  // Watched events are caller-owned auto-reset events that may be closed,
  // reset, or rewatched immediately after StopWatching() returns. Wait until
  // the worker rebuilds its native wait array so an in-flight
  // WaitForMultipleObjects cannot consume a post-stop signal or wait on a
  // closed handle. This blocks mid-task, so `configuration_applied_event_` is
  // intentionally not declared idle: `Wait()` reports a blocking call and
  // terminates the flow emitted by `Signal()`.
  internal::MessagePumpForIOEventScopedAllowBaseSyncPrimitives allow_wait;
  configuration_applied_event_.Wait();
  return is_armed ? StopWatchingResult::kRemovedArmed
                  : StopWatchingResult::kRemovedDisarmed;
}

void MessagePumpForIOEventImpl::WaitShard::Rearm(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    AutoLock lock(lock_);
    auto it = std::ranges::find_if(
        registrations_, [controller](const Registration& registration) {
          return &registration.controller.get() == controller;
        });
    CHECK(it != registrations_.end());
    CHECK(!it->is_armed);
    it->is_armed = true;
  }
  configuration_changed_event_.Signal();
}

MessagePumpForIO::IOEventWatchController*
MessagePumpForIOEventImpl::WaitShard::PollForCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  if (watch_count_ == 0) {
    return nullptr;
  }

  size_t count = 0;
  STACK_UNINITIALIZED std::array<HANDLE, kMaxShardWatches> events;
  {
    AutoLock lock(lock_);
    for (const auto& registration : registrations_) {
      if (registration.is_armed) {
        events[count++] = registration.controller->event_handle();
      }
    }
  }
  if (count == 0) {
    return nullptr;
  }
  const DWORD result = ::WaitForMultipleObjects(checked_cast<DWORD>(count),
                                                events.data(), FALSE, 0);
  if (result == WAIT_TIMEOUT) {
    return nullptr;
  }

  PCHECK(result != WAIT_FAILED);
  CHECK_LT(result - WAIT_OBJECT_0, count);
  HANDLE completed_event = events[result - WAIT_OBJECT_0];
  {
    AutoLock lock(lock_);
    auto it = std::ranges::find(
        registrations_, completed_event, [](const Registration& registration) {
          return registration.controller->event_handle();
        });
    CHECK(it != registrations_.end());
    IOEventWatchController* controller = &it->controller.get();
    std::ranges::rotate(it, it + 1, registrations_.end());
    return controller;
  }
}

void MessagePumpForIOEventImpl::WaitShard::StopAndJoin() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  {
    AutoLock lock(lock_);
    should_stop_ = true;
  }
  configuration_changed_event_.Signal();
  {
    internal::MessagePumpForIOEventScopedAllowBaseSyncPrimitives allow_join;
    Join();
  }
  AutoLock lock(lock_);
  for (const auto& registration : registrations_) {
    registration.controller->pump_ = nullptr;
    registration.controller->handler_ = nullptr;
  }
}

void MessagePumpForIOEventImpl::WaitShard::Run() {
  // Reused across iterations to avoid re-initializing the array on every wait.
  // Only the first `count` entries are populated and passed to
  // `WaitForMultipleObjects()` on each iteration; any stale handle values left
  // in indices `>= count` after a watch is disarmed or removed are never read.
  std::array<HANDLE, MAXIMUM_WAIT_OBJECTS> events;
  events[0] = configuration_changed_event_.handle();
  // Watched event signaled by the previous wait, if any. Handled at the top of
  // the next iteration so `lock_` is acquired only once per iteration.
  HANDLE completed_event = nullptr;
  while (true) {
    size_t count = 1;
    IOEventWatchController* completed_controller = nullptr;
    bool removal_applied = false;
    {
      AutoLock lock(lock_);
      if (completed_event) {
        // The registration may have been removed by `StopWatching()` while
        // the worker was waiting; if so, the signal is dropped.
        auto it =
            std::ranges::find(registrations_, completed_event,
                              [](const Registration& registration) {
                                return registration.controller->event_handle();
                              });
        if (it != registrations_.end()) {
          CHECK(it->is_armed);
          it->is_armed = false;
          completed_controller = &it->controller.get();
          std::ranges::rotate(it, it + 1, registrations_.end());
        }
      }
      // A disarmed completion that is not queued is safe to drop here because
      // `StopAndJoin()` detaches all remaining registrations from the pump.
      if (should_stop_) {
        return;
      }
      for (const auto& registration : registrations_) {
        if (registration.is_armed) {
          events[count++] = registration.controller->event_handle();
        }
      }
      removal_applied = std::exchange(removal_pending_, false);
    }

    // Queue the completion and acknowledge removals outside `lock_` so the
    // pump thread, once woken, does not immediately block on `lock_` in
    // `Rearm()`. The completion must be queued before
    // `configuration_applied_event_` is signaled: `StopWatching()` blocks on
    // that event, which keeps `completed_controller` alive until it is queued,
    // and `ShardState::StopWatching()` then purges it from the queue.
    if (completed_controller) {
      owner_->QueueCompletion(*completed_controller, *this);
    }
    if (removal_applied) {
      configuration_applied_event_.Signal();
    }

    const DWORD result = ::WaitForMultipleObjects(
        checked_cast<DWORD>(count), events.data(), FALSE, INFINITE);
    PCHECK(result != WAIT_FAILED);
    CHECK_LT(result - WAIT_OBJECT_0, count);
    completed_event =
        result == WAIT_OBJECT_0 ? nullptr : events[result - WAIT_OBJECT_0];
  }
}

MessagePumpForIOEventImpl::ShardState::ShardState() {
  completion_available_event_.declare_only_used_while_idle();
}

MessagePumpForIOEventImpl::ShardState::~ShardState() {
  for (const auto& shard : shards_) {
    shard->StopAndJoin();
  }
  pending_completions_.clear();
}

void MessagePumpForIOEventImpl::ShardState::Watch(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Reuse an existing shard with free capacity (including an empty shard
  // retained within the hysteresis window) before spawning a new thread.
  for (const auto& shard : shards_) {
    if (shard->Watch(controller)) {
      return;
    }
  }
  auto shard = std::make_unique<WaitShard>(this, controller);
  shard->StartAsync();
  shards_.push_back(std::move(shard));
}

void MessagePumpForIOEventImpl::ShardState::StopWatching(
    IOEventWatchController* controller,
    size_t direct_watch_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  for (const auto& shard : shards_) {
    WaitShard::StopWatchingResult result = shard->StopWatching(controller);
    if (result == WaitShard::StopWatchingResult::kNotFound) {
      continue;
    }
    // If a disarmed controller was removed then there is potentially a
    // pending completion for it that needs to be flushed.
    if (result == WaitShard::StopWatchingResult::kRemovedDisarmed) {
      // `WaitShard::StopWatching()` returned only after the worker
      // acknowledged the removal, which it does after queuing any completion
      // it had already claimed for `controller`. No later completion can be
      // queued, so purging the queue here drops every stale completion.
      {
        AutoLock lock(completion_lock_);
        size_t erase_result =
            EraseIf(pending_completions_, [controller](const auto& completion) {
              return &completion.controller.get() == controller;
            });
        if (erase_result > 0) {
          pending_completion_count_.store(pending_completions_.size(),
                                          std::memory_order_relaxed);
          if (pending_completions_.empty()) {
            completion_available_event_.Reset();
          }
        }
      }
    }
    PruneEmptyShards(direct_watch_count);
    return;
  }
  NOTREACHED();
}

void MessagePumpForIOEventImpl::ShardState::PruneEmptyShards(
    size_t direct_watch_count) {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // Retain an empty shard while its preceding tier (the pump's direct-watch
  // array for the first shard, or the previous shard for subsequent shards) is
  // more than half full. This hysteresis avoids repeated thread creation/join
  // churn when watch counts fluctuate near a shard boundary, while still
  // reclaiming idle worker threads once watch usage drops well below the
  // threshold.
  for (size_t i = shards_.size(); i-- > 0;) {
    if (shards_[i]->WatchCount() != 0) {
      continue;
    }
    const size_t preceding_count =
        i == 0 ? direct_watch_count : shards_[i - 1]->WatchCount();
    const size_t preceding_capacity =
        i == 0 ? kMaxDirectWatches : kMaxShardWatches;
    if (preceding_count > preceding_capacity / 2) {
      continue;
    }
    shards_[i]->StopAndJoin();
    shards_.erase(shards_.begin() + checked_cast<ptrdiff_t>(i));
  }
}

MessagePumpForIOEventImpl::ShardState::PendingCompletion
MessagePumpForIOEventImpl::ShardState::TakeCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  return TryTakeCompletion().value();
}

MessagePumpForIO::IOEventWatchController*
MessagePumpForIOEventImpl::ShardState::PollForCompletion() {
  DCHECK_CALLED_ON_VALID_SEQUENCE(sequence_checker_);
  // With no shards there are no sharded watches, so nothing can be queued.
  if (shards_.empty()) {
    return nullptr;
  }

  const size_t shard_count = shards_.size();
  for (size_t i = 0; i < shard_count; ++i) {
    const size_t index = (next_poll_shard_index_ + i) % shard_count;
    if (IOEventWatchController* controller =
            shards_[index]->PollForCompletion()) {
      next_poll_shard_index_ = (index + 1) % shard_count;
      return controller;
    }
  }

  // Check the queue after scanning the shards, so a completion that a worker
  // queued during the scan is still found. A worker may also have disarmed a
  // registration but not yet queued it; that completion is missed here, but
  // queuing it signals `completion_available_event_`, which wakes the pump.
  // Skip the lock when nothing appears queued.
  if (pending_completion_count_.load(std::memory_order_relaxed) == 0) {
    return nullptr;
  }
  if (std::optional<PendingCompletion> completion = TryTakeCompletion()) {
    IOEventWatchController* controller = &completion->controller.get();
    completion->owner->Rearm(controller);
    return controller;
  }
  return nullptr;
}

std::optional<MessagePumpForIOEventImpl::ShardState::PendingCompletion>
MessagePumpForIOEventImpl::ShardState::TryTakeCompletion() {
  AutoLock lock(completion_lock_);
  if (pending_completions_.empty()) {
    return std::nullopt;
  }
  auto completion = std::move(pending_completions_.front());
  pending_completions_.pop_front();
  pending_completion_count_.store(pending_completions_.size(),
                                  std::memory_order_relaxed);
  if (pending_completions_.empty()) {
    completion_available_event_.Reset();
  }
  return completion;
}

void MessagePumpForIOEventImpl::ShardState::QueueCompletion(
    IOEventWatchController& controller,
    WaitShard& owner) {
  AutoLock lock(completion_lock_);
  const bool was_empty = pending_completions_.empty();
  pending_completions_.push_back({raw_ref(controller), raw_ref(owner)});
  // `completion_available_event_` is a manual-reset event that is reset only
  // when `pending_completions_` becomes empty, so it is already signaled
  // unless the queue was empty. Signal only on that transition, and do so
  // under `completion_lock_`: otherwise the pump could take this completion
  // first, and the late signal would wake it with an empty queue.
  if (was_empty) {
    completion_available_event_.Signal();
  }
  pending_completion_count_.store(pending_completions_.size(),
                                  std::memory_order_relaxed);
}

MessagePumpForIO::IOEventHandler::IOEventHandler(const Location& from_here)
    : io_handler_location_(from_here) {}

MessagePumpForIO::IOEventWatchController::IOEventWatchController(
    win::ScopedHandle event_handle)
    : event_(std::move(event_handle)) {
  event_.declare_only_used_while_idle();
}

MessagePumpForIO::IOEventWatchController::~IOEventWatchController() {
  StopWatching();
}

HANDLE MessagePumpForIO::IOEventWatchController::event_handle() const {
  return event_.handle();
}

void MessagePumpForIO::IOEventWatchController::StopWatching() {
  if (pump_) {
    pump_->StopWatching(this);
  }
}

MessagePumpForIOEventImpl::MessagePumpForIOEventImpl() {
  schedule_work_event_.declare_only_used_while_idle();
}

MessagePumpForIOEventImpl::~MessagePumpForIOEventImpl() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  shard_state_.reset();
  for (IOEventWatchController* controller : watches_) {
    if (controller) {
      controller->pump_ = nullptr;
      controller->handler_ = nullptr;
    }
  }
}

bool MessagePumpForIOEventImpl::ScheduleNativeWork() {
  schedule_work_event_.Signal();
  return true;
}

bool MessagePumpForIOEventImpl::SupportsIOEvents() const {
  return true;
}

bool MessagePumpForIOEventImpl::WatchIOEvent(IOEventWatchController* controller,
                                             IOEventHandler* handler) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  CHECK(controller);
  CHECK(handler);
  CHECK(!controller->pump_);
  controller->pump_ = this;
  controller->handler_ = handler;
  const size_t direct_watch_count =
      watches_.size() - static_cast<size_t>(shard_state_ != nullptr);
  if (direct_watch_count == kMaxDirectWatches) {
    if (!shard_state_) {
      shard_state_ = std::make_unique<ShardState>();
      events_.back() = shard_state_->completion_available_handle();
      watches_.push_back(nullptr);
      events_.push_back(schedule_work_event_.handle());
    }
    shard_state_->Watch(controller);
    return true;
  }
  events_.back() = controller->event_handle();
  watches_.push_back(controller);
  // Tasks run between callbacks, so give ready I/O priority over task wakeups.
  events_.push_back(schedule_work_event_.handle());
  return true;
}

bool MessagePumpForIOEventImpl::RegisterIOHandler(HANDLE file_handle,
                                                  IOHandler* handler) {
  // IOCP registration is unavailable in an event-based I/O pump.
  NOTREACHED();
}

bool MessagePumpForIOEventImpl::RegisterJobObject(HANDLE job_handle,
                                                  IOHandler* handler) {
  // Job registration requires an I/O completion port.
  NOTREACHED();
}

size_t MessagePumpForIOEventImpl::GetShardCountForTesting() const {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  return shard_state_ ? shard_state_->shard_count() : 0;
}

size_t MessagePumpForIOEventImpl::GetPendingShardCompletionCountForTesting()
    const {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  return shard_state_ ? shard_state_->pending_completion_count() : 0;
}

void MessagePumpForIOEventImpl::StopWatching(
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  const size_t direct_watch_count =
      watches_.size() - static_cast<size_t>(shard_state_ != nullptr);
  const auto watch = std::ranges::find(watches_, controller);
  if (watch != watches_.end()) {
    const auto index = watch - watches_.begin();
    events_.erase(events_.begin() + index);
    watches_.erase(watch);
    if (shard_state_) {
      shard_state_->PruneEmptyShards(direct_watch_count - 1);
    }
  } else {
    CHECK(shard_state_);
    shard_state_->StopWatching(controller, direct_watch_count);
  }
  controller->pump_ = nullptr;
  controller->handler_ = nullptr;
}

bool MessagePumpForIOEventImpl::WaitForIOCompletion(DWORD timeout) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  // A native-I/O poll must not consume task wakeups.
  const size_t count = watches_.size() + (timeout ? 1u : 0u);
  if (count == 0) {
    return false;
  }
  const DWORD result = ::WaitForMultipleObjects(checked_cast<DWORD>(count),
                                                events_.data(), FALSE, timeout);
  if (result == WAIT_TIMEOUT) {
    return false;
  }
  PCHECK(result != WAIT_FAILED);
  CHECK_LT(result - WAIT_OBJECT_0, count);
  MessagePumpWakeupCounter::GetForCurrentThread().RecordWakeup();
  if (result - WAIT_OBJECT_0 == watches_.size()) {
    native_msg_scheduled_.store(false, std::memory_order_relaxed);
    return true;
  }

  const size_t index = result - WAIT_OBJECT_0;
  IOEventWatchController* controller = watches_[index];
  if (!controller) {
    auto completion = shard_state_->TakeCompletion();
    controller = &completion.controller.get();
    // Rearm before calling out: the callback may stop or destroy the watch.
    completion.owner->Rearm(controller);
  }
  DispatchIOEvent(index, controller);
  return true;
}

bool MessagePumpForIOEventImpl::PollForIOBeforeIdle() {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  if (!shard_state_) {
    return false;
  }
  IOEventWatchController* controller = shard_state_->PollForCompletion();
  if (!controller) {
    return false;
  }
  MessagePumpWakeupCounter::GetForCurrentThread().RecordWakeup();
  // Attribute the dispatch to the aggregate shard slot for rotation purposes.
  const auto sentinel = std::ranges::find(watches_, nullptr);
  CHECK(sentinel != watches_.end());
  DispatchIOEvent(static_cast<size_t>(sentinel - watches_.begin()), controller);
  return true;
}

void MessagePumpForIOEventImpl::DispatchIOEvent(
    size_t index,
    IOEventWatchController* controller) {
  DCHECK_CALLED_ON_VALID_THREAD(bound_thread_);
  IOEventHandler* handler = controller->handler_;

  // Move the signaled watch to the back so a busy event cannot starve others.
  auto watches = span(watches_).subspan(index);
  std::ranges::rotate(watches, watches.begin() + 1);
  auto events = span(events_).subspan(index, watches.size());
  std::ranges::rotate(events, events.begin() + 1);

  run_state_->delegate->BeginNativeWorkBeforeDoWork();
  auto scoped_do_work_item = run_state_->delegate->BeginWorkItem();
  TRACE_EVENT(
      "base,toplevel", "IOEventHandler::OnIOEventSignaled",
      [&](perfetto::EventContext ctx) {
        ctx.event()->set_chrome_message_pump()->set_io_handler_location_iid(
            trace_event::InternedSourceLocation::Get(
                &ctx, trace_event::TraceSourceLocation(
                          handler->io_handler_location())));
      });
  handler->OnIOEventSignaled(controller);
}

}  // namespace base
