// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"

// Scheduler-loop Quarantine is a quarantine pool behind PartitionAlloc with
// Advanced Checks and `ADVANCED_MEMORY_SAFETY_CHECKS()`.
// Both requests to prevent `free()`d allocation getting released to free-list,
// by passing `FreeFlags::kSchedulerLoopQuarantine` at time of `free()`.
// This will keep these allocations in Scheduler-Loop Quarantine for while.
// TODO(crbug.com/329027914): In addition to the threshold-based purging in
// Scheduler-Loop Quarantine, implement smarter purging strategy to detect
// "empty stack".
//
// - Built on PartitionAlloc: only supports allocations in a known root
// - As fast as PA: SLQ just defers `Free()` handling and may benefit from
// thread
//   cache etc.
// - Thread-safe
// - No allocation time information: triggered on `Free()`
// - Don't use quarantined objects' payload - available for zapping
// - Don't allocate heap memory.
// - Flexible to support several applications
//
// There is one `SchedulerLoopQuarantineRoot` for every `PartitionRoot`,
// and keeps track of size of quarantined allocations etc.
// `SchedulerLoopQuarantineBranch` provides an actual quarantine request
// interface. It belongs to a `SchedulerLoopQuarantineRoot` and there can be
// multiple instances (e.g. one per thread). By having one branch per thread, it
// requires no lock for faster quarantine.
// ┌────────────────────────────┐
// │PartitionRoot               │
// └┬───────────────────────────┘
// ┌▽────────────────────────┐
// │Quarantine Root          │
// └┬───────────┬───────────┬┘
// ┌▽─────────┐┌▽─────────┐┌▽─────────┐
// │Branch 1  ││Branch 2  ││Branch 3  │
// └──────────┘└──────────┘└──────────┘

#ifndef PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_H_
#define PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_H_

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <optional>
#include <type_traits>
#include <vector>

#include "partition_alloc/internal_allocator_forward.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/export_template.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/threading/platform_thread.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_stats.h"

namespace partition_alloc {

class PartitionRoot;
struct SchedulerLoopQuarantineStats;

namespace internal {

// Utility to disable thread safety analysis when we know it is safe.
class PA_SCOPED_LOCKABLE FakeScopedGuard {
 public:
  PA_ALWAYS_INLINE explicit FakeScopedGuard(Lock& lock)
      PA_EXCLUSIVE_LOCK_FUNCTION(lock) {}
  // For some reason, defaulting this causes a thread safety annotation failure.
  PA_ALWAYS_INLINE
  ~FakeScopedGuard()  // NOLINT(modernize-use-equals-default)
      PA_UNLOCK_FUNCTION() {}
};

class ThreadCache;

struct SchedulerLoopQuarantineConfig {
  // Capacity for a branch in bytes.
  size_t branch_capacity_in_bytes = 0;
  // Leak quarantined allocations at exit.
  bool leak_on_destruction = false;
  bool enable_quarantine = false;
  bool enable_zapping = false;
  bool enable_task_controlled_purge = false;
  bool pause_in_between_tasks = false;
  // Accepts allocations up to this bucket size. If the given number does not
  // match bucket size, it is rounded up to next bucket size.
  size_t max_quarantine_size = BucketIndexLookup::kMaxBucketSize;
  // For informational purposes only.
  char branch_name[32] = "";
};

struct BucketSizeDetails;

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) SchedulerLoopQuarantineRoot {
 public:
  explicit SchedulerLoopQuarantineRoot(PartitionRoot& allocator_root)
      : allocator_root_(allocator_root) {}

  PartitionRoot& GetAllocatorRoot() { return allocator_root_; }

  void AccumulateStats(SchedulerLoopQuarantineStats& stats) const {
    stats.count += count_.load(std::memory_order_relaxed);
    stats.size_in_bytes += size_in_bytes_.load(std::memory_order_relaxed);
    stats.cumulative_count += cumulative_count_.load(std::memory_order_relaxed);
    stats.cumulative_size_in_bytes +=
        cumulative_size_in_bytes_.load(std::memory_order_relaxed);
    stats.quarantine_miss_count +=
        quarantine_miss_count_.load(std::memory_order_relaxed);
  }

 private:
  PartitionRoot& allocator_root_;

  // Stats.
  std::atomic_size_t size_in_bytes_ = 0;
  std::atomic_size_t count_ = 0;  // Number of quarantined entries
  std::atomic_size_t cumulative_count_ = 0;
  std::atomic_size_t cumulative_size_in_bytes_ = 0;
  std::atomic_size_t quarantine_miss_count_ = 0;

  template <bool>
  friend class SchedulerLoopQuarantineBranch;
};

// When set to `thread_bound = true`, the branch is for single-thread use
// (faster).
template <bool thread_bound>
class SchedulerLoopQuarantineBranch {
 public:
  static constexpr bool kThreadBound = thread_bound;
  using Root = SchedulerLoopQuarantineRoot;

