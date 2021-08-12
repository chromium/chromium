// Copyright (c) 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_bucket.h"

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager.h"
#include "base/allocator/partition_allocator/oom.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/object_bitmap.h"
#include "base/bits.h"
#include "base/check.h"
#include "base/debug/alias.h"
#include "build/build_config.h"

namespace base {
namespace internal {

namespace {

template <bool thread_safe>
[[noreturn]] NOINLINE void PartitionOutOfMemoryMappingFailure(
    PartitionRoot<thread_safe>* root,
    size_t size) LOCKS_EXCLUDED(root->lock_) {
  NO_CODE_FOLDING();
  root->OutOfMemory(size);
  IMMEDIATE_CRASH();  // Not required, kept as documentation.
}

template <bool thread_safe>
[[noreturn]] NOINLINE void PartitionOutOfMemoryCommitFailure(
    PartitionRoot<thread_safe>* root,
    size_t size) LOCKS_EXCLUDED(root->lock_) {
  NO_CODE_FOLDING();
  root->OutOfMemory(size);
  IMMEDIATE_CRASH();  // Not required, kept as documentation.
}

#if !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BACKUP_REF_PTR)
// |start| has to be aligned to kSuperPageSize, but |end| doesn't. This means
// that a partial super page is allowed at the end. Since the block list uses
// kSuperPageSize granularity, a partial super page is considered blocked if
// there is a raw_ptr<T> pointing anywhere in that super page, even if doesn't
// point to that partially allocated region.
bool AreAllowedSuperPagesForBRPPool(const char* start, const char* end) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(start) % kSuperPageSize));
  for (const char* super_page = start; super_page < end;
       super_page += kSuperPageSize) {
    // If any blocked superpage is found inside the given memory region,
    // the memory region is blocked.
    if (!AddressPoolManagerBitmap::IsAllowedSuperPageForBRPPool(super_page))
      return false;
  }
  return true;
}
#endif  // !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BACKUP_REF_PTR)

// Reserves |requested_size| worth of super pages from the specified pool of the
// GigaCage. If BRP pool is requested this function will honor BRP block list.
//
// The returned pointer will be aligned to kSuperPageSize, and so
// |requested_address| should be. |requested_size| doesn't have to be, however.
//
// |requested_address| is merely a hint, which will be attempted, but easily
// given up on if doesn't work the first time.
//
// The function doesn't need to hold root->lock_ or any other locks, because:
// - It (1) reserves memory, (2) then consults AreAllowedSuperPagesForBRPPool
//   for that memory, and (3) returns the memory if
//   allowed, or unreserves and decommits if not allowed. So no other
//   overlapping region can be allocated while executing
//   AreAllowedSuperPagesForBRPPool.
// - IsAllowedSuperPageForBRPPool (used by AreAllowedSuperPagesForBRPPool) is
//   designed to not need locking.
char* ReserveMemoryFromGigaCage(pool_handle pool,
                                void* requested_address,
                                size_t requested_size) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(requested_address) % kSuperPageSize));

  char* ptr = AddressPoolManager::GetInstance()->Reserve(
      pool, requested_address, requested_size);

  // In 32-bit mode, when allocating from BRP pool, verify that the requested
  // allocation honors the block list. Find a better address otherwise.
#if !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BACKUP_REF_PTR)
  if (pool == GetBRPPool()) {
    constexpr int kMaxRandomAddressTries = 10;
    for (int i = 0; i < kMaxRandomAddressTries; ++i) {
      if (!ptr || AreAllowedSuperPagesForBRPPool(ptr, ptr + requested_size))
        break;
      AddressPoolManager::GetInstance()->UnreserveAndDecommit(pool, ptr,
                                                              requested_size);
      // No longer try to honor |requested_address|, because it didn't work for
      // us last time.
      ptr = AddressPoolManager::GetInstance()->Reserve(pool, nullptr,
                                                       requested_size);
    }

    // If the allocation attempt succeeds, we will break out of the following
    // loop immediately.
    //
    // Last resort: sequentially scan the whole 32-bit address space. The number
    // of blocked super-pages should be very small, so we expect to practically
    // never need to run the following code. Note that it may fail to find an
    // available page, e.g., when it becomes available after the scan passes
    // through it, but we accept the risk.
    for (uintptr_t ptr_to_try = kSuperPageSize; ptr_to_try != 0;
         ptr_to_try += kSuperPageSize) {
      if (!ptr || AreAllowedSuperPagesForBRPPool(ptr, ptr + requested_size))
        break;
      AddressPoolManager::GetInstance()->UnreserveAndDecommit(pool, ptr,
                                                              requested_size);
      // Reserve() can return a different pointer than attempted.
      ptr = AddressPoolManager::GetInstance()->Reserve(
          pool, reinterpret_cast<void*>(ptr_to_try), requested_size);
    }

    // If the loop ends naturally, the last allocated region hasn't been
    // verified. Do it now.
    if (ptr && !AreAllowedSuperPagesForBRPPool(ptr, ptr + requested_size)) {
      AddressPoolManager::GetInstance()->UnreserveAndDecommit(pool, ptr,
                                                              requested_size);
      ptr = nullptr;
    }
  }
#endif  // !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BACKUP_REF_PTR)

#if !defined(PA_HAS_64_BITS_POINTERS)
  // Only mark the region as belonging to the pool after it has passed the
  // blocklist check in order to avoid a potential race with destructing a
  // raw_ptr<T> object that points to non-PA memory in another thread.
  // If `MarkUsed` was called earlier, the other thread could incorrectly
  // determine that the allocation had come form PartitionAlloc.
  if (ptr)
    AddressPoolManager::GetInstance()->MarkUsed(pool, ptr, requested_size);
