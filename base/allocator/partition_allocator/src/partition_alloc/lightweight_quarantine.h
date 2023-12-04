// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Lightweight Quarantine (LQ) provides a low-cost quarantine mechanism with
// following characteristics.
//
// - Built on PartitionAlloc: only supports allocations in a known root
// - As fast as PA: LQ just defers `Free()` handling and may benefit from thread
//   cache etc.
// - Thread-safe
// - No allocation time information: triggered on `Free()`
// - Don't use quarantined objects' payload - available for zapping
// - Don't allocate heap memory.
// - Flexible to support several applications
//
// `LightweightQuarantineRoot` represents one quarantine system
// (e.g. scheduler loop quarantine).
// `LightweightQuarantineBranch` provides a quarantine request interface.
// It belongs to a `LightweightQuarantineRoot` and there can be multiple
// instances (e.g. one per thread). By having one branch per thread, it requires
// no lock for faster quarantine.
// ┌────────────────────────────┐
// │PartitionRoot               │
// └┬──────────────────────────┬┘
// ┌▽────────────────────────┐┌▽────────────────────┐
// │LQRoot 1                 ││LQRoot 2             │
// └┬───────────┬───────────┬┘└──────────────┬──┬──┬┘
// ┌▽─────────┐┌▽─────────┐┌▽─────────┐      ▽  ▽  ▽
// │LQBranch 1││LQBranch 2││LQBranch 3│
// └──────────┘└──────────┘└──────────┘

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_

#include <stdint.h>
#include <array>
#include <atomic>
#include <limits>
#include <type_traits>

#include "partition_alloc/partition_alloc_base/export_template.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_stats.h"

namespace partition_alloc {

struct PartitionRoot;
struct LightweightQuarantineStats;

namespace internal {

template <size_t QuarantineCapacityCount>
class LightweightQuarantineBranch;

class LightweightQuarantineRoot {
 public:
  explicit LightweightQuarantineRoot(PartitionRoot& allocator_root,
                                     size_t capacity_in_bytes = 0)
      : allocator_root_(allocator_root),
        capacity_in_bytes_(capacity_in_bytes) {}

  template <size_t QuarantineCapacityCount>
  LightweightQuarantineBranch<QuarantineCapacityCount> CreateBranch(
      bool lock_required = true) {
    return LightweightQuarantineBranch<QuarantineCapacityCount>(*this,
                                                                lock_required);
  }

  void AccumulateStats(LightweightQuarantineStats& stats) const {
    stats.count += count_.load(std::memory_order_relaxed);
    stats.size_in_bytes += size_in_bytes_.load(std::memory_order_relaxed);
    stats.cumulative_count += cumulative_count_.load(std::memory_order_relaxed);
    stats.cumulative_size_in_bytes +=
        cumulative_size_in_bytes_.load(std::memory_order_relaxed);
    stats.quarantine_miss_count +=
        quarantine_miss_count_.load(std::memory_order_relaxed);
  }

  size_t GetCapacityInBytes() const {
    return capacity_in_bytes_.load(std::memory_order_relaxed);
  }
  void SetCapacityInBytesForTesting(size_t capacity) {
    capacity_in_bytes_.store(capacity, std::memory_order_relaxed);
    // `size_in_bytes` may exceed `capacity_in_bytes` here.
    // Each branch will try to shrink their quarantine later.
  }

 private:
  PartitionRoot& allocator_root_;
  std::atomic_size_t capacity_in_bytes_;

  // Number of quarantined entries.
  std::atomic_size_t count_ = 0;
  // Total size of quarantined entries, capped by `capacity_in_bytes`.
  std::atomic_size_t size_in_bytes_ = 0;

  // Stats.
  std::atomic_size_t cumulative_count_ = 0;
  std::atomic_size_t cumulative_size_in_bytes_ = 0;
  std::atomic_size_t quarantine_miss_count_ = 0;

  template <size_t>
  friend class LightweightQuarantineBranch;
};

template <size_t QuarantineCapacityCount>
class LightweightQuarantineBranch {
 public:
  // `QuarantineCapacityCount` must be a positive number.
  static constexpr uint32_t kQuarantineCapacityCount = QuarantineCapacityCount;
  static_assert(0 < QuarantineCapacityCount);

