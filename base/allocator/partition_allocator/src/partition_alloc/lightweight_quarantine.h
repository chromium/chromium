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

#include <array>
#include <atomic>
#include <cstdint>
#include <limits>
#include <memory>
#include <type_traits>
#include <vector>

#include "partition_alloc/internal_allocator_forward.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_stats.h"

namespace partition_alloc {

struct PartitionRoot;
struct LightweightQuarantineStats;

namespace internal {

class LightweightQuarantineBranch;

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) LightweightQuarantineRoot {
 public:
  explicit LightweightQuarantineRoot(PartitionRoot& allocator_root,
                                     size_t capacity_in_bytes = 0)
      : allocator_root_(allocator_root),
        capacity_in_bytes_(capacity_in_bytes) {}

  LightweightQuarantineBranch CreateBranch(bool lock_required = true);

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
  void SetCapacityInBytes(size_t capacity) {
    capacity_in_bytes_.store(capacity, std::memory_order_relaxed);
    // `size_in_bytes` may exceed `capacity_in_bytes` here.
    // Each branch will try to shrink their quarantine later.
  }

 private:
  PartitionRoot& allocator_root_;
  std::atomic_size_t capacity_in_bytes_;
  // Total size of quarantined entries, capped by `capacity_in_bytes`.
  std::atomic_size_t size_in_bytes_ = 0;

  // Stats.
  std::atomic_size_t count_ = 0;  // Number of quarantined entries
  std::atomic_size_t cumulative_count_ = 0;
  std::atomic_size_t cumulative_size_in_bytes_ = 0;
  std::atomic_size_t quarantine_miss_count_ = 0;

  friend class LightweightQuarantineBranch;
};

class PA_COMPONENT_EXPORT(PARTITION_ALLOC) LightweightQuarantineBranch {
 public:
  using Root = LightweightQuarantineRoot;

  LightweightQuarantineBranch(const LightweightQuarantineBranch&) = delete;
  LightweightQuarantineBranch(LightweightQuarantineBranch&& b);
  ~LightweightQuarantineBranch();

  // Quarantines an object. This list holds information you put into `entry`
  // as much as possible.  If the object is too large, this may return
  // `false`, meaning that quarantine request has failed (and freed
  // immediately). Otherwise, returns `true`.
  bool Quarantine(void* object,
                  SlotSpanMetadata* slot_span,
                  uintptr_t slot_start);

  // Dequarantine all entries **held by this branch**.
  // It is possible that another branch with entries and it remains untouched.
  void Purge() {
    ConditionalScopedGuard guard(lock_required_, lock_);
    PurgeInternal(0);
  }

  // Determines this list contains an object.
  bool IsQuarantinedForTesting(void* object);

  Root& GetRoot() { return root_; }

 private:
  LightweightQuarantineBranch(Root& root, bool lock_required);

  // Try to dequarantine entries to satisfy below:
  //   root_.size_in_bytes_ <=  target_size_in_bytes
  // It is possible that this branch cannot satisfy the
  // request as it has control over only what it has. If you need to ensure the
  // constraint, call `Purge()` for each branch in sequence, synchronously.
  void PurgeInternal(size_t target_size_in_bytes)
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

  // `slots_` hold quarantined entries.
  struct QuarantineSlot {
    uintptr_t slot_start;
    size_t usable_size;
  };
  std::vector<QuarantineSlot, InternalAllocator<QuarantineSlot>> slots_
      PA_GUARDED_BY(lock_);
  size_t branch_size_in_bytes_ PA_GUARDED_BY(lock_) = 0;

  friend class LightweightQuarantineRoot;
};

}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_
