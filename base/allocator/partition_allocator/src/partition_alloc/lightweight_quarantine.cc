// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/src/partition_alloc/lightweight_quarantine.h"

#include "base/allocator/partition_allocator/src/partition_alloc/partition_page.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_root.h"
#include "base/allocator/partition_allocator/src/partition_alloc/partition_stats.h"

namespace partition_alloc::internal {

namespace {
size_t GetObjectSize(void* object) {
  const auto* entry_slot_span = SlotSpanMetadata::FromObject(object);
  return entry_slot_span->GetUtilizedSlotSize();
}
}  // namespace

template <typename QuarantineEntry, size_t CapacityCount>
uint32_t LightweightQuarantineList<QuarantineEntry, CapacityCount>::Quarantine(
    QuarantineEntry&& entry) {
  const auto entry_size = GetObjectSize(entry.GetObject());

  const size_t capacity_in_bytes =
      capacity_in_bytes_.load(std::memory_order_relaxed);
  if (capacity_in_bytes < entry_size) {
    // Even this single entry does not fit within the capacity.
    root_->Free(entry.GetObject());
    quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
    return kInvalidEntryID;
  }

  size_t entry_id;
  {
    // It may be possible to narrow down the locked section, but we will not
    // make any detailed adjustments for now, as we aim to create a lock-free
    // implementation by having a thread-local list.
    ScopedGuard guard(lock_);

    // Dequarantine some entries as required.
    size_t count = count_.load(std::memory_order_acquire);
    size_t size_in_bytes = size_in_bytes_.load(std::memory_order_acquire);
    while (kCapacityCount < count + 1 ||
           capacity_in_bytes < size_in_bytes + entry_size) {
      PA_DCHECK(0 < count);
      // As quarantined entries are shuffled, picking last entry is equivalent
      // to picking random entry.
      void* to_free =
          slots_[entry_ids_[count - 1] & kSlotIndexMask].GetObject();
      size_t to_free_size = GetObjectSize(to_free);

      PA_DCHECK(to_free);
      // We don't guarantee the deferred `Free()` has the same `FreeFlags`.
      root_->Free<FreeFlags::kNoHooks>(to_free);

      // Increment the counter embedded in the entry id.
      // This helps to identify the entry associated with this slot.
      entry_ids_[count - 1] += kCapacityCount;
      if (PA_UNLIKELY(entry_ids_[count - 1] == kInvalidEntryID)) {
        // Increment again so that it does not collide with the invalid id.
        entry_ids_[count - 1] += kCapacityCount;
      }

      count--;
      size_in_bytes -= to_free_size;
      // Contents of `slots_[...]` remains  as is, to keep the free-time
      // information as much as possible.
    }

    // Obtain an entry id.
    PA_DCHECK(count < kCapacityCount);
    entry_id = entry_ids_[count];
    count++;
    size_in_bytes += entry_size;

    // Update stats (locked).
    count_.store(count, std::memory_order_release);
    size_in_bytes_.store(size_in_bytes, std::memory_order_release);

    // Swap randomly so that the quarantine indices remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % count;
    std::swap(entry_ids_[random_index], entry_ids_[count - 1]);

    auto& slot = slots_[entry_id & kSlotIndexMask];
    slot.entry_id = entry_id;
    slot.entry = std::move(entry);
  }

  // Update stats (not locked).
  cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  cumulative_size_in_bytes_.fetch_add(entry_size, std::memory_order_relaxed);
  return entry_id;
}

template <typename QuarantineEntry, size_t CapacityCount>
void LightweightQuarantineList<QuarantineEntry, CapacityCount>::AccumulateStats(
    LightweightQuarantineStats& stats) const {
  stats.count += count_.load(std::memory_order_relaxed);
  stats.size_in_bytes += size_in_bytes_.load(std::memory_order_relaxed);
  stats.cumulative_count += cumulative_count_.load(std::memory_order_relaxed);
  stats.cumulative_size_in_bytes +=
      cumulative_size_in_bytes_.load(std::memory_order_relaxed);
  stats.quarantine_miss_count +=
      quarantine_miss_count_.load(std::memory_order_relaxed);
}

template <typename QuarantineEntry, size_t CapacityCount>
bool LightweightQuarantineList<QuarantineEntry, CapacityCount>::
    IsQuarantinedForTesting(void* object) {
  ScopedGuard guard(lock_);
  for (size_t i = 0; i < count_; i++) {
    if (slots_[entry_ids_[i] & kSlotIndexMask].GetObject() == object) {
      return true;
    }
  }
  return false;
}

template <typename QuarantineEntry, size_t CapacityCount>
void LightweightQuarantineList<QuarantineEntry, CapacityCount>::Purge() {
  ScopedGuard guard(lock_);

  size_t count = count_.load(std::memory_order_acquire);
  while (0 < count) {
    void* to_free = slots_[entry_ids_[count - 1] & kSlotIndexMask].GetObject();
    PA_DCHECK(to_free);
    root_->Free<FreeFlags::kNoHooks>(to_free);
    count--;
  }
  count_.store(0, std::memory_order_release);
  size_in_bytes_.store(0, std::memory_order_release);
  std::iota(entry_ids_.begin(), entry_ids_.end(), 0);
}

template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
    LightweightQuarantineList<LightweightQuarantineEntry, 1024>;

}  // namespace partition_alloc::internal