  using Root = LightweightQuarantineRoot;

  LightweightQuarantineBranch(const LightweightQuarantineBranch&) = delete;
  LightweightQuarantineBranch(LightweightQuarantineBranch&& b)
      : root_(b.root_),
        lock_required_(b.lock_required_),
        slots_(std::move(b.slots_)),
        branch_count_(b.branch_count_),
        branch_size_in_bytes_(b.branch_size_in_bytes_) {}
  ~LightweightQuarantineBranch() { Purge(); }

  // Quarantines an object. This list holds information you put into `entry`
  // as much as possible.  If the object is too large, this may return
  // `false`, meaning that quarantine request has failed (and freed
  // immediately). Otherwise, returns `true`.
  bool Quarantine(void* object);

  // Dequarantine all entries **held by this branch**.
  // It is possible that another branch with entries and it remains untouched.
  void Purge() {
    ConditionalScopedGuard guard(lock_required_, lock_);
    PurgeInternal(0, 0);
  }

  // Determines this list contains an object.
  bool IsQuarantinedForTesting(void* object) {
    ConditionalScopedGuard guard(lock_required_, lock_);
    for (size_t i = 0; i < branch_count_; i++) {
      if (slots_[i] == object) {
        return true;
      }
    }
    return false;
  }

  Root& GetRoot() { return root_; }

 private:
  explicit LightweightQuarantineBranch(Root& root, bool lock_required)
      : root_(root), lock_required_(lock_required) {}

  // Try to dequarantine entries to satisfy below:
  //   branch_count_ <= target_branch_count
  //   && root_.size_in_bytes_ <=  target_size_in_bytes
  // It is possible that this branch cannot satisfy the
  // request as it has control over only what it has. If you need to ensure the
  // constraint, call `Purge()` for each branch in sequence, synchronously.
  void PurgeInternal(size_t target_branch_count, size_t target_size_in_bytes)
      PA_EXCLUSIVE_LOCKS_REQUIRED(lock_);

  Root& root_;

  bool lock_required_;
  Lock lock_;

  // An utility to lock only if a condition is met.
  class PA_SCOPED_LOCKABLE ConditionalScopedGuard {
   public:
    explicit ConditionalScopedGuard(bool condition, Lock& lock)
        PA_EXCLUSIVE_LOCK_FUNCTION(lock)
        : condition_(condition), lock_(lock) {
      if (condition_) {
        lock_.Acquire();
      }
    }
    ~ConditionalScopedGuard() PA_UNLOCK_FUNCTION() {
      if (condition_) {
        lock_.Release();
      }
    }

   private:
    const bool condition_;
    Lock& lock_;
  };

  // Non-cryptographic random number generator.
  // Thread-unsafe so guarded by `lock_`.
  base::InsecureRandomGenerator random_ PA_GUARDED_BY(lock_);

  // `slots_` hold an array of quarantined entries.
  // The contents of empty slots are undefined and reads should not occur.
  // First `branch_count_` slots are used and entries should be shuffled.
  std::array<void*, kQuarantineCapacityCount> slots_ PA_GUARDED_BY(lock_);

  // # of quarantined entries in this branch.
  size_t branch_count_ PA_GUARDED_BY(lock_) = 0;
  size_t branch_size_in_bytes_ PA_GUARDED_BY(lock_) = 0;

  friend class LightweightQuarantineRoot;
};

#define EXPORT_TEMPLATE                             \
  extern template class PA_EXPORT_TEMPLATE_DECLARE( \
      PA_COMPONENT_EXPORT(PARTITION_ALLOC))
using LightweightQuarantineBranchForTesting = LightweightQuarantineBranch<1024>;
using SchedulerLoopQuarantineRoot = LightweightQuarantineRoot;
using SchedulerLoopQuarantineBranch = LightweightQuarantineBranch<1024>;
EXPORT_TEMPLATE LightweightQuarantineBranch<1024>;
#undef EXPORT_TEMPLATE

}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_
