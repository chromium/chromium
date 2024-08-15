// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
#define PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_

#include <cstdint>

#include "partition_alloc/address_pool_manager.h"
#include "partition_alloc/address_pool_manager_types.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_dcheck_helper.h"
#include "partition_alloc/reservation_offset_table.h"

// Should not include partition_root.h, partition_bucket.h, partition_page.h.

namespace partition_alloc::internal {

struct WritablePartitionSuperPageExtentEntry;
struct ReadOnlyPartitionSuperPageExtentEntry;

// An "extent" is a span of consecutive superpages. We link the partition's next
// extent (if there is one) to the very start of a superpage's metadata area.
template <const MetadataKind kind>
struct PartitionSuperPageExtentEntry {
  // The data member of PartitionSuperPageExtentEntry. To make ReadOnly- and
  // WritablePartitionSuperPageExtentEntry have the same data member and the
  // same memory layout, all the data member are put into this struct. ReadOnly-
  // and WritablePartitionSuperPageExtentEntry extend this class without adding
  // any data members.
  MaybeConstT<kind, PartitionRoot*> root;
  MaybeConstT<kind, ReadOnlyPartitionSuperPageExtentEntry*> next;
  MaybeConstT<kind, uint16_t> number_of_consecutive_super_pages;
  MaybeConstT<kind, uint16_t> number_of_nonempty_slot_spans;
};

struct ReadOnlyPartitionSuperPageExtentEntry
    : public PartitionSuperPageExtentEntry<MetadataKind::kReadOnly> {
  WritablePartitionSuperPageExtentEntry* ToWritable(
      const PartitionRoot* partition_root) {
    return ToWritableInternal(root);
  }

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  ReadOnlyPartitionSuperPageExtentEntry* ToReadOnly() { return this; }
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

 private:
  // In order to resolve circular dependencies, i.e. ToWritable() needs
  // PartitionRoot, define template method: ToWritableInternal() here and
  // ToWritable() uses it.
  template <typename T>
  WritablePartitionSuperPageExtentEntry* ToWritableInternal(
      [[maybe_unused]] T* partition_root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    return reinterpret_cast<WritablePartitionSuperPageExtentEntry*>(
        reinterpret_cast<intptr_t>(this) + partition_root->ShadowPoolOffset());
#else
    return reinterpret_cast<WritablePartitionSuperPageExtentEntry*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
  }
};

struct WritablePartitionSuperPageExtentEntry
    : public PartitionSuperPageExtentEntry<MetadataKind::kWritable> {
  PA_ALWAYS_INLINE void IncrementNumberOfNonemptySlotSpans() {
    DCheckNumberOfPartitionPagesInSuperPagePayload(
        this, root, number_of_nonempty_slot_spans);
    ++number_of_nonempty_slot_spans;
  }

  PA_ALWAYS_INLINE void DecrementNumberOfNonemptySlotSpans() {
    PA_DCHECK(number_of_nonempty_slot_spans);
    --number_of_nonempty_slot_spans;
  }

#if !PA_CONFIG(ENABLE_SHADOW_METADATA)
  WritablePartitionSuperPageExtentEntry* ToWritable() { return this; }
#endif  // !PA_CONFIG(ENABLE_SHADOW_METADATA)

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  ReadOnlyPartitionSuperPageExtentEntry* ToReadOnly(
      const PartitionRoot* partition_root) {
    return ToReadOnlyInternal(partition_root);
  }

 private:
  // In order to resolve circular dependencies, i.e. ToReadOnly() needs
  // PartitionRoot, define template method: ToReadOnlyInternal() and
  // ToReadOnly() uses it.
  template <typename T>
  ReadOnlyPartitionSuperPageExtentEntry* ToReadOnlyInternal(
      [[maybe_unused]] T* partition_root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
    return reinterpret_cast<ReadOnlyPartitionSuperPageExtentEntry*>(
        reinterpret_cast<intptr_t>(this) - partition_root->ShadowPoolOffset());
#else
    return reinterpret_cast<ReadOnlyPartitionSuperPageExtentEntry*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
  }
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
};

static_assert(sizeof(ReadOnlyPartitionSuperPageExtentEntry) ==
                  sizeof(WritablePartitionSuperPageExtentEntry),
              "ReadOnlyPartitionSuperPageExtentEntry and "
              "WritablePartitionSuperPageExtentEntry must have the same size");
static_assert(
    sizeof(ReadOnlyPartitionSuperPageExtentEntry) <= kPageMetadataSize,
    "PartitionSuperPageExtentEntry must be able to fit in a metadata slot");
static_assert(kMaxSuperPagesInPool / kSuperPageSize <=
                  std::numeric_limits<
                      decltype(ReadOnlyPartitionSuperPageExtentEntry ::
                                   number_of_consecutive_super_pages)>::max(),
              "number_of_consecutive_super_pages must be big enough");

// Returns the base of the first super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesBeginFromExtent(const ReadOnlyPartitionSuperPageExtentEntry* extent) {
  PA_DCHECK(0 < extent->number_of_consecutive_super_pages);
  uintptr_t extent_as_uintptr = reinterpret_cast<uintptr_t>(extent);
  PA_DCHECK(IsManagedByNormalBuckets(extent_as_uintptr));
  return base::bits::AlignDown(extent_as_uintptr, kSuperPageAlignment);
}

// Returns the end of the last super page in the range of consecutive super
// pages.
//
// CAUTION! |extent| must point to the extent of the first super page in the
// range of consecutive super pages.
PA_ALWAYS_INLINE uintptr_t
SuperPagesEndFromExtent(const ReadOnlyPartitionSuperPageExtentEntry* extent) {
  return SuperPagesBeginFromExtent(extent) +
         (extent->number_of_consecutive_super_pages * kSuperPageSize);
}

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_SUPERPAGE_EXTENT_ENTRY_H_
