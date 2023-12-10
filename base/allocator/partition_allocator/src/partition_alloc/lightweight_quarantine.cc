// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/lightweight_quarantine.h"

#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc::internal {

namespace {
size_t GetObjectSize(void* object) {
  const auto* entry_slot_span = SlotSpanMetadata::FromObject(object);
  return entry_slot_span->GetUtilizedSlotSize();
}
}  // namespace

template <size_t QuarantineCapacityCount>
bool LightweightQuarantineBranch<QuarantineCapacityCount>::Quarantine(
    void* object) {
  const auto entry_size = GetObjectSize(object);

  const size_t capacity_in_bytes =
      root_.capacity_in_bytes_.load(std::memory_order_relaxed);

  {
    ConditionalScopedGuard guard(lock_required_, lock_);

    const size_t size_in_bytes_held_by_others =
        root_.size_in_bytes_.load(std::memory_order_relaxed) -
        branch_size_in_bytes_;
    if (capacity_in_bytes < size_in_bytes_held_by_others + entry_size) {
      // Even if this branch dequarantines all entries held by it, this entry
      // cannot fit within the capacity.
      root_.allocator_root_.template Free<FreeFlags::kNoHooks>(object);
      root_.quarantine_miss_count_.fetch_add(1u, std::memory_order_relaxed);
      return false;
    }

    // Dequarantine some entries as required.
    PurgeInternal(kQuarantineCapacityCount - 1, capacity_in_bytes - entry_size);

    // Update stats (locked).
    branch_count_++;
    PA_DCHECK(branch_count_ <= kQuarantineCapacityCount);
    branch_size_in_bytes_ += entry_size;
    PA_DCHECK(branch_size_in_bytes_ <= capacity_in_bytes);

    slots_[branch_count_ - 1] = object;

    // Swap randomly so that the quarantine list remain shuffled.
    // This is not uniformly random, but sufficiently random.
    const size_t random_index = random_.RandUint32() % branch_count_;
    std::swap(slots_[random_index], slots_[branch_count_ - 1]);
  }

  // Update stats (not locked).
  root_.count_.fetch_add(1, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_add(entry_size, std::memory_order_relaxed);
  root_.cumulative_count_.fetch_add(1, std::memory_order_relaxed);
  root_.cumulative_size_in_bytes_.fetch_add(entry_size,
                                            std::memory_order_relaxed);
  return true;
}

template <size_t QuarantineCapacityCount>
void LightweightQuarantineBranch<QuarantineCapacityCount>::PurgeInternal(
    size_t target_branch_count,
    size_t target_size_in_bytes) {
  size_t size_in_bytes = root_.size_in_bytes_.load(std::memory_order_acquire);
  int64_t freed_count = 0;
  int64_t freed_size_in_bytes = 0;

  // Dequarantine some entries as required.
  while (branch_count_ && (target_branch_count < branch_count_ ||
                           target_size_in_bytes < size_in_bytes)) {
    // As quarantined entries are shuffled, picking last entry is equivalent
    // to picking random entry.
    void* to_free = slots_[branch_count_ - 1];
    size_t to_free_size = GetObjectSize(to_free);

    PA_DCHECK(to_free);
    // We don't guarantee the deferred `Free()` has the same `FreeFlags`.
    root_.allocator_root_.template Free<FreeFlags::kNoHooks>(to_free);

    freed_count++;
    freed_size_in_bytes += to_free_size;

    branch_count_--;
    size_in_bytes -= to_free_size;
  }

  branch_size_in_bytes_ -= freed_size_in_bytes;
  root_.count_.fetch_sub(freed_count, std::memory_order_relaxed);
  root_.size_in_bytes_.fetch_sub(freed_size_in_bytes,
                                 std::memory_order_release);
}

#define EXPORT_TEMPLATE \
  template class PA_EXPORT_TEMPLATE_DEFINE(PA_COMPONENT_EXPORT(PARTITION_ALLOC))
EXPORT_TEMPLATE LightweightQuarantineBranch<1024>;
#undef EXPORT_TEMPLATE

}  // namespace partition_alloc::internal