  explicit SchedulerLoopQuarantineBranch(PartitionRoot* allocator_root,
                                         ThreadCache* tcache = nullptr);
  SchedulerLoopQuarantineBranch(const SchedulerLoopQuarantineBranch&) = delete;
  SchedulerLoopQuarantineBranch(SchedulerLoopQuarantineBranch&& b) = delete;
  SchedulerLoopQuarantineBranch& operator=(
      const SchedulerLoopQuarantineBranch&) = delete;
  ~SchedulerLoopQuarantineBranch();

  void Configure(SchedulerLoopQuarantineRoot& root,
                 const SchedulerLoopQuarantineConfig& config)
      PA_LOCKS_EXCLUDED(lock_);
  Root& GetRoot() {
    PA_CHECK(enable_quarantine_ && root_);
    return *root_;
  }

  // Dequarantine all entries **held by this branch**.
  // It is possible that another branch with entries and it remains untouched.
  void Purge() PA_LOCKS_EXCLUDED(lock_);
  // Similar to `Purge()`, but marks this branch as unusable. Can be called
  // multiple times.
  void Destroy() PA_LOCKS_EXCLUDED(lock_);

  // Determines this list contains an object.
  bool IsQuarantinedForTesting(void* object) PA_LOCKS_EXCLUDED(lock_);

  size_t GetCapacityInBytes() {
    return branch_capacity_in_bytes_.load(std::memory_order_relaxed);
  }
  // After shrinking the capacity, this branch may need to `Purge()` to meet the
  // requirement.
  void SetCapacityInBytes(size_t capacity_in_bytes);

  void Quarantine(SlotStart slot_start,
                  SlotSpanMetadata* slot_span,
                  const internal::BucketSizeDetails& size_details)
      PA_LOCKS_EXCLUDED(lock_);

  // TODO(crbug.com/329027914): Make these private once migration to
  // TaskAnnotator is complete and SchedulerLoopQuarantineTaskObserver is
  // removed.
  PA_EXCLUDE_FROM_EXPLICIT_INSTANTIATION
  PA_ALWAYS_INLINE void AllowScanlessPurge()
    requires kThreadBound
  {
    // Always thread-bound; no need to lock.
    FakeScopedGuard guard(lock_);

    PA_CHECK(disallow_scanless_purge_ > 0);
    --disallow_scanless_purge_;
    if (disallow_scanless_purge_ == 0) {
      // Now scanless purge is allowed. Purging at this timing is more
      // performance efficient.
      PurgeInternal(0);
    }
  }

  PA_EXCLUDE_FROM_EXPLICIT_INSTANTIATION
  PA_ALWAYS_INLINE void DisallowScanlessPurge()
    requires kThreadBound
  {
    // Always thread-bound; no need to lock.
    FakeScopedGuard guard(lock_);

    ++disallow_scanless_purge_;
    PA_CHECK(disallow_scanless_purge_ > 0);  // Overflow check.
  }

  PA_EXCLUDE_FROM_EXPLICIT_INSTANTIATION
  PA_ALWAYS_INLINE void OnTaskStart()
    requires kThreadBound
  {
    PA_DCHECK(thread_id_ == base::PlatformThread::CurrentId());
    task_nesting_depth_++;
    // We only un-pause quarantine on entering the outermost task to avoid
    // decrementing `pause_quarantine_` multiple times in nested tasks.
    if (task_nesting_depth_ == 1) {
      if (pause_in_between_tasks_) {
        PA_DCHECK(pause_quarantine_ > 0);
        --pause_quarantine_;  // Un-pause
      }
    }
    // Both features require disallowing scanless purge during task execution.
    if (enable_task_controlled_purge_ || pause_in_between_tasks_) {
      DisallowScanlessPurge();
    }
  }

  PA_EXCLUDE_FROM_EXPLICIT_INSTANTIATION
  PA_ALWAYS_INLINE void OnTaskFinish()
    requires kThreadBound
  {
    PA_DCHECK(thread_id_ == base::PlatformThread::CurrentId());
    // Both features require allowing scanless purge (and potentially purging)
    // after task execution.
    if (enable_task_controlled_purge_ || pause_in_between_tasks_) {
      AllowScanlessPurge();
    }
    PA_DCHECK(task_nesting_depth_ > 0);
    task_nesting_depth_--;
    // We only restore the paused state on exiting the outermost task.
    if (task_nesting_depth_ == 0) {
      if (pause_in_between_tasks_) {
        ++pause_quarantine_;  // Pause
      }
    }
  }

