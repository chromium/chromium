// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// clang-format off
#include "partition_alloc/internal/partition_root_internal.h"
// clang-format on

#include <atomic>
#include <cstdint>
#include <unordered_map>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/internal_allocator.h"
#include "partition_alloc/partition_lock.h"

namespace partition_alloc {

namespace {

std::atomic<size_t> g_untyped_intended_leak_size{0};

template <typename T, typename U>
using InternalUnorderedMap =
    std::unordered_map<T,
                       U,
                       std::hash<T>,
                       std::equal_to<T>,
                       internal::InternalAllocator<std::pair<const T, U>>>;

InternalUnorderedMap<uint32_t, size_t>& GetLeakSizePerTypeIdMap() {
  static internal::base::NoDestructor<InternalUnorderedMap<uint32_t, size_t>>
      s_map;
  return *s_map;
}

}  // namespace

// static
void PartitionRoot::Zap(SlotStart slot_start,
                        SlotSpanMetadata* slot_span,
                        uint32_t type_id) {
  void* object = reinterpret_cast<void*>(slot_start.value());

  uint64_t zap_value = internal::kIntendedLeakQuarantineMarker |
                       (static_cast<uint64_t>(type_id) << 8);

  size_t slot_size = slot_span->GetUtilizedSlotSize();
  size_t count = slot_size / sizeof(uint64_t);
  std::fill_n(static_cast<uint64_t*>(object), count, zap_value);

  size_t remainder_offset = sizeof(uint64_t) * count;
  size_t remainder_size = slot_size - remainder_offset;

  std::fill_n(PA_UNSAFE_TODO(static_cast<uint8_t*>(object) + remainder_offset),
              remainder_size, internal::kIntendedLeakQuarantineRemainder);
}

// static
void PartitionRoot::RecordLeakSizePerTypeId(uint32_t type_id,
                                            size_t slot_size) {
  if (type_id == internal::kIntendedLeakUnknownTypeId) {
    g_untyped_intended_leak_size.fetch_add(slot_size,
                                           std::memory_order_relaxed);
    return;
  }

  internal::ScopedGuard guard(PartitionRoot::GetLeakSizeMapLock());

  auto& map = GetLeakSizePerTypeIdMap();
  auto it = map.find(type_id);
  if (it != map.end()) {
    it->second += slot_size;
  } else {
    map.insert(std::make_pair(type_id, slot_size));
  }
}

size_t PartitionRoot::GetSlotSizeFromRequestedSizeForTesting(
    size_t requested_size) const {
  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  PA_CHECK(raw_size >= requested_size);  // check for overflows

  // We should avoid calling `GetBucketDistribution()` repeatedly in the
  // same function, since the bucket distribution can change underneath
  // us. If we pass this changed value to `SizeToBucketIndex()` in the
  // same allocation request, we'll get inconsistent state.
  uint16_t bucket_index =
      SizeToBucketIndex(raw_size, this->GetBucketDistribution());
  return PA_UNSAFE_TODO(buckets_[bucket_index].slot_size);
}

// static
void PartitionRoot::DumpIntendedLeakStats(PartitionStatsDumper* dumper) {
  size_t untyped_size =
      g_untyped_intended_leak_size.load(std::memory_order_relaxed);
  if (untyped_size > 0) {
    dumper->DumpIntendedLeak(internal::kIntendedLeakUnknownTypeId,
                             untyped_size);
  }

  internal::ScopedGuard guard(PartitionRoot::GetLeakSizeMapLock());

  auto& map = GetLeakSizePerTypeIdMap();
  for (auto entry : map) {
    dumper->DumpIntendedLeak(entry.first, entry.second);
  }
}

void PartitionRoot::ClearIntendedLeakStatsForTesting() {
  g_untyped_intended_leak_size.store(0, std::memory_order_relaxed);
  internal::ScopedGuard guard(PartitionRoot::GetLeakSizeMapLock());
  GetLeakSizePerTypeIdMap().clear();
}

}  // namespace partition_alloc
