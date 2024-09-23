// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/gwp_asan_support.h"

#if PA_BUILDFLAG(ENABLE_GWP_ASAN_SUPPORT)

#include "partition_alloc/build_config.h"
#include "partition_alloc/freeslot_bitmap_constants.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"

namespace partition_alloc {

namespace {
PartitionOptions GwpAsanPartitionOptions() {
  PartitionOptions options;
  options.backup_ref_ptr = PartitionOptions::kEnabled;
  return options;
}
}  // namespace

// static
void* GwpAsanSupport::MapRegion(size_t slot_count,
                                std::vector<uint16_t>& free_list) {
  PA_CHECK(slot_count > 0);

  static internal::base::NoDestructor<PartitionRoot> root(
      GwpAsanPartitionOptions());

  const size_t kSlotSize = 2 * internal::SystemPageSize();
  uint16_t bucket_index = PartitionRoot::SizeToBucketIndex(
      kSlotSize, root->GetBucketDistribution());
  auto* bucket = root->buckets + bucket_index;

  const size_t kSuperPagePayloadStartOffset =
      internal::SuperPagePayloadStartOffset(
          /* is_managed_by_normal_buckets = */ true);
  PA_CHECK(kSuperPagePayloadStartOffset % kSlotSize == 0);
  const size_t kSuperPageGwpAsanSlotAreaBeginOffset =
      kSuperPagePayloadStartOffset;
  const size_t kSuperPageGwpAsanSlotAreaEndOffset =
      internal::SuperPagePayloadEndOffset();
  const size_t kSuperPageGwpAsanSlotAreaSize =
      kSuperPageGwpAsanSlotAreaEndOffset - kSuperPageGwpAsanSlotAreaBeginOffset;
  const size_t kSlotsPerSlotSpan = bucket->get_bytes_per_span() / kSlotSize;
  const size_t kSlotsPerSuperPage =
      kSuperPageGwpAsanSlotAreaSize / (kSlotsPerSlotSpan * kSlotSize);

  size_t super_page_count = 1 + ((slot_count - 1) / kSlotsPerSuperPage);
  PA_CHECK(super_page_count <=
           std::numeric_limits<size_t>::max() / kSuperPageSize);
  uintptr_t super_page_span_start;
  {
    internal::ScopedGuard locker{internal::PartitionRootLock(root.get())};
    super_page_span_start = bucket->AllocNewSuperPageSpanForGwpAsan(
        root.get(), super_page_count, AllocFlags::kNone);

    if (!super_page_span_start) {
      return nullptr;
    }

#if PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
    // Mapping the GWP-ASan region in to the lower 32-bits of address space
    // makes it much more likely that a bad pointer dereference points into
    // our region and triggers a false positive report. We rely on the fact
    // that PA address pools are never allocated in the first 4GB due to
    // their alignment requirements.
    PA_CHECK(super_page_span_start >= (1ULL << 32));
#endif  // PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)

    uintptr_t super_page_span_end =
        super_page_span_start + super_page_count * kSuperPageSize;
    PA_CHECK(super_page_span_start < super_page_span_end);

    for (uintptr_t super_page = super_page_span_start;
         super_page < super_page_span_end; super_page += kSuperPageSize) {
      auto* page_metadata =
          internal::PartitionSuperPageToMetadataArea(super_page);

      // Index 0 is invalid because it is the super page extent metadata.
      for (size_t partition_page_idx =
               1 + internal::NumPartitionPagesPerFreeSlotBitmap();
           partition_page_idx + bucket->get_pages_per_slot_span() <
           internal::NumPartitionPagesPerSuperPage();
           partition_page_idx += bucket->get_pages_per_slot_span()) {
        auto* slot_span_metadata =
            &page_metadata[partition_page_idx].slot_span_metadata;
        bucket->InitializeSlotSpanForGwpAsan(slot_span_metadata, root.get());
        auto slot_span_start =
            internal::SlotSpanMetadata<internal::MetadataKind::kReadOnly>::
                ToSlotSpanStart(slot_span_metadata);

        for (uintptr_t slot_idx = 0; slot_idx < kSlotsPerSlotSpan; ++slot_idx) {
          auto slot_start = slot_span_start + slot_idx * kSlotSize;
          PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(slot_start,
                                                                   kSlotSize)
              ->InitializeForGwpAsan();
          size_t global_slot_idx = (slot_start - super_page_span_start -
                                    kSuperPageGwpAsanSlotAreaBeginOffset) /
                                   kSlotSize;
          PA_DCHECK(global_slot_idx < std::numeric_limits<uint16_t>::max());
          free_list.push_back(global_slot_idx);
          if (free_list.size() == slot_count) {
            return reinterpret_cast<void*>(
                super_page_span_start + kSuperPageGwpAsanSlotAreaBeginOffset -
                internal::SystemPageSize());  // Depends on the PA guard region
                                              // in front of the super page
                                              // payload area.
          }
        }
      }
    }
  }

  PA_NOTREACHED();
}

// static
bool GwpAsanSupport::CanReuse(uintptr_t slot_start) {
  const size_t kSlotSize = 2 * internal::SystemPageSize();
  return PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(slot_start,
                                                                  kSlotSize)
      ->CanBeReusedByGwpAsan();
}

}  // namespace partition_alloc

#endif  // PA_BUILDFLAG(ENABLE_GWP_ASAN_SUPPORT)