  // Once called, all the branches stop purging. This means every branch grows
  // unbounded, potentially resulting in OOM. However, if we know the program
  // is being terminated, this can help reduce hangs.
  static void DangerouslyDisablePurge();

  const SchedulerLoopQuarantineConfig& GetConfigurationForTesting();

  class ScopedQuarantineExclusion {
    SchedulerLoopQuarantineBranch& branch_;

   public:
    PA_ALWAYS_INLINE explicit ScopedQuarantineExclusion(
        SchedulerLoopQuarantineBranch& branch)
        : branch_(branch) {
      PA_DCHECK(!branch.enable_quarantine_ || kThreadBound);
      ++branch_.pause_quarantine_;
    }
    ScopedQuarantineExclusion(const ScopedQuarantineExclusion&) = delete;
    PA_ALWAYS_INLINE ~ScopedQuarantineExclusion() {
      --branch_.pause_quarantine_;
    }
  };

  int PausedCountForTesting() { return pause_quarantine_; }

 private:
  // `ToBeFreedArray` is used in `Quarantine` and
  // `PurgeInternalWithDefferedFree`. See the function comment about the
  // purpose. In order to avoid reentrancy issues, we must not deallocate any
  // object in `Quarantine`. So, std::vector is not an option. std::array
  // doesn't deallocate, plus, std::array has perf advantages.
  static constexpr size_t kMaxFreeTimesPerPurge = 1024;
  using ToBeFreedArray = std::array<uintptr_t, kMaxFreeTimesPerPurge>;

  // Try to dequarantine entries to satisfy below:
  //   root_.size_in_bytes_ <=  target_size_in_bytes
  // It is possible that this branch cannot satisfy the
  // request as it has control over only what it has. If you need to ensure the
  // constraint, call `Purge()` for each branch in sequence, synchronously.
  void PurgeInternal(size_t target_size_in_bytes,
                     [[maybe_unused]] bool for_destruction = false)
      PA_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  PartitionRoot* const allocator_root_;
  ThreadCache* const tcache_;
  const base::PlatformThreadId thread_id_;
  Root* root_;
  Lock lock_;

  // Non-cryptographic random number generator.
  // Thread-unsafe so guarded by `lock_`.
  base::InsecureRandomGenerator random_ PA_GUARDED_BY(lock_);

  bool enable_quarantine_ = false;
  bool enable_zapping_ = false;
  bool leak_on_destruction_ = false;
  bool enable_task_controlled_purge_ = false;
  bool pause_in_between_tasks_ = false;
  int task_nesting_depth_ = 0;

  uint16_t largest_bucket_index_ = BucketIndexLookup::kNumBuckets - 1;

  // When non-zero, this branch temporarily stops accepting incoming quarantine
  // requests.
  int pause_quarantine_ = 0;

  // `slots_` hold quarantined entries.
  struct QuarantineSlot {
    SlotStart slot_start;
    // Record bucket index instead of slot size because look-up from bucket
    // index to slot size is more lightweight compared to its reverse look-up.
    size_t bucket_index = 0;
  };
  std::vector<QuarantineSlot, InternalAllocator<QuarantineSlot>> slots_
      PA_GUARDED_BY(lock_);
  size_t branch_size_in_bytes_ PA_GUARDED_BY(lock_) = 0;
  // Using `std::atomic` here so that other threads can update this value.
  std::atomic_size_t branch_capacity_in_bytes_ = 0;

  // TODO(http://crbug.com/329027914): Implement stack scanning, to be performed
  // when this value is non-zero.
  //
  // Currently, a scanless purge is always performed. However, this value is
  // still used as a hint to determine safer purge timings for memory
  // optimization.
  uint32_t disallow_scanless_purge_ PA_GUARDED_BY(lock_) = 0;

  // Debug and testing data.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  std::atomic_bool being_destructed_ = false;
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

  // Kept for testing purposes only.
  SchedulerLoopQuarantineConfig config_for_testing_;
};

using GlobalSchedulerLoopQuarantineBranch =
    SchedulerLoopQuarantineBranch<false>;
using ThreadBoundSchedulerLoopQuarantineBranch =
    SchedulerLoopQuarantineBranch<true>;

extern template class PA_EXPORT_TEMPLATE_DECLARE(
    PA_COMPONENT_EXPORT(PARTITION_ALLOC)) SchedulerLoopQuarantineBranch<false>;
extern template class PA_EXPORT_TEMPLATE_DECLARE(
    PA_COMPONENT_EXPORT(PARTITION_ALLOC)) SchedulerLoopQuarantineBranch<true>;

}  // namespace internal

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_SCHEDULER_LOOP_QUARANTINE_H_
