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
//   - TODO(crbug.com/1462223): Implement Miracle Object quarantine with LQ.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_

#include <stdint.h>
#include <array>
#include <atomic>
#include <limits>
#include <numeric>
#include <type_traits>

#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/bits.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/export_template.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/rand_util.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_alloc_base/thread_annotations.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_lock.h"

namespace partition_alloc {

struct PartitionRoot;
struct LightweightQuarantineStats;

namespace internal {

// `LightweightQuarantineEntry` represents one quarantine entry,
// with the original `Free()` request information.
struct LightweightQuarantineEntry {
  LightweightQuarantineEntry() = default;
  explicit LightweightQuarantineEntry(void* object) : object(object) {}
  PA_ALWAYS_INLINE void* GetObject() const { return object; }

  void* object = nullptr;
};

template <typename QuarantineEntry, size_t CapacityCount>
class LightweightQuarantineList {
 public:
  // `CapacityCount` must be power of two.
  static constexpr uint32_t kCapacityCount = CapacityCount;
  static_assert(base::bits::IsPowerOfTwo(kCapacityCount));

  // "Entry" is an object that holds free-time information, created for each
  // quarantined object.
  // An application may overwrite `QuarantineEntry` with their custom entry
  // to record more `Free()`-time information.
  using Entry = QuarantineEntry;
  // To be accessed from a crash handler, it must be a trivially copyable.
  static_assert(std::is_trivially_copyable_v<Entry>);

  // Entry ids are concatenation of "slot index" and "counter".
  // Their lower bits store "slot index", an index of `slots_`.
  // Their upper bits store "counter", which is incremented every time
  // when used (may overflow). It is used to verify the slot is occupied by that
  // entry.
  static constexpr uint32_t kSlotIndexMask = kCapacityCount - 1;
  static constexpr uint32_t kInvalidEntryID =
      std::numeric_limits<uint32_t>::max();

  // "Slot" is a place to put an entry. Each slot owns at most one entry.
  struct Slot {
    void* GetObject() const {
      // We assume `Entry` has `GetObject()` member function.
      return entry.GetObject();
    }

    // Used to make sure the metadata entry isn't stale.
    uint32_t entry_id = kInvalidEntryID;
    Entry entry;
  };
  static_assert(std::is_trivially_copyable_v<Slot>);

  explicit LightweightQuarantineList(PartitionRoot* root,
                                     size_t capacity_in_bytes = 0)
      : root_(root), capacity_in_bytes_(capacity_in_bytes) {
    PA_CHECK(root);
    // Initialize entry ids with iota.
    // They can be initialized with any value as long as
    // `entry_ids_[i] & kSlotIndexMask` are unique.
    std::iota(entry_ids_.begin(), entry_ids_.end(), 0);
  }
  LightweightQuarantineList(const LightweightQuarantineList&) = delete;
  ~LightweightQuarantineList() { Purge(); }

  // Quarantines an object. This list holds information you put into `Entry`
  // as much as possible.
  // If the object is too large, this may return `kInvalidEntryID`, meaning
  // that quarantine request has failed (and freed immediately).
  // Otherwise, returns an entry id for the quarantine.
  uint32_t Quarantine(Entry&& entry);

  void AccumulateStats(LightweightQuarantineStats& stats) const;

  // Determines this list contains an entry with `entry.GetObject() == ptr`.
  bool IsQuarantinedForTesting(void* object);

  // Dequarantine all entries.
  void Purge();

  // Returns a pointer to an array of `Slot`.
  // Don't try to dereference to avoid harmful races.
  // You can save this address and entry id returned by `Quarantine()`
  // somewhere, and use `GetEntryByID()` to obtain the free time information.
  // E.g. embed an entry id into zapping pattern and detect the pattern in
  // a crash handler to report the free time information.
  uintptr_t GetSlotsAddress() {
    ScopedGuard guard(lock_);
    return reinterpret_cast<uintptr_t>(slots_.data());
  }

  // Returns an `Entry` associated with the id.
  // May return `nullptr` if it is overwritten by another entry. This can rarely
  // return wrong entry if the id is colliding with another entry.
  // Not thread-safe, use only in crash handling or in tests.
  static const Entry* GetEntryByID(uintptr_t slots_address, uint32_t entry_id) {
    const auto* slots = reinterpret_cast<Slot*>(slots_address);
    const auto& slot = slots[entry_id & kSlotIndexMask];
    if (slot.entry_id != entry_id) {
      return nullptr;
    }
    return &slot.entry;
  }

  size_t GetCapacityInBytes() const {
    return capacity_in_bytes_.load(std::memory_order_relaxed);
  }
  void SetCapacityInBytesForTesting(size_t capacity_in_bytes) {
    capacity_in_bytes_.store(capacity_in_bytes, std::memory_order_relaxed);
    // Purge to maintain invariant.
    Purge();
  }

 private:
  Lock lock_;
  // Not `PA_GUARDED_BY` as they have another lock.
  PartitionRoot* const root_;
  std::atomic_size_t capacity_in_bytes_;

  // Non-cryptographic random number generator.
  // Thread-unsafe so guarded by `lock_`.
  base::InsecureRandomGenerator random_ PA_GUARDED_BY(lock_);

  // `slots_` hold an array of quarantined entries.
  // The contents of empty slots are undefined and reads should not occur.
  // There is no guarantee that non-empty slots will be placed consecutively.
  std::array<Slot, kCapacityCount> slots_ PA_GUARDED_BY(lock_);

  // Number of quarantined entries, capped by `kCapacityCount`.
  std::atomic_size_t count_ = 0;
  // Total size of quarantined entries, capped by `capacity_in_bytes_`.
  std::atomic_size_t size_in_bytes_ = 0;
  // `entry_ids_` is a supplementary data store to access slots quickly.
  // Its first `count_` elements represents quarantined entry ids and
  // used to choose an entry to dequarantine quickly.
  // The other elements reperent empty slot indices to find an empty slot to
  // fill in quickly. All elements are also responsible for managing upper bits
  // of entry ids so that they are as unique as possible.
  std::array<uint32_t, kCapacityCount> entry_ids_ PA_GUARDED_BY(lock_);

  // Stats.
  std::atomic_size_t cumulative_count_ = 0;
  std::atomic_size_t cumulative_size_in_bytes_ = 0;
  std::atomic_size_t quarantine_miss_count_ = 0;
};

using SchedulerLoopQuarantine =
    LightweightQuarantineList<LightweightQuarantineEntry, 1024>;
extern template class PA_EXPORT_TEMPLATE_DECLARE(
    PA_COMPONENT_EXPORT(PARTITION_ALLOC))
    LightweightQuarantineList<LightweightQuarantineEntry, 1024>;

}  // namespace internal

}  // namespace partition_alloc

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_LIGHTWEIGHT_QUARANTINE_H_
