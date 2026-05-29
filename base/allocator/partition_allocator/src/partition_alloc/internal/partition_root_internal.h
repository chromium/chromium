// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_H_
#define PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_H_

#include <cstring>

#include "partition_alloc/address_pool_manager_types.h"
#include "partition_alloc/allocation_guard.h"
#include "partition_alloc/bucket_lookup.h"
#include "partition_alloc/in_slot_metadata.h"
#include "partition_alloc/internal/partition_page_internal.h"
#include "partition_alloc/internal/reservation_offset_table_internal.h"
#include "partition_alloc/internal/thread_cache_internal.h"
#include "partition_alloc/partition_address_space.h"
#include "partition_alloc/partition_alloc-inl.h"
#include "partition_alloc/partition_alloc_allocation_data.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/algorithm.h"
#include "partition_alloc/partition_alloc_base/cxx_wrapper/optional.h"
#include "partition_alloc/partition_alloc_base/export_template.h"
#include "partition_alloc/partition_alloc_base/no_destructor.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_hooks.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_cookie.h"
#include "partition_alloc/partition_dcheck_helper.h"
#include "partition_alloc/partition_direct_map_extent.h"
#include "partition_alloc/partition_freelist_entry.h"
#include "partition_alloc/partition_oom.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/slot_start.h"
#include "partition_alloc/tagging.h"
#include "partition_alloc/thread_isolation/thread_isolation.h"

// When a memory tool is replacing malloc to keep aligned behaviour working we
// use window's aligned_malloc and aligned_free, but otherwise we need memalign.
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif  // PA_BUILDFLAG(PA_COMPILER_MSVC)
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc {

namespace internal {

// We want this size to be big enough that we have time to start up other
// scripts _before_ we wrap around.
static constexpr size_t kAllocInfoSize = 1 << 24;

struct AllocInfo {
  std::atomic<size_t> index{0};
  struct {
    uintptr_t addr;
    size_t size;
  } allocs[kAllocInfoSize] = {};
};

// Represents the detailed size information for a given requested allocation
// size.
struct BucketSizeDetails {
  uint16_t bucket_index;
  size_t slot_size;
};

#if PA_BUILDFLAG(RECORD_ALLOC_INFO)
extern AllocInfo g_allocs;

void RecordAllocOrFree(uintptr_t addr, size_t size);
#endif  // PA_BUILDFLAG(RECORD_ALLOC_INFO)

// Avoid including partition_address_space.h from this .h file, by moving the
// call to IsManagedByPartitionAllocBRPPool into the .cc file.
#if PA_BUILDFLAG(DCHECKS_ARE_ON)
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address);
#else
PA_ALWAYS_INLINE void DCheckIfManagedByPartitionAllocBRPPool(
    uintptr_t address) {}
#endif
}  // namespace internal

// AllocInternal exposed for testing.
template <AllocFlags flags>
PA_NOINLINE PA_MALLOC_FN void* PartitionRoot::AllocInternalForTesting(
    size_t requested_size,
    size_t slot_span_alignment,
    const char* type_name) {
  return AllocInternal<flags>(requested_size, slot_span_alignment, type_name);
}

PA_ALWAYS_INLINE size_t
PartitionRoot::GetSlotUsableSize(const SlotSpanMetadata* slot_span) {
  return AdjustSizeForExtrasSubtract(slot_span->GetUtilizedSlotSize());
}

PA_ALWAYS_INLINE PartitionRoot::BucketDistribution
PartitionRoot::GetBucketDistribution() const {
  return settings_.bucket_distribution;
}

PA_ALWAYS_INLINE size_t
PartitionRoot::get_total_size_of_committed_pages() const {
  return total_size_of_committed_pages_.load(std::memory_order_relaxed);
}
PA_ALWAYS_INLINE size_t PartitionRoot::get_max_size_of_committed_pages() const {
  return max_size_of_committed_pages_.load(std::memory_order_relaxed);
}

PA_ALWAYS_INLINE size_t
PartitionRoot::get_total_size_of_allocated_bytes() const {
  // Since this is only used for bookkeeping, we don't care if the value is
  // stale, so no need to get a lock here.
  return total_size_of_allocated_bytes_.load(std::memory_order_relaxed);
}

PA_ALWAYS_INLINE size_t PartitionRoot::get_max_size_of_allocated_bytes() const {
  // Since this is only used for bookkeeping, we don't care if the value is
  // stale, so no need to get a lock here.
  return max_size_of_allocated_bytes_.load(std::memory_order_relaxed);
}