#endif

  PA_DCHECK(!(reinterpret_cast<uintptr_t>(ptr) % kSuperPageSize));
  return ptr;
}

template <bool thread_safe>
SlotSpanMetadata<thread_safe>* PartitionDirectMap(
    PartitionRoot<thread_safe>* root,
    int flags,
    size_t raw_size,
    size_t slot_span_alignment) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));

  // No static EXCLUSIVE_LOCKS_REQUIRED(), as the checker doesn't understand
  // scoped unlocking.
  root->lock_.AssertAcquired();

  const bool return_null = flags & PartitionAllocReturnNull;
  if (UNLIKELY(raw_size > MaxDirectMapped())) {
    if (return_null)
      return nullptr;

    // The lock is here to protect PA from:
    // 1. Concurrent calls
    // 2. Reentrant calls
    //
    // This is fine here however, as:
    // 1. Concurrency: |PartitionRoot::OutOfMemory()| never returns, so the lock
    //    will not be re-acquired, which would lead to acting on inconsistent
    //    data that could have been modified in-between releasing and acquiring
    //    it.
    // 2. Reentrancy: This is why we release the lock. On some platforms,
    //    terminating the process may free() memory, or even possibly try to
    //    allocate some. Calling free() is fine, but will deadlock since
    //    |PartitionRoot::lock_| is not recursive.
    //
    // Supporting reentrant calls properly is hard, and not a requirement for
    // PA. However up to that point, we've only *read* data, not *written* to
    // any state. Reentrant calls are then fine, especially as we don't continue
    // on this path. The only downside is possibly endless recursion if the OOM
    // handler allocates and fails to use UncheckedMalloc() or equivalent, but
    // that's violating the contract of base::TerminateBecauseOutOfMemory().
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    PartitionExcessiveAllocationSize(raw_size);
  }

  PartitionDirectMapExtent<thread_safe>* map_extent = nullptr;
  PartitionPage<thread_safe>* page = nullptr;

  {
    // Getting memory for direct-mapped allocations doesn't interact with the
    // rest of the allocator, but takes a long time, as it involves several
    // system calls. With GigaCage, no mmap() (or equivalent) call is made on 64
    // bit systems, but page permissions are changed with mprotect(), which is a
    // syscall.
    //
    // These calls are almost always slow (at least a couple us per syscall on a
    // desktop Linux machine), and they also have a very long latency tail,
    // possibly from getting descheduled. As a consequence, we should not hold
    // the lock when performing a syscall. This is not the only problematic
    // location, but since this one doesn't interact with the rest of the
    // allocator, we can safely drop and then re-acquire the lock.
    //
    // Note that this only affects allocations that are not served out of the
    // thread cache, but as a simple example the buffer partition in blink is
    // frequently used for large allocations (e.g. ArrayBuffer), and frequent,
    // small ones (e.g. WTF::String), and does not have a thread cache.
    ScopedUnlockGuard<thread_safe> scoped_unlock{root->lock_};

    const size_t slot_size =
        PartitionRoot<thread_safe>::GetDirectMapSlotSize(raw_size);
    // The super page starts with a partition page worth of metadata and guard
    // pages, hence alignment requests ==PartitionPageSize() will be
    // automatically satisfied. Padding is needed for higher-order alignment
    // requests. Note, |slot_span_alignment| is at least 1 partition page.
    const size_t padding_for_alignment =
        slot_span_alignment - PartitionPageSize();
    const size_t reservation_size =
        PartitionRoot<thread_safe>::GetDirectMapReservationSize(
            raw_size + padding_for_alignment);
#if DCHECK_IS_ON()
    const size_t available_reservation_size =
        reservation_size - padding_for_alignment -
        PartitionRoot<thread_safe>::GetDirectMapMetadataAndGuardPagesSize();
    PA_DCHECK(slot_size <= available_reservation_size);
#endif

    // Allocate from GigaCage. Route to the appropriate GigaCage pool based on
    // BackupRefPtr support.
    pool_handle pool = root->ChooseGigaCagePool(/* is_direct_map= */ true);
    char* reservation_start =
        ReserveMemoryFromGigaCage(pool, nullptr, reservation_size);
    if (UNLIKELY(!reservation_start)) {
      if (return_null)
        return nullptr;

      PartitionOutOfMemoryMappingFailure(root, reservation_size);
    }

    root->total_size_of_direct_mapped_pages.fetch_add(
        reservation_size, std::memory_order_relaxed);

    // Shift by 1 partition page (metadata + guard pages) and alignment padding.
    char* const slot_start =
        reservation_start + PartitionPageSize() + padding_for_alignment;
    RecommitSystemPages(
        reservation_start + SystemPageSize(),
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
        // If PUT_REF_COUNT_IN_PREVIOUS_SLOT is on, and if the BRP pool is used,
        // allocate 2 SystemPages, one for SuperPage metadata and the other for
        // RefCount "bitmap" (only one of its elements will be used).
        (pool == GetBRPPool()) ? SystemPageSize() * 2 : SystemPageSize(),
#else
        SystemPageSize(),
#endif
        PageReadWrite, PageUpdatePermissions);

    // No need to hold root->lock_. Now that memory is reserved, no other
    // overlapping region can be allocated (because of how GigaCage works),
    // so no other thread can update the same offset table entries at the
    // same time. Furthermore, nobody will be ready these offsets until this
    // function returns.
    uintptr_t ptr_start = reinterpret_cast<uintptr_t>(reservation_start);
    uintptr_t ptr_end = ptr_start + reservation_size;
    auto* offset_ptr = ReservationOffsetPointer(ptr_start);
    int offset = 0;
    while (ptr_start < ptr_end) {
      PA_DCHECK(offset_ptr < GetReservationOffsetTableEnd());
      PA_DCHECK(offset < kOffsetTagNormalBuckets);
      *offset_ptr++ = offset++;
      ptr_start += kSuperPageSize;
    }

    auto* super_page_extent =
        reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
            PartitionSuperPageToMetadataArea(reservation_start));
    super_page_extent->root = root;
    // The new structures are all located inside a fresh system page so they
    // will all be zeroed out. These DCHECKs are for documentation and to assert
    // our expectations of the kernel.
    PA_DCHECK(!super_page_extent->number_of_consecutive_super_pages);
    PA_DCHECK(!super_page_extent->next);

    PartitionPage<thread_safe>* first_page =
        reinterpret_cast<PartitionPage<thread_safe>*>(super_page_extent) + 1;
    page = PartitionPage<thread_safe>::FromPtr(slot_start);
    // |first_page| and |page| may be equal, if there is no alignment padding.
    if (page != first_page) {
      PA_DCHECK(page > first_page);
      PA_DCHECK(page - first_page <=
                PartitionPage<thread_safe>::kMaxSlotSpanMetadataOffset);
      PA_CHECK(!first_page->is_valid);
      first_page->has_valid_span_after_this = true;
      first_page->slot_span_metadata_offset = page - first_page;
    }
    auto* metadata =
        reinterpret_cast<PartitionDirectMapMetadata<thread_safe>*>(page);
    // Since direct map metadata is larger than PartitionPage, make sure the
    // first and the last bytes are on the same system page, i.e. within the
    // super page metadata region.
    PA_DCHECK(
        bits::AlignDown(reinterpret_cast<char*>(metadata), SystemPageSize()) ==
        bits::AlignDown(reinterpret_cast<char*>(metadata) +
                            sizeof(PartitionDirectMapMetadata<thread_safe>) - 1,
                        SystemPageSize()));
    PA_DCHECK(page == &metadata->page);
    page->is_valid = true;
    PA_DCHECK(!page->has_valid_span_after_this);
    PA_DCHECK(!page->slot_span_metadata_offset);
    PA_DCHECK(!page->slot_span_metadata.next_slot_span);
    PA_DCHECK(!page->slot_span_metadata.num_allocated_slots);
    PA_DCHECK(!page->slot_span_metadata.num_unprovisioned_slots);
    PA_DCHECK(!page->slot_span_metadata.empty_cache_index);

    PA_DCHECK(!metadata->subsequent_page.subsequent_page_metadata.raw_size);
    // Raw size is set later, by the caller.
    metadata->subsequent_page.slot_span_metadata_offset = 1;

    PA_DCHECK(!metadata->bucket.active_slot_spans_head);
    PA_DCHECK(!metadata->bucket.empty_slot_spans_head);
    PA_DCHECK(!metadata->bucket.decommitted_slot_spans_head);
    PA_DCHECK(!metadata->bucket.num_system_pages_per_slot_span);
    PA_DCHECK(!metadata->bucket.num_full_slot_spans);
    metadata->bucket.slot_size = slot_size;

    new (&page->slot_span_metadata)
        SlotSpanMetadata<thread_safe>(&metadata->bucket);

    // It is typically possible to map a large range of inaccessible pages, and
    // this is leveraged in multiple places, including the GigaCage. However,
    // this doesn't mean that we can commit all this memory.  For the vast
    // majority of allocations, this just means that we crash in a slightly
    // different place, but for callers ready to handle failures, we have to
    // return nullptr. See crbug.com/1187404.
    //
    // Note that we didn't check above, because if we cannot even commit a
    // single page, then this is likely hopeless anyway, and we will crash very
    // soon.
    const bool ok = root->TryRecommitSystemPagesForData(
        &page->slot_span_metadata, slot_start, slot_size,
        PageUpdatePermissions);
    if (!ok) {
      if (!return_null) {
        PartitionOutOfMemoryCommitFailure(root, slot_size);
      }

#if !defined(PA_HAS_64_BITS_POINTERS)
      AddressPoolManager::GetInstance()->MarkUnused(pool, reservation_start,
                                                    reservation_size);
#endif
      AddressPoolManager::GetInstance()->UnreserveAndDecommit(
          pool, reservation_start, reservation_size);

      root->total_size_of_direct_mapped_pages.fetch_sub(
          reservation_size, std::memory_order_relaxed);

      return nullptr;
    }

    auto* next_entry = new (slot_start) PartitionFreelistEntry();
    page->slot_span_metadata.SetFreelistHead(next_entry);

    map_extent = &metadata->direct_map_extent;
    map_extent->reservation_size = reservation_size;
    map_extent->padding_for_alignment = padding_for_alignment;
    map_extent->bucket = &metadata->bucket;
  }

  root->lock_.AssertAcquired();

  // Maintain the doubly-linked list of all direct mappings.
  map_extent->next_extent = root->direct_map_list;
  if (map_extent->next_extent)
    map_extent->next_extent->prev_extent = map_extent;
  map_extent->prev_extent = nullptr;
  root->direct_map_list = map_extent;

  return &page->slot_span_metadata;
}

}  // namespace

