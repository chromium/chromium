// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/slot_start.h"
#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

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
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_stats.h"

namespace partition_alloc {

struct PartitionRoot;
class ThreadCache;
struct SchedulerLoopQuarantineStats;

namespace internal {

struct SchedulerLoopQuarantineConfig {
  // Capacity for a branch in bytes.
  size_t branch_capacity_in_bytes = 0;
  // Leak quarantined allocations at exit.
  bool leak_on_destruction = false;
  bool enable_quarantine = false;
  bool enable_zapping = false;
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

  // TODO(ayumiohno): Remove this once FreeAfterBRPQuarantine creates
  // `size_details` and uses QuarantineWithSize.
  void Quarantine(SlotStart slot_start, SlotSpanMetadata* slot_span)
      PA_LOCKS_EXCLUDED(lock_);

  void QuarantineWithSize(SlotStart slot_start,
                          SlotSpanMetadata* slot_span,
                          const internal::BucketSizeDetails& size_details)
      PA_LOCKS_EXCLUDED(lock_);

  void AllowScanlessPurge();
  void DisallowScanlessPurge();

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
  PA_ALWAYS_INLINE void PurgeInternal(
      size_t target_size_in_bytes,
      [[maybe_unused]] bool for_destruction = false)
      PA_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  PartitionRoot* const allocator_root_;
  ThreadCache* const tcache_;
  Root* root_;
  Lock lock_;

  // Non-cryptographic random number generator.
  // Thread-unsafe so guarded by `lock_`.
  base::InsecureRandomGenerator random_ PA_GUARDED_BY(lock_);

  bool enable_quarantine_ = false;
  bool enable_zapping_ = false;
  bool leak_on_destruction_ = false;

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