PA_ALWAYS_INLINE internal::pool_handle PartitionRoot::ChoosePool() const {
  return settings_.pool_handle;
}
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
PA_ALWAYS_INLINE const internal::PoolOffsetLookup&
PartitionRoot::GetOffsetLookup() const {
  return settings_.offset_lookup;
}
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
PA_ALWAYS_INLINE const internal::ReservationOffsetTable&
PartitionRoot::GetReservationOffsetTable() const {
  return settings_.reservation_offset_table;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionRoot::GetDirectMapMetadataAndGuardPagesSize() {
  // Because we need to fake a direct-map region to look like a super page, we
  // need to allocate more pages around the payload:
  // - The first partition page is a combination of metadata and guard region.
  // - We also add a trailing guard page. In most cases, a system page would
  //   suffice. But on 32-bit systems when BRP is on, we need a partition page
  //   to match granularity of the BRP pool bitmap. For consistency, we'll use
  //   a partition page everywhere, which is cheap as it's uncommitted address
  //   space anyway.
  return 2 * internal::PartitionPageSize();
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionRoot::GetDirectMapSlotSize(size_t raw_size) {
  // Caller must check that the size is not above the MaxDirectMapped()
  // limit before calling. This also guards against integer overflow in the
  // calculation here.
  PA_DCHECK(raw_size <= internal::MaxDirectMapped());
  return partition_alloc::internal::base::bits::AlignUp(
      raw_size, internal::SystemPageSize());
}

PA_ALWAYS_INLINE size_t
PartitionRoot::GetDirectMapReservationSize(size_t padded_raw_size) {
  // Caller must check that the size is not above the MaxDirectMapped()
  // limit before calling. This also guards against integer overflow in the
  // calculation here.
  PA_DCHECK(padded_raw_size <= internal::MaxDirectMapped());
  return partition_alloc::internal::base::bits::AlignUp(
      padded_raw_size + GetDirectMapMetadataAndGuardPagesSize(),
      internal::DirectMapAllocationGranularity());
}

PA_ALWAYS_INLINE bool PartitionRoot::IsDirectMapped(
    partition_alloc::internal::SlotSpanMetadata* slot_span) const {
  return IsDirectMappedBucket(slot_span->bucket);
}

PA_ALWAYS_INLINE size_t PartitionRoot::AdjustSize0IfNeeded(size_t size) const {
  // On macOS and iOS, PartitionGetSizeEstimate() is used for two purposes:
  // as a zone dispatcher and as an underlying implementation of
  // malloc_size(3). As a zone dispatcher, zero has a special meaning of
  // "doesn't belong to this zone". When extras fill out the entire slot,
  // the usable size is 0, thus confusing the zone dispatcher.
  //
  // To save ourselves a branch on this hot path, we could eliminate this
  // check at compile time for cases not listed above. The #if statement would
  // be rather complex. Then there is also the fear of the unknown. The
  // existing cases were discovered through obscure, painful-to-debug crashes.
  // Better save ourselves trouble with not-yet-discovered cases.
  if (size == 0) [[unlikely]] {
    return 1;
  }
  return size;
}

// Adjusts the size by adding extras. Also include the 0->1 adjustment if
// needed.
PA_ALWAYS_INLINE size_t
PartitionRoot::AdjustSizeForExtrasAdd(size_t size) const {
  size = AdjustSize0IfNeeded(size);
  PA_DCHECK(size + settings_.extras_size >= size);
  return size + settings_.extras_size;
}

// Adjusts the size by subtracing extras. Doesn't include the 0->1 adjustment,
// which leads to an asymmetry with AdjustSizeForExtrasAdd, but callers of
// AdjustSizeForExtrasSubtract either expect the adjustment to be included, or
// are indifferent.
PA_ALWAYS_INLINE size_t
PartitionRoot::AdjustSizeForExtrasSubtract(size_t size) const {
  return size - settings_.extras_size;
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
PA_ALWAYS_INLINE bool PartitionRoot::brp_enabled() const {
  return settings_.brp_enabled_;
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

PA_ALWAYS_INLINE StraightenLargerSlotSpanFreeListsMode
PartitionRoot::GetStraightenLargerSlotSpanFreeListsMode() {
  return straighten_larger_slot_span_free_lists_;
}

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
PA_ALWAYS_INLINE std::ptrdiff_t PartitionRoot::MetadataOffset() const {
  return settings_.metadata_offset_;
}
#else
PA_ALWAYS_INLINE
PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR
size_t PartitionRoot::MetadataOffset() const {
  return internal::SystemPageSize();
}
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

PA_NO_SANITIZE("undefined")
PA_ALWAYS_INLINE const PartitionRoot::Bucket& PartitionRoot::bucket_at(
    size_t i) const {
  PA_DCHECK(i <= BucketIndexLookup::kNumBuckets);
  return PA_UNSAFE_TODO(buckets_[i]);
}
PA_ALWAYS_INLINE bool PartitionRoot::IsDirectMappedBucket(
    const PartitionRoot::Bucket* bucket) const {
  // All regular allocations are associated with a bucket in the |buckets_|
  // array. A range check is then sufficient to identify direct-mapped
  // allocations.
  bool ret = !(bucket >= this->buckets_ && bucket <= &this->sentinel_bucket_);
  PA_DCHECK(ret == bucket->is_direct_mapped());
  return ret;
}
template <AllocFlags flags>
PA_ALWAYS_INLINE bool PartitionRoot::AllocWithMemoryToolProlog(size_t size) {
  if (size > partition_alloc::internal::MaxDirectMapped()) {
    if constexpr (ContainsFlags(flags, AllocFlags::kReturnNull)) {
      // Early return indicating not to proceed with allocation
      return false;
    }
    PA_CHECK(false);
  }
  return true;  // Allocation should proceed
}
namespace internal {

PA_ALWAYS_INLINE ::partition_alloc::internal::Lock& PartitionRootLock(
    PartitionRoot* root) {
  return root->lock_;
}

class ScopedSyscallTimer {
 public:
#if PA_CONFIG(COUNT_SYSCALL_TIME)
  explicit ScopedSyscallTimer(PartitionRoot* root)
      : root_(root), tick_(base::TimeTicks::Now()) {}

  ~ScopedSyscallTimer() {
    root_->syscall_count_.fetch_add(1, std::memory_order_relaxed);

    int64_t elapsed_nanos = (base::TimeTicks::Now() - tick_).InNanoseconds();
    if (elapsed_nanos > 0) {
      root_->syscall_total_time_ns_.fetch_add(
          static_cast<uint64_t>(elapsed_nanos), std::memory_order_relaxed);
    }
  }

 private:
  PartitionRoot* root_;
  const base::TimeTicks tick_;
#else
  explicit ScopedSyscallTimer(PartitionRoot* root) {
    root->syscall_count_.fetch_add(1, std::memory_order_relaxed);
  }
#endif
};

}  // namespace internal

template <AllocFlags flags>
PA_ALWAYS_INLINE internal::UntaggedSlotStart PartitionRoot::AllocFromBucket(
    Bucket* bucket,
    size_t raw_size,
    size_t slot_span_alignment,
    size_t* usable_size,
    size_t* slot_size,
    bool* is_already_zeroed) {
  PA_DCHECK((slot_span_alignment >= internal::PartitionPageSize()) &&
            internal::base::bits::HasSingleBit(slot_span_alignment));
  SlotSpanMetadata* slot_span = bucket->active_slot_spans_head;
  // There always must be a slot span on the active list (could be a sentinel).
  PA_DCHECK(slot_span);
  // Check that it isn't marked full, which could only be true if the span was
  // removed from the active list.
  PA_DCHECK(!slot_span->marked_full);

  // Should check validity, but later in this function.
  internal::UntaggedSlotStart slot_start =
      internal::SlotStart::Unchecked(slot_span->get_freelist_head()).Untag();

  // Use the fast path when a slot is readily available on the free list of the
  // first active slot span. However, fall back to the slow path if a
  // higher-order alignment is requested, because an inner slot of an existing
  // slot span is unlikely to satisfy it.
  if (slot_span_alignment <= internal::PartitionPageSize() &&
      slot_start.value()) [[likely]] {
    *is_already_zeroed = false;
    // This is a fast path, avoid calling GetSlotUsableSize() in Release builds
    // as it is costlier. Copy its small bucket path instead.
    *usable_size = AdjustSizeForExtrasSubtract(bucket->slot_size);
    PA_DCHECK(*usable_size == GetSlotUsableSize(slot_span));

    // If these DCHECKs fire, you probably corrupted memory.
    PA_CHECK(DeducedRootIsValid(slot_span));

    // All large allocations must go through the slow path to correctly update
    // the size metadata.
    PA_DCHECK(!slot_span->CanStoreRawSize());
    PA_DCHECK(!slot_span->bucket->is_direct_mapped());

    void* entry = slot_span->PopForAlloc(bucket->slot_size);

    PA_DCHECK(internal::SlotStart::Unchecked(entry).Untag() == slot_start);

    PA_DCHECK(slot_span->bucket == bucket);
  } else {
    // Should check validity, but later in this function.
    slot_start = internal::UntaggedSlotStart::Unchecked(
        bucket->SlowPathAlloc(this, flags, raw_size, slot_span_alignment,
                              &slot_span, is_already_zeroed));
    if (!slot_start) [[unlikely]] {
      return internal::UntaggedSlotStart();
    }
    PA_DCHECK(slot_span == SlotSpanMetadata::FromSlotStart(slot_start, this));
    PA_CHECK(DeducedRootIsValid(slot_span));
    // For direct mapped allocations, |bucket| is the sentinel.
    PA_DCHECK((slot_span->bucket == bucket) ||
              (slot_span->bucket->is_direct_mapped() &&
               (bucket == &sentinel_bucket_)));

    *usable_size = GetSlotUsableSize(slot_span);
  }

  slot_start.Check(this);

  *slot_size = slot_span->bucket->slot_size;
  return slot_start;
}

AllocationNotificationData PartitionRoot::CreateAllocationNotificationData(
    void* object,
    size_t size,
    const char* type_name) const {
  AllocationNotificationData notification_data(object, size, type_name);

  if (IsMemoryTaggingEnabled()) {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    notification_data.SetMteReportingMode(memory_tagging_reporting_mode());
#endif
  }

  return notification_data;
}

FreeNotificationData PartitionRoot::CreateDefaultFreeNotificationData(
    void* address) {
  return FreeNotificationData(address);
}

FreeNotificationData PartitionRoot::CreateFreeNotificationData(
    void* address) const {
  FreeNotificationData notification_data =
      CreateDefaultFreeNotificationData(address);

  if (IsMemoryTaggingEnabled()) {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    notification_data.SetMteReportingMode(memory_tagging_reporting_mode());
#endif
  }

  return notification_data;
}

// static
template <FreeFlags flags>
PA_ALWAYS_INLINE bool PartitionRoot::FreeProlog(void* object,
                                                const PartitionRoot* root) {
  static_assert(AreValidFlags(flags));
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  if constexpr (!ContainsFlags(flags, FreeFlags::kNoMemoryToolOverride)) {
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
    if (ContainsFlags(flags, FreeFlags::kAlignedFreeForMemoryTool)) {
      _aligned_free(object);
    } else {
      free(object);
    }
#else   // !PA_BUILDFLAG(PA_COMPILER_MSVC)
    free(object);
#endif  // PA_BUILDFLAG(PA_COMPILER_MSVC)
    return true;
  }
#else   // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // If the memory tool is not replacing the allocator, then the
  // kAlignedFreeForMemoryTool flag is unused and should not be passed.
  static_assert(!ContainsFlags(flags, FreeFlags::kAlignedFreeForMemoryTool));
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

  if (!object) [[unlikely]] {
    return true;
  }

  if constexpr (ContainsFlags(flags, FreeFlags::kNoHooks)) {
    return false;
  }

  if (PartitionAllocHooks::AreHooksEnabled()) {
    // A valid |root| might not be available if this function is called from
    // |FreeInUnknownRoot| and not deducible if object originates from
    // an override hook.
    // TODO(crbug.com/40152647): See if we can make the root available more
    // reliably or even make this function non-static.
    auto notification_data = root ? root->CreateFreeNotificationData(object)
                                  : CreateDefaultFreeNotificationData(object);
    PartitionAllocHooks::FreeObserverHookIfEnabled(notification_data);
    if (PartitionAllocHooks::FreeOverrideHookIfEnabled(object)) {
      return true;
    }
  }

  return false;
}

// static
PA_ALWAYS_INLINE PartitionRoot*
PartitionRoot::GetRootFromAddressInFirstSuperpage(void* object) {
  PA_DCHECK(object);
  // Fetch the root from the address, and not SlotSpanMetadata. This is
  // important, as obtaining it from SlotSpanMetadata is a slow operation
  // (looking into the metadata area, and following a pointer), which can induce
  // cache coherency traffic (since they're read on every free(), and written to
  // on any malloc()/free() that is not a hit in the thread cache). This way we
  // change the critical path from object -> slot_span -> root into two
  // *parallel* ones:
  // 1. object -> root
  // 2. object -> slot_span (inside FreeInline)
  uintptr_t object_addr =
      internal::SlotStart::Unchecked(object).Untag().value();
  return FromAddrInFirstSuperpage(object_addr);
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeInlineInUnknownRoot(void* object) {
  bool early_return = FreeProlog<flags>(object, nullptr);
  if (early_return) {
    return;
  }
  // FreeProlog ensures the object is not nullptr.
  PA_DCHECK(object);

  auto* root = GetRootFromAddressInFirstSuperpage(object);
  root->FreeInlineInternal<flags | FreeFlags::kNoHooks>(object);
}

// static
template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeInlineInUnknownRoot(
    void* object,
    FreeHintType<FreeHintFlags(flags)> hint) {
  bool early_return = FreeProlog<flags>(object, nullptr);
  if (early_return) {
    return;
  }
  // FreeProlog ensures the object is not nullptr.
  PA_DCHECK(object);

  auto* root = GetRootFromAddressInFirstSuperpage(object);
  root->FreeInline<flags | FreeFlags::kNoHooks>(object, hint);
}

PA_ALWAYS_INLINE std::pair<internal::SlotStart, internal::SlotSpanMetadata*>
PartitionRoot::GetSlotStartAndSlotSpanFromAddress(void* object) {
  PA_DCHECK(object);

  // On some platforms, malloc() interception is fragile. For example, on
  // Android, malloc() interception is more fragile than on other platforms,
  // as we use wrapped symbols. However, the pools allow us to quickly tell
  // that a pointer was allocated with PartitionAlloc.
  //
  // This is a crash to detect imperfect symbol interception. However, we can
  // forward allocations we don't own to the system malloc() implementation in
  // these rare cases, assuming that some remain.
  //
  // On platforms with ENABLE_SYSTEM_FREE_FALLBACK, this is already checked in
  // PartitionFree() in the shim.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    !PA_BUILDFLAG(ENABLE_SYSTEM_FREE_FALLBACK)
  uintptr_t object_addr =
      internal::SlotStart::Unchecked(object).Untag().value();
  PA_CHECK(IsManagedByPartitionAlloc(object_addr));
#endif

  internal::SlotStart slot_start = internal::SlotStart::Checked(object, this);
  SlotSpanMetadata* slot_span =
      SlotSpanMetadata::FromSlotStart(slot_start.Untag(), this);
  PA_DCHECK(PartitionRoot::FromSlotSpanMetadata(slot_span) == this);
  PA_DCHECK(slot_span ==
            SlotSpanMetadata::FromSlotStart(slot_start.Untag(), this));

  return {slot_start, slot_span};
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeInlineInternal(void* object) {
  // The correct PartitionRoot might not be deducible if the |object| originates
  // from an override hook.
  bool early_return = FreeProlog<flags>(object, this);
  if (early_return) {
    return;
  }
  // FreeProlog ensures the object is not nullptr.
  PA_DCHECK(object);

  // Almost all calls to FreeNoNooks() will end up writing to |*object|.
  PA_PREFETCH_FOR_WRITE(object);
  auto [slot_start, slot_span] = GetSlotStartAndSlotSpanFromAddress(object);
  // We are going to read from |*slot_span| in all branches, but haven't
  // done it yet.
  PA_PREFETCH(slot_span);
  FreeNoHooksImmediate<flags>(slot_start, slot_span);
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeInlineInternal(
    void* object,
    FreeHintType<FreeHintFlags(flags)> hint) {
  // The correct PartitionRoot might not be deducible if the |object| originates
  // from an override hook.
  bool early_return = FreeProlog<flags>(object, this);
  if (early_return) {
    return;
  }
  // FreeProlog ensures the object is not nullptr.
  PA_DCHECK(object);

  // Almost all calls to FreeWithSizeNoHooks() will end up writing to |*object|.
  PA_PREFETCH_FOR_WRITE(object);
  auto [slot_start, slot_span] = GetSlotStartAndSlotSpanFromAddress(object);

  if constexpr (ContainsFlags(flags, FreeFlags::kWithAlignmentHint) &&
                ContainsFlags(flags, FreeFlags::kWithSizeHint)) {
    if (settings_.enable_free_with_size) {
      auto adjusted_size =
          GetAdjustedSizeForAlignment(hint.alignment, hint.size);
      // Overflow check. adjusted_size must be larger or equal to the original
      // size.
      PA_CHECK(adjusted_size >= hint.size);

      FreeHintType<FreeHintFlags(flags)> new_hint = hint;
      new_hint.size = adjusted_size;
      FreeNoHooksImmediate<flags>(slot_start, slot_span, new_hint);
      return;
    }
  }

  // We are going to read from |*slot_span| in all branches, but haven't
  // done it yet.
  if constexpr (!ContainsFlags(flags, FreeFlags::kWithSizeHint)) {
    PA_PREFETCH(slot_span);
  }
  FreeNoHooksImmediate<flags>(slot_start, slot_span, hint);
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeNoHooksImmediateInternal(
    internal::SlotStart slot_start,
    SlotSpanMetadata* slot_span,
    const internal::BucketSizeDetails& size_details) {
  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/in-slot metadata management
  // - Before the "raw" allocator.
  //
  // On the deallocation side:
  // 1. Check cookie/in-slot metadata, adjust the pointer
  // 2. Deallocation
  //   a. Return to the thread cache if possible. If it succeeds, return.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  PA_DCHECK(slot_start);
  PA_DCHECK(slot_span);
  PA_DCHECK(DeducedRootIsValid(slot_span));

  // Layout inside the slot:
  //   |...object...|[empty]|[cookie]|[unused]|[metadata]|
  //   <--------(a)--------->
  //                        <--(b)--->   +    <---(b)---->
  //   <-------------(c)------------->   +    <---(c)---->
  //     (a) usable_size
  //     (b) extras
  //     (c) utilized_slot_size
  //
  // Note: in-slot metadata and cookie can be 0-sized.
  //
  // For more context, see the other "Layout inside the slot" comment inside
  // AllocInternalNoHooks().

#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
  if (settings_.use_cookie) {
    // Verify the cookie after the allocated region.
    // If this assert fires, you probably corrupted memory.
    const size_t usable_size = GetSlotUsableSize(size_details, slot_span);
    internal::PartitionCookieCheckValue(
        PA_UNSAFE_TODO(slot_start.ToObject() + usable_size), usable_size);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_COOKIE)

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (brp_enabled()) [[likely]] {
    auto* ref_count = InSlotMetadataPointerFromSlotStartAndSize(
        slot_start.Untag(), size_details.slot_size);
    // If there are no more references to the allocation, it can be freed
    // immediately. Otherwise, defer the operation and zap the memory to turn
    // potential use-after-free issues into unexploitable crashes. Zapping must
    // complete before we clear kMemoryHeldByAllocatorBit in
    // ReleaseFromAllocator(), otherwise another thread may allocate and start
    // using the slot in the middle of zapping.
    bool was_zapped = false;
    if (!ref_count->IsAliveWithNoKnownRefs()) [[unlikely]] {
      was_zapped = true;
      QuarantineForBrp(slot_span, slot_start);
    }

    if (!(ref_count->ReleaseFromAllocator(slot_start.Untag(), slot_span)))
        [[unlikely]] {
      PA_CHECK(was_zapped);
      total_size_of_brp_quarantined_bytes.fetch_add(
          slot_span->GetSlotSizeForBookkeeping(), std::memory_order_relaxed);
      total_count_of_brp_quarantined_slots_.fetch_add(
          1, std::memory_order_relaxed);
      cumulative_size_of_brp_quarantined_bytes_.fetch_add(
          slot_span->GetSlotSizeForBookkeeping(), std::memory_order_relaxed);
      cumulative_count_of_brp_quarantined_slots_.fetch_add(
          1, std::memory_order_relaxed);

      if constexpr (ContainsFlags(flags, FreeFlags::kSchedulerLoopQuarantine)) {
        // This flag is to be read on `FreeAfterBRPQuarantine()`.
        ref_count->SetQuarantineRequest();
      }
      return;
    }
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  // memset() can be really expensive.
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  internal::DebugMemset(slot_start.ToObject(), internal::kFreedByte,
                        slot_span->GetUtilizedSlotSize());
#endif  // PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)

  if constexpr (ContainsFlags(flags, FreeFlags::kIntendedLeak)) {
    // Must not enable `thread_cache` and `brp` to use `kIntendedLeak`.
    PA_CHECK(!settings_.with_thread_cache);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    PA_CHECK(!brp_enabled());
#endif
    intended_leak_size_.fetch_add(size_details.slot_size);
    return;  // Leak
  } else if constexpr (ContainsFlags(flags,
                                     FreeFlags::kSchedulerLoopQuarantine)) {
    internal::ThreadCache* thread_cache = GetThreadCache();
    if (internal::ThreadCache::IsValid(thread_cache)) [[likely]] {
      thread_cache->GetSchedulerLoopQuarantineBranch().Quarantine(
          slot_start, slot_span, size_details);
    } else {
      scheduler_loop_quarantine_.Quarantine(slot_start, slot_span,
                                            size_details);
    }
    return;
  } else if constexpr (
      ContainsFlags(
          flags,
          FreeFlags::kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks)) {
    scheduler_loop_quarantine_for_advanced_memory_safety_checks_.Quarantine(
        slot_start, slot_span, size_details);
    return;
  }

  RawFreeWithThreadCache(slot_start, size_details, slot_span);
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeNoHooksImmediate(
    internal::SlotStart slot_start,
    SlotSpanMetadata* slot_span) {
  auto size_details = SlotSpanToBucketSizeDetails(slot_span);
  FreeNoHooksImmediateInternal<flags>(slot_start, slot_span, size_details);
}

template <FreeFlags flags>
PA_ALWAYS_INLINE void PartitionRoot::FreeNoHooksImmediate(
    internal::SlotStart slot_start,
    SlotSpanMetadata* slot_span,
    FreeHintType<FreeHintFlags(flags)> hint) {
  internal::BucketSizeDetails size_details;
  if constexpr (ContainsFlags(flags, FreeFlags::kWithSizeHint)) {
    if (settings_.enable_free_with_size) {
      size_details = SizeToBucketSizeDetails(hint.size, slot_span);
      FreeNoHooksImmediateInternal<flags>(slot_start, slot_span, size_details);
      return;
    }
  }
  size_details = SlotSpanToBucketSizeDetails(slot_span);

  FreeNoHooksImmediateInternal<flags>(slot_start, slot_span, size_details);
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
// static
PA_ALWAYS_INLINE void PartitionRoot::FreeAfterBRPQuarantine(
    internal::UntaggedSlotStart slot_start,
    size_t slot_size) {
  auto* slot_span = internal::SlotSpanMetadata::FromSlotStart(slot_start);
  auto* root = PartitionRoot::FromSlotSpanMetadata(slot_span);
  // Currently, InSlotMetadata is allocated when BRP is used.
  PA_DCHECK(root->brp_enabled());
  PA_DCHECK(!PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
                 slot_start, slot_span->bucket->slot_size)
                 ->IsAlive());

  // Iterating over the entire slot can be really expensive.
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
  auto hook = PartitionAllocHooks::GetQuarantineOverrideHook();
  // If we have a hook the object segment is not necessarily filled
  // with |kQuarantinedByte|.
  if (!hook) [[likely]] {
    unsigned char* object = slot_start.Tag().ToObject();
    for (size_t i = 0; i < root->GetSlotUsableSize(slot_span); ++i) {
      PA_UNSAFE_TODO(PA_DCHECK(object[i] == internal::kQuarantinedByte));
    }
  }
  internal::DebugMemset(slot_start.Tag().ToObject(), internal::kFreedByte,
                        slot_span->GetUtilizedSlotSize());
#endif  // PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)

  root->total_size_of_brp_quarantined_bytes.fetch_sub(
      slot_span->GetSlotSizeForBookkeeping(), std::memory_order_relaxed);
  root->total_count_of_brp_quarantined_slots_.fetch_sub(
      1, std::memory_order_relaxed);

  internal::InSlotMetadata* metadata =
      internal::InSlotMetadataPointer(slot_start.value(), slot_size);
  auto size_details = internal::BucketSizeDetails{
      .bucket_index =
          SizeToBucketIndex(slot_size, root->GetBucketDistribution()),
      .slot_size = slot_size,
  };
  PA_DCHECK(slot_size == slot_span->bucket->slot_size);

  // `FreeFlags::kSchedulerLoopQuarantine` was used for the original `Free()`
  // call. Send the allocation to yet another quarantine.
  if (metadata->PopQuarantineRequest()) {
    internal::ThreadCache* thread_cache = root->GetThreadCache();
    if (internal::ThreadCache::IsValid(thread_cache)) [[likely]] {
      thread_cache->GetSchedulerLoopQuarantineBranch().Quarantine(
          slot_start.Tag(), slot_span, size_details);
    } else {
      root->scheduler_loop_quarantine_.Quarantine(slot_start.Tag(), slot_span,
                                                  size_details);
    }
  } else {
    root->RawFreeWithThreadCache(slot_start.Tag(), size_details, slot_span);
  }
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

PA_ALWAYS_INLINE void PartitionRoot::FreeInSlotSpan(
    internal::UntaggedSlotStart slot_start,
    SlotSpanMetadata* slot_span) {
  return slot_span->Free(slot_start.value(), this);
}

#if PA_CONFIG(IS_NONCLANG_MSVC)
// MSVC only supports inline assembly on x86. This preprocessor directive
// is intended to be a replacement for the same.
//
// TODO(crbug.com/40234441): Make sure inlining doesn't degrade this into
// a no-op or similar. The documentation doesn't say.
#pragma optimize("", off)
#endif
PA_ALWAYS_INLINE void PartitionRoot::RawFree(internal::SlotStart slot_start,
                                             SlotSpanMetadata* slot_span) {
  void* ptr = slot_start.ToObject();
  // At this point we are about to acquire the lock, so we try to minimize the
  // risk of blocking inside the locked section.
  //
  // For allocations that are not direct-mapped, there will always be a store at
  // the beginning of |*slot_start|, to link the freelist. This is why there is
  // a prefetch of it at the beginning of the free() path.
  //
  // However, the memory which is being freed can be very cold (for instance
  // during browser shutdown, when various caches are finally completely freed),
  // and so moved to either compressed memory or swap. This means that touching
  // it here can cause a major page fault. This is in turn will cause
  // descheduling of the thread *while locked*. Since we don't have priority
  // inheritance locks on most platforms, avoiding long locked periods relies on
  // the OS having proper priority boosting. There is evidence
  // (crbug.com/1228523) that this is not always the case on Windows, and a very
  // low priority background thread can block the main one for a long time,
  // leading to hangs.
  //
  // To mitigate that, make sure that we fault *before* locking. Note that this
  // is useless for direct-mapped allocations (which are very rare anyway), and
  // that this path is *not* taken for thread cache bucket purge (since it calls
  // RawFreeLocked()). This is intentional, as the thread cache is purged often,
  // and the memory has a consequence the memory has already been touched
  // recently (to link the thread cache freelist).
  *static_cast<volatile uintptr_t*>(ptr) = 0;
  // Note: even though we write to slot_start + sizeof(void*) as well, due to
  // alignment constraints, the two locations are always going to be in the same
  // OS page. No need to write to the second one as well.
  //
  // Do not move the store above inside the locked section.
#if !(PA_CONFIG(IS_NONCLANG_MSVC))
  __asm__ __volatile__("" : : "r"(slot_start) : "memory");
#endif
  // This is done for memory usage (by improving the compression ratio of heap
  // pages), not for security, so we care more about being affordable than
  // prompt. This is done after the thread cache, so most deallocation do not
  // end up here. Nevertheless, we do not need to memset() direct-mapped
  // allocations, as they are released right away. And single-slot slot spans
  // are also excluded, because they can be entirely decommitted once leaving
  // the global ring.
  //
  // This is done before acquiring the lock, to prevent page faults causing
  // issues there.
  if (settings_.eventually_zero_freed_memory &&
      !IsDirectMappedBucket(slot_span->bucket) &&
      slot_span->bucket->get_slots_per_span() > 1) {
    internal::SecureMemset(ptr, 0, GetSlotUsableSize(slot_span));
  }

  DecreaseTotalSizeOfAllocatedBytes(slot_start.value(),
                                    slot_span->GetSlotSizeForBookkeeping());

  ::partition_alloc::internal::ScopedGuard guard{
      internal::PartitionRootLock(this)};
  FreeInSlotSpan(slot_start.Untag(), slot_span);
}
#if PA_CONFIG(IS_NONCLANG_MSVC)
#pragma optimize("", on)
#endif

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
PA_ALWAYS_INLINE void PartitionRoot::RetagSlotIfNeeded(
    internal::UntaggedSlotStart slot_start,
    size_t slot_size) {
  void* slot_start_ptr = slot_start.Tag().ToObject();
  // This branch is |likely| because HAS_MEMORY_TAGGING build flag is true for
  // arm64 Android devices and only a small portion of them will have memory
  // tagging enabled.
  if (!IsMemoryTaggingEnabled()) [[likely]] {
    return;
  }

  if (slot_size <= internal::kMaxMemoryTaggingSize) [[likely]] {
    if (UseRandomMemoryTagging()) {
      // Exclude the previous tag so that immediate use after free is detected
      // 100% of the time.
      uint8_t previous_tag = internal::ExtractTagFromPtr(slot_start_ptr);
      internal::TagMemoryRangeRandomly(slot_start_ptr, slot_size,
                                       1 << previous_tag);
    } else {
      internal::TagMemoryRangeIncrement(slot_start_ptr, slot_size);
    }
  }
}
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)

PA_ALWAYS_INLINE void PartitionRoot::RawFreeWithThreadCache(
    internal::SlotStart slot_start,
    const internal::BucketSizeDetails& size_details,
    SlotSpanMetadata* slot_span) {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  RetagSlotIfNeeded(slot_start.Untag(), size_details.slot_size);
  slot_start = slot_start.Untag().Tag();
#endif

  // `[[likely]]`: performance-sensitive partitions have a thread cache,
  // direct-mapped allocations are uncommon.
  internal::ThreadCache* thread_cache = GetThreadCache();
  // TODO(crbug.com/467243745): Once
  // `internal::ThreadCache::largest_active_bucket_index_` becomes a per-class
  // variable, remove the initialization check in `IsValid` and reuse the
  // `bucket_index > largest_active_bucket_index_` within `MaybePutInCache`.
  if (internal::ThreadCache::IsValid(thread_cache) &&
      (size_details.slot_size <= BucketIndexLookup::kMaxBucketSize))
      [[likely]] {
    PA_DCHECK(!IsDirectMappedBucket(slot_span->bucket));
    std::optional<size_t> slot_size = thread_cache->MaybePutInCache(
        slot_start.Untag(), size_details.bucket_index);
    if (slot_size.has_value()) [[likely]] {
      // This is a fast path, avoid calling GetSlotUsableSize() in Release
      // builds as it is costlier. Copy its small bucket path instead.
      PA_DCHECK(!slot_span->CanStoreRawSize());
      size_t usable_size = AdjustSizeForExtrasSubtract(slot_size.value());
      PA_DCHECK(usable_size == GetSlotUsableSize(slot_span));
      thread_cache->RecordDeallocation(usable_size);
      return;
    }
  }

  if (internal::ThreadCache::IsValid(thread_cache)) [[likely]] {
    // Accounting must be done outside `RawFree()`, as it's also called from
    // the thread cache. We would double-count otherwise.
    //
    // GetSlotUsableSize() will always give the correct result, and we are in
    // a slow path here (since the thread cache case returned earlier).
    size_t usable_size = GetSlotUsableSize(slot_span);
    thread_cache->RecordDeallocation(usable_size);
  }
  RawFree(slot_start, slot_span);
}

PA_ALWAYS_INLINE void PartitionRoot::RawFreeLocked(
    internal::UntaggedSlotStart slot_start,
    SlotSpanMetadata* slot_span) {
  // Direct-mapped deallocation releases then re-acquires the lock. The caller
  // may not expect that, but we never call this function on direct-mapped
  // allocations.
  PA_DCHECK(!IsDirectMappedBucket(slot_span->bucket));
  DecreaseTotalSizeOfAllocatedBytes(slot_start.value(),
                                    slot_span->GetSlotSizeForBookkeeping());
  FreeInSlotSpan(slot_start, slot_span);
}

PA_ALWAYS_INLINE PartitionRoot* PartitionRoot::FromSlotSpanMetadata(
    const SlotSpanMetadata* slot_span) {
  auto* extent_entry = reinterpret_cast<SuperPageExtentEntry*>(
      reinterpret_cast<uintptr_t>(slot_span) & internal::SystemPageBaseMask());
  return extent_entry->root;
}

PA_ALWAYS_INLINE PartitionRoot* PartitionRoot::FromFirstSuperPage(
    uintptr_t super_page) {
  PA_DCHECK(internal::ReservationOffsetTable::Get(super_page)
                .IsReservationStart(super_page));
  // Slow
  const std::ptrdiff_t offset = internal::GetMetadataOffsetFromAddr(super_page);
  auto* extent_entry = internal::PartitionSuperPageToExtent(super_page, offset);
  PartitionRoot* root = extent_entry->root;
  PA_DCHECK(root->inverted_self_ == ~reinterpret_cast<uintptr_t>(root));
  return root;
}

PA_ALWAYS_INLINE PartitionRoot* PartitionRoot::FromAddrInFirstSuperpage(
    uintptr_t address) {
  uintptr_t super_page = address & internal::kSuperPageBaseMask;
  PA_DCHECK(internal::ReservationOffsetTable::Get(super_page)
                .IsReservationStart(super_page));
  return FromFirstSuperPage(super_page);
}

PA_ALWAYS_INLINE void PartitionRoot::IncreaseTotalSizeOfAllocatedBytes(
    uintptr_t addr,
    size_t len,
    size_t raw_size) {
  // |total_size_of_allocated_bytes_| is only for debugging/stats, so relaxed
  // memory order is sufficient.
  size_t previous_total_size_of_allocated_bytes =
      total_size_of_allocated_bytes_.fetch_add(len, std::memory_order_relaxed);
  size_t new_total_size_of_allocated_bytes =
      previous_total_size_of_allocated_bytes + len;

  size_t expected, desired;
  do {
    expected = max_size_of_allocated_bytes_.load(std::memory_order_relaxed);
    desired = std::max(expected, new_total_size_of_allocated_bytes);
  } while (!max_size_of_allocated_bytes_.compare_exchange_weak(
      expected, desired, std::memory_order_relaxed, std::memory_order_relaxed));

#if PA_BUILDFLAG(RECORD_ALLOC_INFO)
  partition_alloc::internal::RecordAllocOrFree(addr | 0x01, raw_size);
#endif  // PA_BUILDFLAG(RECORD_ALLOC_INFO)
}

PA_ALWAYS_INLINE void PartitionRoot::DecreaseTotalSizeOfAllocatedBytes(
    uintptr_t addr,
    size_t len) {
  // An underflow here means we've miscounted |total_size_of_allocated_bytes_|
  // somewhere.
  // |total_size_of_allocated_bytes_| is only for debugging/stats, so relaxed
  // memory order is sufficient.
  size_t previous_total_size_of_allocated_bytes =
      total_size_of_allocated_bytes_.fetch_sub(len, std::memory_order_relaxed);
  PA_DCHECK(previous_total_size_of_allocated_bytes >= len);
#if PA_BUILDFLAG(RECORD_ALLOC_INFO)
  partition_alloc::internal::RecordAllocOrFree(addr | 0x00, len);
#endif  // PA_BUILDFLAG(RECORD_ALLOC_INFO)
}

PA_ALWAYS_INLINE void PartitionRoot::IncreaseCommittedPages(size_t len) {
  const auto old_total =
      total_size_of_committed_pages_.fetch_add(len, std::memory_order_relaxed);

  const auto new_total = old_total + len;

  // This function is called quite frequently; to avoid performance problems, we
  // don't want to hold a lock here, so we use compare and exchange instead.
  size_t expected =
      max_size_of_committed_pages_.load(std::memory_order_relaxed);
  size_t desired;
  do {
    desired = std::max(expected, new_total);
  } while (!max_size_of_committed_pages_.compare_exchange_weak(
      expected, desired, std::memory_order_relaxed, std::memory_order_relaxed));
}

PA_ALWAYS_INLINE void PartitionRoot::DecreaseCommittedPages(size_t len) {
  total_size_of_committed_pages_.fetch_sub(len, std::memory_order_relaxed);
}

PA_ALWAYS_INLINE void PartitionRoot::DecommitSystemPagesForData(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  internal::ScopedSyscallTimer timer{this};
  DecommitSystemPages(address, length, accessibility_disposition);
  DecreaseCommittedPages(length);
}

// Not unified with TryRecommitSystemPagesForData() to preserve error codes.
PA_ALWAYS_INLINE void PartitionRoot::RecommitSystemPagesForData(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition,
    bool request_tagging) {
  internal::ScopedSyscallTimer timer{this};

  auto page_accessibility = GetPageAccessibility(request_tagging);
  bool ok = TryRecommitSystemPages(address, length, page_accessibility,
                                   accessibility_disposition);
  if (!ok) [[unlikely]] {
    // Decommit some memory and retry. The alternative is crashing.
    DecommitEmptySlotSpans();
    RecommitSystemPages(address, length, page_accessibility,
                        accessibility_disposition);
  }

  IncreaseCommittedPages(length);
}

template <bool already_locked>
PA_ALWAYS_INLINE bool PartitionRoot::TryRecommitSystemPagesForDataInternal(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition,
    bool request_tagging) {
  internal::ScopedSyscallTimer timer{this};

  auto page_accessibility = GetPageAccessibility(request_tagging);
  bool ok = TryRecommitSystemPages(address, length, page_accessibility,
                                   accessibility_disposition);
  if (!ok) [[unlikely]] {
    {
      // Decommit some memory and retry. The alternative is crashing.
      if constexpr (!already_locked) {
        ::partition_alloc::internal::ScopedGuard guard(
            internal::PartitionRootLock(this));
        DecommitEmptySlotSpans();
      } else {
        internal::PartitionRootLock(this).AssertAcquired();
        DecommitEmptySlotSpans();
      }
    }
    ok = TryRecommitSystemPages(address, length, page_accessibility,
                                accessibility_disposition);
  }

  if (ok) {
    IncreaseCommittedPages(length);
  }

  return ok;
}

PA_ALWAYS_INLINE bool
PartitionRoot::TryRecommitSystemPagesForDataWithAcquiringLock(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition,
    bool request_tagging) {
  return TryRecommitSystemPagesForDataInternal<false>(
      address, length, accessibility_disposition, request_tagging);
}

PA_ALWAYS_INLINE
bool PartitionRoot::TryRecommitSystemPagesForDataLocked(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition,
    bool request_tagging) {
  return TryRecommitSystemPagesForDataInternal<true>(
      address, length, accessibility_disposition, request_tagging);
}

PA_ALWAYS_INLINE size_t PartitionRoot::GetSlotUsableSize(
    const internal::BucketSizeDetails& size_details,
    SlotSpanMetadata* slot_span) {
  if (size_details.slot_size <= kThreadCacheLargeSizeThreshold) [[likely]] {
    PA_DCHECK(!slot_span->CanStoreRawSize());
    auto usable_size = AdjustSizeForExtrasSubtract(size_details.slot_size);
    PA_DCHECK(usable_size == GetSlotUsableSize(slot_span));
    return usable_size;
  }
  return GetSlotUsableSize(slot_span);
}

// Returns the page configuration to use when mapping slot spans for a given
// partition root. ReadWriteTagged is used on MTE-enabled systems for
// PartitionRoots supporting it.
PA_ALWAYS_INLINE PageAccessibilityConfiguration
PartitionRoot::GetPageAccessibility(bool request_tagging) const {
  PageAccessibilityConfiguration::Permissions permissions =
      PageAccessibilityConfiguration::kReadWrite;
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  if (IsMemoryTaggingEnabled() && request_tagging) {
    permissions = PageAccessibilityConfiguration::kReadWriteTagged;
  }
#endif
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  return PageAccessibilityConfiguration(permissions,
                                        settings_.thread_isolation);
#else
  return PageAccessibilityConfiguration(permissions);
#endif
}

PA_ALWAYS_INLINE PageAccessibilityConfiguration
PartitionRoot::PageAccessibilityWithThreadIsolationIfEnabled(
    PageAccessibilityConfiguration::Permissions permissions) const {
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  return PageAccessibilityConfiguration(permissions,
                                        settings_.thread_isolation);
#endif
  return PageAccessibilityConfiguration(permissions);
}

// Return the capacity of the underlying slot (adjusted for extras). This
// doesn't mean this capacity is readily available. It merely means that if
// a new allocation (or realloc) happened with that returned value, it'd use
// the same amount of underlying memory.
PA_ALWAYS_INLINE size_t PartitionRoot::AllocationCapacityFromSlotStart(
    internal::UntaggedSlotStart slot_start) const {
  auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start, this);
  return AdjustSizeForExtrasSubtract(slot_span->bucket->slot_size);
}

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
PA_ALWAYS_INLINE internal::InSlotMetadata*
PartitionRoot::InSlotMetadataPointerFromSlotStartAndSize(
    internal::UntaggedSlotStart slot_start,
    size_t slot_size) {
  return internal::InSlotMetadataPointer(slot_start.value(), slot_size);
}

PA_ALWAYS_INLINE internal::InSlotMetadata*
PartitionRoot::InSlotMetadataPointerFromObjectForTesting(void* object) const {
  auto slot_start = internal::SlotStart::Unchecked(object).Untag();
  auto* slot_span = SlotSpanMetadata::FromSlotStart(slot_start, this);
  return InSlotMetadataPointerFromSlotStartAndSize(
      slot_start, slot_span->bucket->slot_size);
}
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

// static
PA_ALWAYS_INLINE uint16_t
PartitionRoot::SizeToBucketIndex(size_t size,
                                 BucketDistribution bucket_distribution) {
  switch (bucket_distribution) {
    case BucketDistribution::kNeutral:
      return BucketIndexLookup::GetIndexForNeutralBuckets(size);
    case BucketDistribution::kDenser:
      return BucketIndexLookup::GetIndexForDenserBuckets(size);
  }
  PA_NOTREACHED();
}

PA_ALWAYS_INLINE internal::BucketSizeDetails
PartitionRoot::SlotSpanToBucketSizeDetails(SlotSpanMetadata* slot_span) const {
  return internal::BucketSizeDetails{
      .bucket_index = static_cast<uint16_t>(slot_span->bucket - this->buckets_),
      .slot_size = slot_span->bucket->slot_size,
  };
}

PA_ALWAYS_INLINE internal::BucketSizeDetails
PartitionRoot::SizeToBucketSizeDetails(size_t requested_size,
                                       SlotSpanMetadata* slot_span) const {
  auto raw_size = AdjustSizeForExtrasAdd(requested_size);
  if (raw_size <= BucketIndexLookup::kMaxBucketSize) [[likely]] {
    // For non-direct-mapped allocations, `bucket_index` and `slot_size` are
    // determined without using `slot_span`.
    auto bucket_index =
        SizeToBucketIndex(raw_size, this->GetBucketDistribution());
    auto slot_size = BucketIndexLookup::GetBucketSize(bucket_index);
    if (settings_.enable_strict_free_size_check) {
      // TODO(crbug.com/410190984): Remove this prefetch & CHECKS once the
      // PA_CHECK of the given size against the slot span metadata is replaced
      // with a PA_DCHECK.
      PA_PREFETCH(slot_span);
      PA_CHECK(bucket_index ==
               static_cast<uint16_t>(slot_span->bucket - this->buckets_));
      PA_CHECK(slot_size == slot_span->bucket->slot_size);
    } else {
      PA_DCHECK(bucket_index ==
                static_cast<uint16_t>(slot_span->bucket - this->buckets_));
      PA_DCHECK(slot_size == slot_span->bucket->slot_size);
    }
    return internal::BucketSizeDetails{
        .bucket_index = bucket_index,
        .slot_size = slot_size,
    };
  }
  // For direct-mapped allocations, `slot_size` is derived from `slot_span`.
  // `bucket_index` is also populated from `slot_span` although its value is
  // invalid and SHOULD NOT be used.
  return SlotSpanToBucketSizeDetails(slot_span);
}

template <AllocFlags flags>
PA_ALWAYS_INLINE void* PartitionRoot::AllocInternal(size_t requested_size,
                                                    size_t slot_span_alignment,
                                                    const char* type_name) {
  static_assert(AreValidFlags(flags));
  PA_DCHECK((slot_span_alignment >= internal::PartitionPageSize()) &&
            internal::base::bits::HasSingleBit(slot_span_alignment));
  static_assert(!ContainsFlags(
      flags, AllocFlags::kMemoryShouldBeTaggedForMte));  // Internal only.

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  if constexpr (!ContainsFlags(flags, AllocFlags::kNoMemoryToolOverride)) {
    if (!PartitionRoot::AllocWithMemoryToolProlog<flags>(requested_size)) {
      // Early return if AllocWithMemoryToolProlog returns false
      return nullptr;
    }
    void* result = nullptr;
    // Taken from base::AlignedAlloc implementation.
    if constexpr (ContainsFlags(flags,
                                AllocFlags::kAlignedAllocForMemoryTool)) {
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
      result = _aligned_malloc(requested_size, slot_span_alignment);
#elif PA_BUILDFLAG(IS_ANDROID)
      // Android technically supports posix_memalign(), but does not expose it
      // in the current version of the library headers used by Chromium.
      // Luckily, memalign() on Android returns pointers which can safely be
      // used with free(), so we can use it instead.  Issue filed to document
      // this: http://code.google.com/p/android/issues/detail?id=35391
      result = memalign(slot_span_alignment, requested_size);
#else
      int ret = posix_memalign(&result, slot_span_alignment, requested_size);
      if (ret != 0) {
        result = nullptr;
      }
#endif  // PA_BUILDFLAG(PA_COMPILER_MSVC)
      // Aligned alloc functions don't have the `calloc` behavior of zeroing
      // the allocated memory, so we need to do it manually.
      if constexpr (ContainsFlags(flags, AllocFlags::kZeroFill)) {
        if (result) {
          // SAFETY: `result` is non-null and `requested_size` is the size of
          // the allocation, so this is a valid range to zero out.
          PA_UNSAFE_BUFFERS(memset(result, 0, requested_size));
        }
      }
    } else {
      constexpr bool zero_fill = ContainsFlags(flags, AllocFlags::kZeroFill);
      result = zero_fill ? calloc(1, requested_size) : malloc(requested_size);
    }
    if constexpr (!ContainsFlags(flags, AllocFlags::kReturnNull)) {
      PA_CHECK(result);
    }
    return result;
  }
#else   // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  // If `MEMORY_TOOL_REPLACES_ALLOCATOR` is not defined,
  // `kAlignedAllocForMemoryTool` should not be passed to `AllocInternal`.
  static_assert(!ContainsFlags(flags, AllocFlags::kAlignedAllocForMemoryTool));
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

  constexpr bool no_hooks = ContainsFlags(flags, AllocFlags::kNoHooks);
  bool hooks_enabled;

  if constexpr (!no_hooks) {
    PA_DCHECK(initialized_);
    void* object = nullptr;
    hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
    if (hooks_enabled) {
      auto additional_flags = AllocFlags::kNone;
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
      if (IsMemoryTaggingEnabled()) {
        additional_flags |= AllocFlags::kMemoryShouldBeTaggedForMte;
      }
#endif
      // The override hooks will return false if it can't handle the request,
      // i.e. due to unsupported flags. In this case, we forward the allocation
      // request to the default mechanisms.
      // TODO(crbug.com/40152647): See if we can make the forwarding more
      // verbose to ensure that this situation doesn't go unnoticed.
      if (PartitionAllocHooks::AllocationOverrideHookIfEnabled(
              &object, flags | additional_flags, requested_size, type_name)) {
        PartitionAllocHooks::AllocationObserverHookIfEnabled(
            CreateAllocationNotificationData(object, requested_size,
                                             type_name));
        return object;
      }
    }
  }

  void* const object =
      AllocInternalNoHooks<flags>(requested_size, slot_span_alignment);

  if constexpr (!no_hooks) {
    if (hooks_enabled) [[unlikely]] {
      PartitionAllocHooks::AllocationObserverHookIfEnabled(
          CreateAllocationNotificationData(object, requested_size, type_name));
    }
  }

  return object;
}

template <AllocFlags flags>
PA_ALWAYS_INLINE void* PartitionRoot::AllocInternalNoHooks(
    size_t requested_size,
    size_t slot_span_alignment) {
  static_assert(AreValidFlags(flags));

  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/in-slot metadata management
  // - Before the "raw" allocator.
  //
  // That is, the general allocation flow is:
  // 1. Adjustment of requested size to make room for extras
  // 2. Allocation:
  //   a. Call to the thread cache, if it succeeds, go to step 3.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  // 3. Handle cookie/in-slot metadata, zero allocation if required

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  PA_CHECK(raw_size >= requested_size);  // check for overflows

  // We should avoid calling `GetBucketDistribution()` repeatedly in the
  // same function, since the bucket distribution can change underneath
  // us. If we pass this changed value to `SizeToBucketIndex()` in the
  // same allocation request, we'll get inconsistent state.
  uint16_t bucket_index =
      SizeToBucketIndex(raw_size, this->GetBucketDistribution());
  size_t usable_size;
  bool is_already_zeroed = false;
  internal::UntaggedSlotStart slot_start;
  size_t slot_size = 0;

  auto* thread_cache = GetOrCreateThreadCache();

  // Don't use thread cache if higher order alignment is requested, because the
  // thread cache will not be able to satisfy it.
  //
  // `[[likely]]`: performance-sensitive partitions use the thread cache.
  if (internal::ThreadCache::IsValid(thread_cache) &&
      slot_span_alignment <= internal::PartitionPageSize()) [[likely]] {
    // Note: getting slot_size from the thread cache rather than by
    // `buckets_[bucket_index].slot_size` to avoid touching `buckets_` on the
    // fast path.
    slot_start = thread_cache->GetFromCache(bucket_index, &slot_size);

    // `[[likely]]`: median hit rate in the thread cache is 95%, from metrics.
    if (slot_start.value()) [[likely]] {
      // This follows the logic of SlotSpanMetadata::GetUsableSize for small
      // buckets_, which is too expensive to call here.
      // Keep it in sync!
      usable_size = AdjustSizeForExtrasSubtract(slot_size);

#if PA_BUILDFLAG(DCHECKS_ARE_ON)
      // Make sure that the allocated pointer comes from the same place it would
      // for a non-thread cache allocation.
      SlotSpanMetadata* slot_span =
          SlotSpanMetadata::FromSlotStart(slot_start, this);
      PA_DCHECK(DeducedRootIsValid(slot_span));
      PA_DCHECK(slot_span->bucket == &bucket_at(bucket_index));
      PA_DCHECK(slot_span->bucket->slot_size == slot_size);
      PA_DCHECK(usable_size == GetSlotUsableSize(slot_span));
      // All large allocations must go through the RawAlloc path to correctly
      // set |usable_size|.
      PA_DCHECK(!slot_span->CanStoreRawSize());
      PA_DCHECK(!slot_span->bucket->is_direct_mapped());
#endif
    } else {
      slot_start = RawAlloc<flags>(PA_UNSAFE_TODO(buckets_ + bucket_index),
                                   raw_size, slot_span_alignment, &usable_size,
                                   &slot_size, &is_already_zeroed);
    }
  } else {
    slot_start = RawAlloc<flags>(PA_UNSAFE_TODO(buckets_ + bucket_index),
                                 raw_size, slot_span_alignment, &usable_size,
                                 &slot_size, &is_already_zeroed);
  }

  if (!slot_start.value()) [[unlikely]] {
    return nullptr;
  }

  if (internal::ThreadCache::IsValid(thread_cache)) [[likely]] {
    thread_cache->RecordAllocation(usable_size);
  }

  // Layout inside the slot:
  //   |...object...|[empty]|[cookie]|[unused]|[metadata]|
  //   <----(a)----->
  //   <--------(b)--------->
  //                        <--(c)--->   +    <---(c)---->
  //   <----(d)----->   +   <--(d)--->   +    <---(d)---->
  //   <-------------(e)------------->   +    <---(e)---->
  //   <-----------------------(f)----------------------->
  //     (a) requested_size
  //     (b) usable_size
  //     (c) extras
  //     (d) raw_size
  //     (e) utilized_slot_size
  //     (f) slot_size
  //
  // Notes:
  // - Cookie exists only in the PA_BUILDFLAG(DCHECKS_ARE_ON) case.
  // - Think of raw_size as the minimum size required internally to satisfy
  //   the allocation request (i.e. requested_size + extras)
  // - At most one "empty" or "unused" space can occur at a time. They occur
  //   when slot_size is larger than raw_size. "unused" applies only to large
  //   allocations (direct-mapped and single-slot slot spans) and "empty" only
  //   to small allocations.
  //   Why either-or, one might ask? We make an effort to put the trailing
  //   cookie as close to data as possible to catch overflows (often
  //   off-by-one), but that's possible only if we have enough space in metadata
  //   to save raw_size, i.e. only for large allocations. For small allocations,
  //   we have no other choice than putting the cookie at the very end of the
  //   slot, thus creating the "empty" space.
  // - Unlike "unused", "empty" counts towards usable_size, because the app can
  //   query for it and use this space without a need for reallocation.
  // - In-slot metadata may or may not exist in the slot. Currently it exists
  //   only when BRP is used.
  // - If slot_start is not SystemPageSize()-aligned (possible only for small
  //   allocations), in-slot metadata is stored at the end of the slot.
  //   Otherwise it is stored in a special table placed after the super page
  //   metadata. For simplicity, the space for in-slot metadata is still
  //   reserved at the end of the slot, even though redundant.

  void* object = slot_start.Tag().ToObject();

  // Add the cookie after the allocation.
#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
  if (settings_.use_cookie) {
    internal::PartitionCookieWriteValue(
        PA_UNSAFE_TODO(static_cast<unsigned char*>(object) + usable_size));
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_COOKIE)

  // Fill the region kUninitializedByte (on debug builds, if not requested to 0)
  // or 0 (if requested and not 0 already).
  constexpr bool zero_fill = ContainsFlags(flags, AllocFlags::kZeroFill);
  // `[[likely]]`: operator new() calls malloc(), not calloc().
  if constexpr (!zero_fill) {
    // memset() can be really expensive.
#if PA_BUILDFLAG(EXPENSIVE_DCHECKS_ARE_ON)
    internal::DebugMemset(object, internal::kUninitializedByte, usable_size);
#endif
  } else if (!is_already_zeroed) {
    PA_UNSAFE_TODO(memset(object, 0, usable_size));
  }

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (brp_enabled()) [[likely]] {
    auto* ref_count =
        new (InSlotMetadataPointerFromSlotStartAndSize(slot_start, slot_size))
            internal::InSlotMetadata();
#if PA_CONFIG(IN_SLOT_METADATA_STORE_REQUESTED_SIZE)
    ref_count->SetRequestedSize(requested_size);
#else
    (void)ref_count;
#endif
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  return object;
}

template <AllocFlags flags>
PA_ALWAYS_INLINE internal::UntaggedSlotStart PartitionRoot::RawAlloc(
    Bucket* bucket,
    size_t raw_size,
    size_t slot_span_alignment,
    size_t* usable_size,
    size_t* slot_size,
    bool* is_already_zeroed) {
  internal::UntaggedSlotStart slot_start;
  {
    ::partition_alloc::internal::ScopedGuard guard{
        internal::PartitionRootLock(this)};
    slot_start =
        AllocFromBucket<flags>(bucket, raw_size, slot_span_alignment,
                               usable_size, slot_size, is_already_zeroed);
  }

  if (slot_start.value()) [[likely]] {
    IncreaseTotalSizeOfAllocatedBytes(slot_start.value(), *slot_size, raw_size);
  }

  return slot_start;
}

PA_ALWAYS_INLINE size_t
PartitionRoot::GetAdjustedSizeForAlignment(size_t alignment,
                                           size_t requested_size) {
  // Aligned allocation support relies on the natural alignment guarantees of
  // PartitionAlloc. Specifically, it relies on the fact that slots within a
  // slot span are aligned to slot size, from the beginning of the span.
  //
  // For alignments <=PartitionPageSize(), the code below adjusts the request
  // size to be a power of two, no less than alignment. Since slot spans are
  // aligned to PartitionPageSize(), which is also a power of two, this will
  // automatically guarantee alignment on the adjusted size boundary, thanks to
  // the natural alignment described above.
  //
  // For alignments >PartitionPageSize(), we need to pass the request down the
  // stack to only give us a slot span aligned to this more restrictive
  // boundary. In the current implementation, this code path will always
  // allocate a new slot span and hand us the first slot, so no need to adjust
  // the request size. As a consequence, allocating many small objects with
  // such a high alignment can cause a non-negligable fragmentation,
  // particularly if these allocations are back to back.
  // TODO(bartekn): We should check that this is not causing issues in practice.
  //
  // This relies on the fact that there are no extras before the allocation, as
  // they'd shift the returned allocation from the beginning of the slot, thus
  // messing up alignment. Extras after the allocation are acceptable, but they
  // have to be taken into account in the request size calculation to avoid
  // crbug.com/1185484.

  // This is mandated by |posix_memalign()|, so should never fire.
  PA_CHECK(internal::base::bits::HasSingleBit(alignment));
  // Catch unsupported alignment requests early.
  PA_CHECK(alignment <= internal::kMaxSupportedAlignment);

  // Memory returned by the regular allocator *always* respects |kAlignment|,
  // which is a power of two, and any valid alignment is also a power of two.
  // So we can use the requested_size as is.
  if (alignment <= internal::kAlignment) {
    return requested_size;
  }
  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);

  size_t adjusted_size = requested_size;
  if (alignment <= internal::PartitionPageSize()) {
    // Handle cases such as size = 16, alignment = 64.
    // Wastes memory when a large alignment is requested with a small size, but
    // this is hard to avoid, and should not be too common.
    if (raw_size < alignment) [[unlikely]] {
      raw_size = alignment;
    } else {
      // PartitionAlloc only guarantees alignment for power-of-two sized
      // allocations. To make sure this applies here, round up the allocation
      // size.
      raw_size =
          static_cast<size_t>(1)
          << (int{sizeof(size_t) * 8} -
              partition_alloc::internal::base::bits::CountlZero(raw_size - 1));
    }
    PA_DCHECK(internal::base::bits::HasSingleBit(raw_size));
    // Adjust back, because AllocInternalNoHooks/Alloc will adjust it again.
    adjusted_size = AdjustSizeForExtrasSubtract(raw_size);
  }
  return adjusted_size;
}

template <AllocFlags flags>
PA_ALWAYS_INLINE void* PartitionRoot::AlignedAllocInline(
    size_t alignment,
    size_t requested_size) {
  auto adjusted_size = GetAdjustedSizeForAlignment(alignment, requested_size);

  // Overflow check. adjusted_size must be larger or equal to requested_size.
  if (adjusted_size < requested_size) [[unlikely]] {
    if constexpr (ContainsFlags(flags, AllocFlags::kReturnNull)) {
      return nullptr;
    }
    // OutOfMemoryDeathTest.AlignedAlloc requires
    // base::TerminateBecauseOutOfMemory (invoked by
    // PartitionExcessiveAllocationSize).
    internal::PartitionExcessiveAllocationSize(requested_size);
    // internal::PartitionExcessiveAllocationSize(size) causes OOM_CRASH.
    PA_NOTREACHED();
  }

  // Slot spans are naturally aligned on partition page size, but make sure you
  // don't pass anything less, because it'll mess up callee's calculations.
  size_t slot_span_alignment =
      std::max(alignment, internal::PartitionPageSize());
  constexpr AllocFlags kMaybeAlignedAllocForMemoryTool =
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
      AllocFlags::kAlignedAllocForMemoryTool;
#else
      AllocFlags::kNone;
#endif
  void* object = AllocInternal<flags | kMaybeAlignedAllocForMemoryTool>(
      adjusted_size, slot_span_alignment, nullptr);

  // |alignment| is a power of two, but the compiler doesn't necessarily know
  // that. A regular % operation is very slow, make sure to use the equivalent,
  // faster form.
  // No need to MTE-untag, as it doesn't change alignment.
  PA_CHECK(!(reinterpret_cast<uintptr_t>(object) & (alignment - 1)));

  return object;
}

template <AllocFlags alloc_flags, FreeFlags free_flags>
void* PartitionRoot::ReallocInline(void* ptr,
                                   size_t new_size,
                                   const char* type_name) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  if (!PartitionRoot::AllocWithMemoryToolProlog<alloc_flags>(new_size)) {
    // Early return if AllocWithMemoryToolProlog returns false
    return nullptr;
  }
  void* result = realloc(ptr, new_size);
  if constexpr (!ContainsFlags(alloc_flags, AllocFlags::kReturnNull)) {
    PA_CHECK(result);
  }
  return result;
#else
  if (!ptr) [[unlikely]] {
    return AllocInternal<alloc_flags>(new_size, internal::PartitionPageSize(),
                                      type_name);
  }

  if (!new_size) [[unlikely]] {
    FreeInUnknownRoot<free_flags>(ptr);
    return nullptr;
  }

  if (new_size > internal::MaxDirectMapped()) {
    if constexpr (ContainsFlags(alloc_flags, AllocFlags::kReturnNull)) {
      return nullptr;
    }
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  constexpr bool no_hooks = ContainsFlags(alloc_flags, AllocFlags::kNoHooks);
  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  bool overridden = false;
  size_t old_usable_size = 0;
  if (!no_hooks && hooks_enabled) [[unlikely]] {
    overridden = PartitionAllocHooks::ReallocOverrideHookIfEnabled(
        &old_usable_size, ptr);
  }
  if (!overridden) [[likely]] {
    // |ptr| may have been allocated in another root.
    SlotSpanMetadata* slot_span = SlotSpanMetadata::FromSlotStart(
        internal::SlotStart::Unchecked(ptr).Untag());
    auto* old_root = PartitionRoot::FromSlotSpanMetadata(slot_span);
    bool success = false;
    bool tried_in_place_for_direct_map = false;
    {
      ::partition_alloc::internal::ScopedGuard guard{
          internal::PartitionRootLock(old_root)};
      PA_CHECK(DeducedRootIsValid(slot_span));
      old_usable_size = old_root->GetSlotUsableSize(slot_span);

      if (slot_span->bucket->is_direct_mapped()) [[unlikely]] {
        tried_in_place_for_direct_map = true;
        // We may be able to perform the realloc in place by changing the
        // accessibility of memory pages and, if reducing the size, decommitting
        // them.
        success = old_root->TryReallocInPlaceForDirectMap(slot_span, new_size);
      }
    }
    if (success) {
      if (!no_hooks && hooks_enabled) [[unlikely]] {
        PartitionAllocHooks::ReallocObserverHookIfEnabled(
            CreateFreeNotificationData(ptr),
            CreateAllocationNotificationData(ptr, new_size, type_name));
      }
      return ptr;
    }

    if (!tried_in_place_for_direct_map) [[likely]] {
      if (old_root->TryReallocInPlaceForNormalBuckets(ptr, slot_span,
                                                      new_size)) {
        return ptr;
      }
    }
  }

#if PA_BUILDFLAG(REALLOC_GROWTH_FACTOR_MITIGATION)
  // Some nVidia drivers have a performance bug where they repeatedly realloc a
  // buffer with a small 4144 byte increment instead of using a growth factor to
  // amortize the cost of a memcpy. To work around this, we apply a growth
  // factor to the new size to avoid this issue. This workaround is only
  // intended to be used for Skia bots, and is not intended to be a general
  // solution.
  if (new_size > old_usable_size) {
    // 1.5x growth factor.
    // Note that in case of integer overflow, the std::max ensures that the
    // new_size is at least as large as the old_usable_size.
    new_size = std::max(new_size, old_usable_size * 3 / 2);
  }
#endif

  // This realloc cannot be resized in-place. Sadness.
  void* ret = AllocInternal<alloc_flags>(
      new_size, internal::PartitionPageSize(), type_name);
  if (!ret) {
    if constexpr (ContainsFlags(alloc_flags, AllocFlags::kReturnNull)) {
      return nullptr;
    }
    internal::PartitionExcessiveAllocationSize(new_size);
  }

  PA_UNSAFE_TODO(memcpy(ret, ptr, std::min(old_usable_size, new_size)));
  FreeInUnknownRoot<free_flags>(
      ptr);  // Implicitly protects the old ptr on MTE systems.
  return ret;
#endif
}

internal::ThreadCache* PartitionRoot::GetOrCreateThreadCache()
    PA_LOCKS_EXCLUDED(thread_cache_construction_lock_) {
  internal::ThreadCache* thread_cache = nullptr;
  if (settings_.with_thread_cache) [[likely]] {
    thread_cache = internal::ThreadCache::Get(settings_.thread_cache_index);
    if (!internal::ThreadCache::IsValid(thread_cache)) [[unlikely]] {
      thread_cache = MaybeInitThreadCache();
    }
  }
  return thread_cache;
}

internal::ThreadCache* PartitionRoot::GetThreadCache() {
  if (settings_.with_thread_cache) [[likely]] {
    return internal::ThreadCache::Get(settings_.thread_cache_index);
  }
  return nullptr;
}

internal::ThreadCache* PartitionRoot::EnsureThreadCache()
    PA_LOCKS_EXCLUDED(thread_cache_construction_lock_) {
  internal::ThreadCache* thread_cache = nullptr;
  if (settings_.with_thread_cache) [[likely]] {
    thread_cache = internal::ThreadCache::Get(settings_.thread_cache_index);
    if (!internal::ThreadCache::IsValid(thread_cache)) [[unlikely]] {
      thread_cache = ForceInitThreadCache();
    }
  }
  return thread_cache;
}

internal::SchedulerLoopQuarantineRoot&
PartitionRoot::GetSchedulerLoopQuarantineRoot() {
  return scheduler_loop_quarantine_root_;
}
}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_INTERNAL_PARTITION_ROOT_INTERNAL_H_