// TODO(ajwong): This seems to interact badly with
// get_pages_per_slot_span() which rounds the value from this up to a
// multiple of NumSystemPagesPerPartitionPage() (aka 4) anyways.
// http://crbug.com/776537
//
// TODO(ajwong): The waste calculation seems wrong. The PTE usage should cover
// both used and unsed pages.
// http://crbug.com/776537
template <bool thread_safe>
uint8_t PartitionBucket<thread_safe>::get_system_pages_per_slot_span() {
  // This works out reasonably for the current bucket sizes of the generic
  // allocator, and the current values of partition page size and constants.
  // Specifically, we have enough room to always pack the slots perfectly into
  // some number of system pages. The only waste is the waste associated with
  // unfaulted pages (i.e. wasted address space).
  // TODO: we end up using a lot of system pages for very small sizes. For
  // example, we'll use 12 system pages for slot size 24. The slot size is
  // so small that the waste would be tiny with just 4, or 1, system pages.
  // Later, we can investigate whether there are anti-fragmentation benefits
  // to using fewer system pages.
  double best_waste_ratio = 1.0f;
  uint16_t best_pages = 0;
  if (slot_size > MaxRegularSlotSpanSize()) {
    // TODO(ajwong): Why is there a DCHECK here for this?
    // http://crbug.com/776537
    PA_DCHECK(!(slot_size % SystemPageSize()));
    best_pages = static_cast<uint16_t>(slot_size >> SystemPageShift());
    PA_CHECK(best_pages <= std::numeric_limits<uint8_t>::max());
    return static_cast<uint8_t>(best_pages);
  }
  PA_DCHECK(slot_size <= MaxRegularSlotSpanSize());
  for (uint16_t i = NumSystemPagesPerPartitionPage() - 1;
       i <= MaxSystemPagesPerRegularSlotSpan(); ++i) {
    size_t page_size = i << SystemPageShift();
    size_t num_slots = page_size / slot_size;
    size_t waste = page_size - (num_slots * slot_size);
    // Leaving a page unfaulted is not free; the page will occupy an empty page
    // table entry.  Make a simple attempt to account for that.
    //
    // TODO(ajwong): This looks wrong. PTEs are allocated for all pages
    // regardless of whether or not they are wasted. Should it just
    // be waste += i * sizeof(void*)?
    // http://crbug.com/776537
    size_t num_remainder_pages = i & (NumSystemPagesPerPartitionPage() - 1);
    size_t num_unfaulted_pages =
        num_remainder_pages
            ? (NumSystemPagesPerPartitionPage() - num_remainder_pages)
            : 0;
    waste += sizeof(void*) * num_unfaulted_pages;
    double waste_ratio =
        static_cast<double>(waste) / static_cast<double>(page_size);
    if (waste_ratio < best_waste_ratio) {
      best_waste_ratio = waste_ratio;
      best_pages = i;
    }
  }
  PA_DCHECK(best_pages > 0);
  PA_CHECK(best_pages <= MaxSystemPagesPerRegularSlotSpan());
  return static_cast<uint8_t>(best_pages);
}

