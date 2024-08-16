// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_
#define PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_

#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_page.h"

namespace partition_alloc::internal {

struct ReadOnlyPartitionDirectMapExtent;
struct WritablePartitionDirectMapExtent;
struct WritablePartitionDirectMapMetadata;

template <const MetadataKind kind>
struct PartitionDirectMapExtent {
  using ReadOnlyType = ReadOnlyPartitionDirectMapExtent;
  using WritableType = WritablePartitionDirectMapExtent;

  MaybeConstT<kind, ReadOnlyPartitionDirectMapExtent*> next_extent;
  MaybeConstT<kind, ReadOnlyPartitionDirectMapExtent*> prev_extent;
  MaybeConstT<kind, const PartitionBucket*> bucket;

  // Size of the entire reservation, including guard pages, meta-data,
  // padding for alignment before allocation, and padding for granularity at the
  // end of the allocation.
  MaybeConstT<kind, size_t> reservation_size;

  // Padding between the first partition page (guard pages + meta-data) and
  // the allocation.
  MaybeConstT<kind, size_t> padding_for_alignment;
};

struct ReadOnlyPartitionDirectMapExtent
    : public PartitionDirectMapExtent<MetadataKind::kReadOnly> {
  PA_ALWAYS_INLINE static ReadOnlyPartitionDirectMapExtent*
  FromSlotSpanMetadata(SlotSpanMetadata* slot_span);

  PA_ALWAYS_INLINE WritablePartitionDirectMapExtent* ToWritable(
      const PartitionRoot* root);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapExtent* ToReadOnly();
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

 private:
  // In order to resolve circular dependencies, i.e. ToWritable() needs
  // PartitionRoot, define template method: ToWritableInternal() and
  // ToWritable() uses it.
  template <typename T>
  WritablePartitionDirectMapExtent* ToWritableInternal(
      [[maybe_unused]] T* root);
};

struct WritablePartitionDirectMapExtent
    : public PartitionDirectMapExtent<MetadataKind::kWritable> {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapExtent* ToReadOnly(
      const PartitionRoot* root);

 private:
  // In order to resolve circular dependencies, i.e. ToReadOnly() needs
  // PartitionRoot, define template method: ToReadOnlyInternal() and
  // ToReadOnly() uses it.
  template <typename T>
  ReadOnlyPartitionDirectMapExtent* ToReadOnlyInternal(
      [[maybe_unused]] T* root);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
};

struct ReadOnlyPartitionDirectMapMetadata;
struct WritablePartitionDirectMapMetadata;

// Metadata page for direct-mapped allocations.
template <const MetadataKind kind>
struct PartitionDirectMapMetadata {
  // |page_metadata| and |second_page_metadata| are needed to match the
  // layout of normal buckets (specifically, of single-slot slot spans), with
  // the caveat that only the first subsequent page is needed (for
  // SubsequentPageMetadata) and others aren't used for direct map.
  // TODO(crbug.com/40238514): Will be ReadOnlyPartitionPageMetadata.
  MaybeConstT<kind, PartitionPageMetadata> page_metadata;
  MaybeConstT<kind, PartitionPageMetadata> second_page_metadata;

  // The following fields are metadata specific to direct map allocations. All
  // these fields will easily fit into the precalculated metadata region,
  // because a direct map allocation starts no further than half way through the
  // super page.
  MaybeConstT<kind, PartitionBucket> bucket;

  std::conditional_t<kind == MetadataKind::kReadOnly,
                     ReadOnlyPartitionDirectMapExtent,
                     WritablePartitionDirectMapExtent>
      direct_map_extent;
};

struct ReadOnlyPartitionDirectMapMetadata
    : public PartitionDirectMapMetadata<MetadataKind::kReadOnly> {
  PA_ALWAYS_INLINE static ReadOnlyPartitionDirectMapMetadata*
  FromSlotSpanMetadata(SlotSpanMetadata* slot_span);

  PA_ALWAYS_INLINE WritablePartitionDirectMapMetadata* ToWritable(
      const PartitionRoot* root);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapMetadata* ToReadOnly();
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

 private:
  // In order to resolve circular dependencies, i.e. ToWritable() needs
  // PartitionRoot, define template method: ToWritableInternal() and
  // ToWritable() uses it.
  template <typename T>
  WritablePartitionDirectMapMetadata* ToWritableInternal(
      [[maybe_unused]] T* root);
};

struct WritablePartitionDirectMapMetadata
    : public PartitionDirectMapMetadata<MetadataKind::kWritable> {
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
  PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapMetadata* ToReadOnly(
      const PartitionRoot* root);

 private:
  // In order to resolve circular dependencies, i.e. ToReadOnly() needs
  // PartitionRoot, define template method: ToReadOnlyInternal() and
  // ToReadOnly() uses it.
  template <typename T>
  ReadOnlyPartitionDirectMapMetadata* ToReadOnlyInternal(
      [[maybe_unused]] T* root);
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)
};

PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapMetadata*
ReadOnlyPartitionDirectMapMetadata::FromSlotSpanMetadata(
    SlotSpanMetadata* slot_span) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  // |*slot_span| is the first field of |PartitionDirectMapMetadata|, just cast.
  auto* metadata =
      reinterpret_cast<ReadOnlyPartitionDirectMapMetadata*>(slot_span);
  PA_DCHECK(&metadata->page_metadata.slot_span_metadata == slot_span);
  return metadata;
}

PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapExtent*
ReadOnlyPartitionDirectMapExtent::FromSlotSpanMetadata(
    SlotSpanMetadata* slot_span) {
  PA_DCHECK(slot_span->bucket->is_direct_mapped());
  return &ReadOnlyPartitionDirectMapMetadata::FromSlotSpanMetadata(slot_span)
              ->direct_map_extent;
}

PA_ALWAYS_INLINE WritablePartitionDirectMapMetadata*
ReadOnlyPartitionDirectMapMetadata::ToWritable(const PartitionRoot* root) {
  return ToWritableInternal(root);
}

template <typename T>
WritablePartitionDirectMapMetadata*
ReadOnlyPartitionDirectMapMetadata::ToWritableInternal(
    [[maybe_unused]] T* root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  return reinterpret_cast<WritablePartitionDirectMapMetadata*>(
      reinterpret_cast<intptr_t>(this) + root->ShadowPoolOffset());
#else
  return reinterpret_cast<WritablePartitionDirectMapMetadata*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
}

PA_ALWAYS_INLINE WritablePartitionDirectMapExtent*
ReadOnlyPartitionDirectMapExtent::ToWritable(const PartitionRoot* root) {
  return ToWritableInternal(root);
}

template <typename T>
WritablePartitionDirectMapExtent*
ReadOnlyPartitionDirectMapExtent::ToWritableInternal([[maybe_unused]] T* root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  return reinterpret_cast<WritablePartitionDirectMapExtent*>(
      reinterpret_cast<intptr_t>(this) + root->ShadowPoolOffset());
#else
  return reinterpret_cast<WritablePartitionDirectMapExtent*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
}

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapMetadata*
ReadOnlyPartitionDirectMapMetadata::ToReadOnly() {
  return this;
}

PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapMetadata*
WritablePartitionDirectMapMetadata::ToReadOnly(const PartitionRoot* root) {
  return ToReadOnlyInternal(root);
}

template <typename T>
ReadOnlyPartitionDirectMapMetadata*
WritablePartitionDirectMapMetadata::ToReadOnlyInternal(
    [[maybe_unused]] T* root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  return reinterpret_cast<ReadOnlyPartitionDirectMapMetadata*>(
      reinterpret_cast<intptr_t>(this) - root->ShadowPoolOffset());
#else
  // must be no-op.
  return reinterpret_cast<ReadOnlyPartitionDirectMapMetadata*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
}

PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapExtent*
ReadOnlyPartitionDirectMapExtent::ToReadOnly() {
  return this;
}

PA_ALWAYS_INLINE ReadOnlyPartitionDirectMapExtent*
WritablePartitionDirectMapExtent::ToReadOnly(const PartitionRoot* root) {
  return ToReadOnlyInternal(root);
}

template <typename T>
ReadOnlyPartitionDirectMapExtent*
WritablePartitionDirectMapExtent::ToReadOnlyInternal([[maybe_unused]] T* root) {
#if PA_CONFIG(ENABLE_SHADOW_METADATA)
  return reinterpret_cast<ReadOnlyPartitionDirectMapExtent*>(
      reinterpret_cast<intptr_t>(this) - root->ShadowPoolOffset());
#else
  return reinterpret_cast<ReadOnlyPartitionDirectMapExtent*>(this);
#endif  // PA_CONFIG(ENABLE_SHADOW_METADATA)
}
#endif  // PA_BUILDFLAG(DCHECKS_ARE_ON)

}  // namespace partition_alloc::internal

#endif  // PARTITION_ALLOC_PARTITION_DIRECT_MAP_EXTENT_H_