template <bool thread_safe>
void PartitionBucket<thread_safe>::Init(uint32_t new_slot_size) {
  slot_size = new_slot_size;
  slot_size_reciprocal = kReciprocalMask / new_slot_size + 1;
  active_slot_spans_head =
      SlotSpanMetadata<thread_safe>::get_sentinel_slot_span();
  empty_slot_spans_head = nullptr;
  decommitted_slot_spans_head = nullptr;
  num_full_slot_spans = 0;
  num_system_pages_per_slot_span = get_system_pages_per_slot_span();
}

template <bool thread_safe>
NOINLINE void PartitionBucket<thread_safe>::OnFull() {
  OOM_CRASH(0);
}

template <bool thread_safe>
ALWAYS_INLINE SlotSpanMetadata<thread_safe>*
PartitionBucket<thread_safe>::AllocNewSlotSpan(PartitionRoot<thread_safe>* root,
                                               int flags,
                                               size_t slot_span_alignment) {
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page) %
              PartitionPageSize()));
  PA_DCHECK(!(reinterpret_cast<uintptr_t>(root->next_partition_page_end) %
              PartitionPageSize()));

  size_t num_partition_pages = get_pages_per_slot_span();
  size_t slot_span_reservation_size = num_partition_pages
                                      << PartitionPageShift();
  size_t slot_span_committed_size = get_bytes_per_span();
  PA_DCHECK(num_partition_pages <= NumPartitionPagesPerSuperPage());
  PA_DCHECK(slot_span_committed_size % SystemPageSize() == 0);
  PA_DCHECK(slot_span_committed_size <= slot_span_reservation_size);

  auto adjusted_next_partition_page =
      bits::AlignUp(root->next_partition_page, slot_span_alignment);
  if (UNLIKELY(adjusted_next_partition_page + slot_span_reservation_size >
               root->next_partition_page_end)) {
    // In this case, we can no longer hand out pages from the current super page
    // allocation. Get a new super page.
    if (!AllocNewSuperPage(root)) {
      return nullptr;
    }
    // AllocNewSuperPage() updates root->next_partition_page, re-query.
    adjusted_next_partition_page =
        bits::AlignUp(root->next_partition_page, slot_span_alignment);
    PA_CHECK(adjusted_next_partition_page + slot_span_reservation_size <=
             root->next_partition_page_end);
  }

  auto* gap_start_page =
      PartitionPage<thread_safe>::FromPtr(root->next_partition_page);
  auto* gap_end_page =
      PartitionPage<thread_safe>::FromPtr(adjusted_next_partition_page);
  for (auto* page = gap_start_page; page < gap_end_page; ++page) {
    PA_DCHECK(!page->is_valid);
    page->has_valid_span_after_this = 1;
  }
  root->next_partition_page =
      adjusted_next_partition_page + slot_span_reservation_size;

  void* slot_span_start = adjusted_next_partition_page;
  auto* slot_span = &gap_end_page->slot_span_metadata;
  InitializeSlotSpan(slot_span);
  // Now that slot span is initialized, it's safe to call FromSlotStartPtr.
  PA_DCHECK(slot_span ==
            SlotSpanMetadata<thread_safe>::FromSlotStartPtr(slot_span_start));

  // System pages in the super page come in a decommited state. Commit them
  // before vending them back.
  // If lazy commit is enabled, pages will be committed when provisioning slots,
  // in ProvisionMoreSlotsAndAllocOne(), not here.
  if (!root->use_lazy_commit) {
    PA_DCHECK(slot_span->GetPreviouslyCommittedSize() == 0);
    root->RecommitSystemPagesForData(slot_span, slot_span_start,
                                     slot_span_committed_size,
                                     PageUpdatePermissions);
    PA_DCHECK(slot_span->GetPreviouslyCommittedSize() ==
              slot_span_committed_size);
  }

  // Double check that we had enough space in the super page for the new slot
  // span.
  PA_DCHECK(root->next_partition_page <= root->next_partition_page_end);
  return slot_span;
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionBucket<thread_safe>::AllocNewSuperPage(
    PartitionRoot<thread_safe>* root) {
  // Need a new super page. We want to allocate super pages in a contiguous
  // address region as much as possible. This is important for not causing
  // page table bloat and not fragmenting address spaces in 32 bit
  // architectures.
  char* requested_address = root->next_super_page;
  // Allocate from GigaCage. Route to the appropriate GigaCage pool based on
  // BackupRefPtr support.
  pool_handle pool = root->ChooseGigaCagePool(/* is_direct_map= */ false);
  char* super_page =
      ReserveMemoryFromGigaCage(pool, requested_address, kSuperPageSize);
  if (UNLIKELY(!super_page))
    return nullptr;

  *ReservationOffsetPointer(reinterpret_cast<uintptr_t>(super_page)) =
      kOffsetTagNormalBuckets;

  root->total_size_of_super_pages.fetch_add(kSuperPageSize,
                                            std::memory_order_relaxed);

  root->next_super_page = super_page + kSuperPageSize;
  char* quarantine_bitmaps = super_page + PartitionPageSize();
  const size_t quarantine_bitmaps_reservation_size =
      root->IsQuarantineAllowed() ? ReservedQuarantineBitmapsSize() : 0;
  const size_t quarantine_bitmaps_size_to_commit =
      root->IsQuarantineAllowed() ? CommittedQuarantineBitmapsSize() : 0;
  PA_DCHECK(quarantine_bitmaps_reservation_size % PartitionPageSize() == 0);
  PA_DCHECK(quarantine_bitmaps_size_to_commit % SystemPageSize() == 0);
  PA_DCHECK(quarantine_bitmaps_size_to_commit <=
            quarantine_bitmaps_reservation_size);
  char* ret = quarantine_bitmaps + quarantine_bitmaps_reservation_size;
  root->next_partition_page = ret;
  root->next_partition_page_end = root->next_super_page - PartitionPageSize();
  PA_DCHECK(ret ==
            SuperPagePayloadBegin(super_page, root->IsQuarantineAllowed()));
  PA_DCHECK(root->next_partition_page_end == SuperPagePayloadEnd(super_page));

  // Keep the first partition page in the super page inaccessible to serve as a
  // guard page, except an "island" in the middle where we put page metadata and
  // also a tiny amount of extent metadata.
  RecommitSystemPages(
      super_page + SystemPageSize(),
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
      // If PUT_REF_COUNT_IN_PREVIOUS_SLOT is on, and if the BRP pool is used,
      // allocate 2 SystemPages, one for SuperPage metadata and the other for
      // RefCount bitmap.
      (pool == GetBRPPool()) ? SystemPageSize() * 2 : SystemPageSize(),
#else
      SystemPageSize(),
#endif
      PageReadWrite, PageUpdatePermissions);

  // If PCScan is used, commit the quarantine bitmap. Otherwise, leave it
  // uncommitted and let PartitionRoot::EnablePCScan commit it when needed.
  if (root->IsQuarantineEnabled()) {
    RecommitSystemPages(quarantine_bitmaps, quarantine_bitmaps_size_to_commit,
                        PageReadWrite, PageUpdatePermissions);
    PCScan::RegisterNewSuperPage(root, reinterpret_cast<uintptr_t>(super_page));
  }

  // If we were after a specific address, but didn't get it, assume that
  // the system chose a lousy address. Here most OS'es have a default
  // algorithm that isn't randomized. For example, most Linux
  // distributions will allocate the mapping directly before the last
  // successful mapping, which is far from random. So we just get fresh
  // randomness for the next mapping attempt.
  if (requested_address && requested_address != super_page)
    root->next_super_page = nullptr;

  // We allocated a new super page so update super page metadata.
  // First check if this is a new extent or not.
  auto* latest_extent =
      reinterpret_cast<PartitionSuperPageExtentEntry<thread_safe>*>(
          PartitionSuperPageToMetadataArea(super_page));
  // By storing the root in every extent metadata object, we have a fast way
  // to go from a pointer within the partition to the root object.
  latest_extent->root = root;
  // Most new extents will be part of a larger extent, and these two fields
  // are unused, but we initialize them to 0 so that we get a clear signal
  // in case they are accidentally used.
  latest_extent->number_of_consecutive_super_pages = 0;
  latest_extent->next = nullptr;
  latest_extent->number_of_nonempty_slot_spans = 0;

  PartitionSuperPageExtentEntry<thread_safe>* current_extent =
      root->current_extent;
  const bool is_new_extent = super_page != requested_address;
  if (UNLIKELY(is_new_extent)) {
    if (UNLIKELY(!current_extent)) {
      PA_DCHECK(!root->first_extent);
      root->first_extent = latest_extent;
    } else {
      PA_DCHECK(current_extent->number_of_consecutive_super_pages);
      current_extent->next = latest_extent;
    }
    root->current_extent = latest_extent;
    latest_extent->number_of_consecutive_super_pages = 1;
  } else {
    // We allocated next to an existing extent so just nudge the size up a
    // little.
    PA_DCHECK(current_extent->number_of_consecutive_super_pages);
    ++current_extent->number_of_consecutive_super_pages;
    PA_DCHECK(ret > SuperPagesBeginFromExtent(current_extent) &&
              ret < SuperPagesEndFromExtent(current_extent));
  }
  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionBucket<thread_safe>::InitializeSlotSpan(
    SlotSpanMetadata<thread_safe>* slot_span) {
  new (slot_span) SlotSpanMetadata<thread_safe>(this);
  slot_span->empty_cache_index = -1;

  slot_span->Reset();

  uint16_t num_partition_pages = get_pages_per_slot_span();
  auto* page = reinterpret_cast<PartitionPage<thread_safe>*>(slot_span);
  for (uint16_t i = 0; i < num_partition_pages; ++i, ++page) {
    PA_DCHECK(i <= PartitionPage<thread_safe>::kMaxSlotSpanMetadataOffset);
    page->slot_span_metadata_offset = i;
    page->is_valid = true;
  }
}

template <bool thread_safe>
ALWAYS_INLINE char* PartitionBucket<thread_safe>::ProvisionMoreSlotsAndAllocOne(
    PartitionRoot<thread_safe>* root,
    SlotSpanMetadata<thread_safe>* slot_span) {
  PA_DCHECK(slot_span !=
            SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
  uint16_t num_slots = slot_span->num_unprovisioned_slots;
  PA_DCHECK(num_slots);
  // We should only get here when _every_ slot is either used or unprovisioned.
  // (The third state is "on the freelist". If we have a non-empty freelist, we
  // should not get here.)
  PA_DCHECK(num_slots + slot_span->num_allocated_slots == get_slots_per_span());
  // Similarly, make explicitly sure that the freelist is empty.
  PA_DCHECK(!slot_span->freelist_head);
  PA_DCHECK(slot_span->num_allocated_slots >= 0);

  size_t size = slot_size;
  char* base = reinterpret_cast<char*>(
      SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  // If we got here, the first unallocated slot is either partially or fully on
  // an uncommitted page. If the latter, it must be at the start of that page.
  char* return_slot = base + (size * slot_span->num_allocated_slots);
  char* next_slot = return_slot + size;
  char* commit_start = bits::AlignUp(return_slot, SystemPageSize());
  PA_DCHECK(next_slot > commit_start);
  char* commit_end = bits::AlignUp(next_slot, SystemPageSize());
  // If the slot was partially committed, |return_slot| and |next_slot| fall
  // in different pages. If the slot was fully uncommitted, |return_slot| points
  // to the page start and |next_slot| doesn't, thus only the latter gets
  // rounded up.
  PA_DCHECK(commit_end > commit_start);

  // The slot being returned is considered allocated.
  slot_span->num_allocated_slots++;
  // Round down, because a slot that doesn't fully fit in the new page(s) isn't
  // provisioned.
  uint16_t slots_to_provision = (commit_end - return_slot) / size;
  slot_span->num_unprovisioned_slots -= slots_to_provision;
  PA_DCHECK(slot_span->num_allocated_slots +
                slot_span->num_unprovisioned_slots <=
            get_slots_per_span());

  // If lazy commit is enabled, meaning system pages in the slot span come
  // in an initially decommitted state, commit them here.
  // Note, we can't use PageKeepPermissionsIfPossible, because we have no
  // knowledge which pages have been committed before (it doesn't matter on
  // Windows anyway).
  if (root->use_lazy_commit) {
    size_t previously_committed_size = slot_span->GetPreviouslyCommittedSize();
    char* previously_committed_watermark = base + previously_committed_size;
    // It shouldn't be possible for watermark to fall in between start and end.
    // Obviously, watermark has to be at least as far as start.
    // TODO(lizeb): Handle commit failure.
    PA_DCHECK(previously_committed_watermark == commit_start ||
              previously_committed_watermark >= commit_end);
    root->RecommitSystemPagesForData(
        slot_span, commit_start, commit_end - commit_start,
        (previously_committed_watermark == commit_start)
            ? PageUpdatePermissions
            : PageKeepPermissionsIfPossible);
    PA_DCHECK(
        slot_span->GetPreviouslyCommittedSize() >=
        bits::AlignUp(
            (get_slots_per_span() - slot_span->num_unprovisioned_slots) * size,
            SystemPageSize()));
  }

  // Add all slots that fit within so far committed pages to the free list.
  PartitionFreelistEntry* prev_entry = nullptr;
  char* next_slot_end = next_slot + size;
  size_t free_list_entries_added = 0;
  while (next_slot_end <= commit_end) {
    auto* entry = new (next_slot) PartitionFreelistEntry();
    if (!slot_span->freelist_head) {
      PA_DCHECK(!prev_entry);
      PA_DCHECK(!free_list_entries_added);
      slot_span->SetFreelistHead(entry);
    } else {
      PA_DCHECK(free_list_entries_added);
      prev_entry->SetNext(entry);
    }
    next_slot = next_slot_end;
    next_slot_end = next_slot + size;
    prev_entry = entry;
#if DCHECK_IS_ON()
    free_list_entries_added++;
#endif
  }

#if DCHECK_IS_ON()
  // The only provisioned slot not added to the free list is the one being
  // returned.
  PA_DCHECK(slots_to_provision == free_list_entries_added + 1);
  // We didn't necessarily provision more than one slot (e.g. if |slot_size|
  // is large), meaning that |slot_span->freelist_head| can be nullptr.
  if (slot_span->freelist_head) {
    PA_DCHECK(free_list_entries_added);
    slot_span->freelist_head->CheckFreeList(slot_size);
  }
#endif

  return return_slot;
}

template <bool thread_safe>
bool PartitionBucket<thread_safe>::SetNewActiveSlotSpan() {
  SlotSpanMetadata<thread_safe>* slot_span = active_slot_spans_head;
  if (slot_span == SlotSpanMetadata<thread_safe>::get_sentinel_slot_span())
    return false;

  SlotSpanMetadata<thread_safe>* next_slot_span;

  for (; slot_span; slot_span = next_slot_span) {
    next_slot_span = slot_span->next_slot_span;
    PA_DCHECK(slot_span->bucket == this);
    PA_DCHECK(slot_span != empty_slot_spans_head);
    PA_DCHECK(slot_span != decommitted_slot_spans_head);

    if (LIKELY(slot_span->is_active())) {
      // This slot span is usable because it has freelist entries, or has
      // unprovisioned slots we can create freelist entries from.
      active_slot_spans_head = slot_span;
      return true;
    }

    // Deal with empty and decommitted slot spans.
    if (LIKELY(slot_span->is_empty())) {
      slot_span->next_slot_span = empty_slot_spans_head;
      empty_slot_spans_head = slot_span;
    } else if (LIKELY(slot_span->is_decommitted())) {
      slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = slot_span;
    } else {
      PA_DCHECK(slot_span->is_full());
      // If we get here, we found a full slot span. Skip over it too, and also
      // mark it as full (via a negative value). We need it marked so that
      // free'ing can tell, and move it back into the active list.
      slot_span->num_allocated_slots = -slot_span->num_allocated_slots;
      ++num_full_slot_spans;
      // num_full_slot_spans is a uint16_t for efficient packing so guard
      // against overflow to be safe.
      if (UNLIKELY(!num_full_slot_spans))
        OnFull();
      // Not necessary but might help stop accidents.
      slot_span->next_slot_span = nullptr;
    }
  }

  active_slot_spans_head =
      SlotSpanMetadata<thread_safe>::get_sentinel_slot_span();
  return false;
}

template <bool thread_safe>
void* PartitionBucket<thread_safe>::SlowPathAlloc(
    PartitionRoot<thread_safe>* root,
    int flags,
    size_t raw_size,
    size_t slot_span_alignment,
    bool* is_already_zeroed) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));

  // The slow path is called when the freelist is empty. The only exception is
  // when a higher-order alignment is requested, in which case the freelist
  // logic is bypassed and we go directly for slot span allocation.
  bool allocate_aligned_slot_span = slot_span_alignment > PartitionPageSize();
  PA_DCHECK(!active_slot_spans_head->freelist_head ||
            allocate_aligned_slot_span);

  SlotSpanMetadata<thread_safe>* new_slot_span = nullptr;
  // |new_slot_span->bucket| will always be |this|, except when |this| is the
  // sentinel bucket, which is used to signal a direct mapped allocation.  In
  // this case |new_bucket| will be set properly later. This avoids a read for
  // most allocations.
  PartitionBucket* new_bucket = this;
  *is_already_zeroed = false;

  // For the PartitionRoot::Alloc() API, we have a bunch of buckets
  // marked as special cases. We bounce them through to the slow path so that
  // we can still have a blazing fast hot path due to lack of corner-case
  // branches.
  //
  // Note: The ordering of the conditionals matter! In particular,
  // SetNewActiveSlotSpan() has a side-effect even when returning
  // false where it sweeps the active list and may move things into the empty or
  // decommitted lists which affects the subsequent conditional.
  if (UNLIKELY(is_direct_mapped())) {
    PA_DCHECK(raw_size > kMaxBucketed);
    PA_DCHECK(this == &root->sentinel_bucket);
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());

    // No fast path for direct-mapped allocations.
    if (flags & PartitionAllocFastPathOrReturnNull)
      return nullptr;

    new_slot_span =
        PartitionDirectMap(root, flags, raw_size, slot_span_alignment);
    if (new_slot_span)
      new_bucket = new_slot_span->bucket;
    // Memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  } else if (LIKELY(!allocate_aligned_slot_span && SetNewActiveSlotSpan())) {
    // First, did we find an active slot span in the active list?
    new_slot_span = active_slot_spans_head;
    PA_DCHECK(new_slot_span->is_active());
  } else if (LIKELY(!allocate_aligned_slot_span &&
                    (empty_slot_spans_head != nullptr ||
                     decommitted_slot_spans_head != nullptr))) {
    // Second, look in our lists of empty and decommitted slot spans.
    // Check empty slot spans first, which are preferred, but beware that an
    // empty slot span might have been decommitted.
    while (LIKELY((new_slot_span = empty_slot_spans_head) != nullptr)) {
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_empty() || new_slot_span->is_decommitted());
      empty_slot_spans_head = new_slot_span->next_slot_span;
      // Accept the empty slot span unless it got decommitted.
      if (new_slot_span->freelist_head) {
        new_slot_span->next_slot_span = nullptr;
        new_slot_span->ToSuperPageExtent()
            ->IncrementNumberOfNonemptySlotSpans();
        break;
      }
      PA_DCHECK(new_slot_span->is_decommitted());
      new_slot_span->next_slot_span = decommitted_slot_spans_head;
      decommitted_slot_spans_head = new_slot_span;
    }
    if (UNLIKELY(!new_slot_span) &&
        LIKELY(decommitted_slot_spans_head != nullptr)) {
      // Commit can be expensive, don't do it.
      if (flags & PartitionAllocFastPathOrReturnNull)
        return nullptr;

      new_slot_span = decommitted_slot_spans_head;
      PA_DCHECK(new_slot_span->bucket == this);
      PA_DCHECK(new_slot_span->is_decommitted());
      decommitted_slot_spans_head = new_slot_span->next_slot_span;

      new_slot_span->Reset();

      // If lazy commit is enabled, pages will be recommitted when provisioning
      // slots, in ProvisionMoreSlotsAndAllocOne(), not here.
      if (!root->use_lazy_commit) {
        char* addr = reinterpret_cast<char*>(
            SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(new_slot_span));
        // We have a guarantee that all slot span memory up to
        // |GetPreviouslyCommittedSize()| has been previously committed, and
        // if decommitted later it was done using PageKeepPermissionsIfPossible,
        // so use the same option as an optimization.
        size_t previously_committed_size =
            new_slot_span->GetPreviouslyCommittedSize();
        PA_DCHECK(previously_committed_size > 0);
        // TODO(lizeb): Handle commit failure.
        root->RecommitSystemPagesForData(new_slot_span, addr,
                                         previously_committed_size,
                                         PageKeepPermissionsIfPossible);
        size_t reminder_bytes_to_commit =
            new_slot_span->bucket->get_bytes_per_span() -
            previously_committed_size;
        if (reminder_bytes_to_commit > 0) {
          // This situation can only happen if lazy commit was used before and
          // it was turned off since.
          root->RecommitSystemPagesForData(
              new_slot_span, addr + previously_committed_size,
              reminder_bytes_to_commit, PageUpdatePermissions);
        }
        PA_DCHECK(new_slot_span->GetPreviouslyCommittedSize() ==
                  new_slot_span->bucket->get_bytes_per_span());
      }

      *is_already_zeroed = DecommittedMemoryIsAlwaysZeroed();
    }
    PA_DCHECK(new_slot_span);
  } else {
    // Getting a new slot span is expensive, don't do it.
    if (flags & PartitionAllocFastPathOrReturnNull)
      return nullptr;

    // Third. If we get here, we need a brand new slot span.
    // TODO(bartekn): For single-slot slot spans, we can use rounded raw_size
    // as slot_span_committed_size.
    new_slot_span = AllocNewSlotSpan(root, flags, slot_span_alignment);
    // New memory from PageAllocator is always zeroed.
    *is_already_zeroed = true;
  }

  // Bail if we had a memory allocation failure.
  if (UNLIKELY(!new_slot_span)) {
    PA_DCHECK(active_slot_spans_head ==
              SlotSpanMetadata<thread_safe>::get_sentinel_slot_span());
    if (flags & PartitionAllocReturnNull)
      return nullptr;
    // See comment in PartitionDirectMap() for unlocking.
    ScopedUnlockGuard<thread_safe> unlock{root->lock_};
    root->OutOfMemory(raw_size);
    IMMEDIATE_CRASH();  // Not required, kept as documentation.
  }

  PA_DCHECK(new_bucket != &root->sentinel_bucket);
  new_bucket->active_slot_spans_head = new_slot_span;
  if (new_slot_span->CanStoreRawSize())
    new_slot_span->SetRawSize(raw_size);

  // If we found an active slot span with free slots, or an empty slot span, we
  // have a usable freelist head.
  if (LIKELY(new_slot_span->freelist_head != nullptr)) {
    PartitionFreelistEntry* entry = new_slot_span->freelist_head;
    PartitionFreelistEntry* new_head = entry->GetNext(slot_size);
    new_slot_span->SetFreelistHead(new_head);
    new_slot_span->num_allocated_slots++;

    // We likely set *is_already_zeroed to true above, make sure that the
    // freelist entry doesn't contain data.
    return entry->ClearForAllocation();
  }

  // Otherwise, we need to provision more slots by committing more pages. Build
  // the free list for the newly provisioned slots.
  PA_DCHECK(new_slot_span->num_unprovisioned_slots);
  return ProvisionMoreSlotsAndAllocOne(root, new_slot_span);
}

template struct PartitionBucket<ThreadSafe>;
template struct PartitionBucket<NotThreadSafe>;

}  // namespace internal
}  // namespace base
