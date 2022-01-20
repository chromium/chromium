// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_

// DESCRIPTION
// PartitionRoot::Alloc() and PartitionRoot::Free() are approximately analogous
// to malloc() and free().
//
// The main difference is that a PartitionRoot object must be supplied to these
// functions, representing a specific "heap partition" that will be used to
// satisfy the allocation. Different partitions are guaranteed to exist in
// separate address spaces, including being separate from the main system
// heap. If the contained objects are all freed, physical memory is returned to
// the system but the address space remains reserved.  See PartitionAlloc.md for
// other security properties PartitionAlloc provides.
//
// THE ONLY LEGITIMATE WAY TO OBTAIN A PartitionRoot IS THROUGH THE
// PartitionAllocator classes. To minimize the instruction count to the fullest
// extent possible, the PartitionRoot is really just a header adjacent to other
// data areas provided by the allocator class.
//
// The constraints for PartitionRoot::Alloc() are:
// - Multi-threaded use against a single partition is ok; locking is handled.
// - Allocations of any arbitrary size can be handled (subject to a limit of
//   INT_MAX bytes for security reasons).
// - Bucketing is by approximate size, for example an allocation of 4000 bytes
//   might be placed into a 4096-byte bucket. Bucket sizes are chosen to try and
//   keep worst-case waste to ~10%.

#include <algorithm>
#include <atomic>
#include <cstddef>
#include <cstdint>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/allocation_guard.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_alloc_notreached.h"
#include "base/allocator/partition_allocator/partition_bucket_lookup.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_freelist_entry.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/starscan/state_bitmap.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/compiler_specific.h"
#include "base/memory/tagging.h"
#include "base/time/time.h"
#include "build/build_config.h"
#include "build/chromecast_buildflags.h"

// We use this to make MEMORY_TOOL_REPLACES_ALLOCATOR behave the same for max
// size as other alloc code.
#define CHECK_MAX_SIZE_OR_RETURN_NULLPTR(size, flags) \
  if (size > MaxDirectMapped()) {                     \
    if (flags & PartitionAllocReturnNull) {           \
      return nullptr;                                 \
    }                                                 \
    PA_CHECK(false);                                  \
  }

namespace base {

class PartitionStatsDumper;

namespace internal {
// Avoid including partition_address_space.h from this .h file, by moving the
// call to IsManagedByPartitionAllocBRPPool into the .cc file.
#if DCHECK_IS_ON()
BASE_EXPORT void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address);
#else
ALWAYS_INLINE void DCheckIfManagedByPartitionAllocBRPPool(uintptr_t address) {}
#endif
}  // namespace internal

enum PartitionPurgeFlags {
  // Decommitting the ring list of empty slot spans is reasonably fast.
  PartitionPurgeDecommitEmptySlotSpans = 1 << 0,
  // Discarding unused system pages is slower, because it involves walking all
  // freelists in all active slot spans of all buckets >= system page
  // size. It often frees a similar amount of memory to decommitting the empty
  // slot spans, though.
  PartitionPurgeDiscardUnusedSystemPages = 1 << 1,
  // Aggressively reclaim memory. This is meant to be used in low-memory
  // situations, not for periodic memory reclaiming.
  PartitionPurgeAggressiveReclaim = 1 << 2,
};

// Options struct used to configure PartitionRoot and PartitionAllocator.
struct PartitionOptions {
  enum class AlignedAlloc : uint8_t {
    // By default all allocations will be aligned to `base::kAlignment`,
    // likely to be 8B or 16B depending on platforms and toolchains.
    // AlignedAlloc() allows to enforce higher alignment.
    // This option determines whether it is supported for the partition.
    // Allowing AlignedAlloc() comes at a cost of disallowing extras in front
    // of the allocation.
    kDisallowed,
    kAllowed,
  };

  enum class ThreadCache : uint8_t {
    kDisabled,
    kEnabled,
  };

  enum class Quarantine : uint8_t {
    kDisallowed,
    kAllowed,
  };

  enum class Cookie : uint8_t {
    kDisallowed,
    kAllowed,
  };

  enum class BackupRefPtr : uint8_t {
    kDisabled,
    kEnabled,
  };

  enum class UseConfigurablePool : uint8_t {
    kNo,
    kIfAvailable,
  };

  // Constructor to suppress aggregate initialization.
  constexpr PartitionOptions(AlignedAlloc aligned_alloc,
                             ThreadCache thread_cache,
                             Quarantine quarantine,
                             Cookie cookie,
                             BackupRefPtr backup_ref_ptr,
                             UseConfigurablePool use_configurable_pool)
      : aligned_alloc(aligned_alloc),
        thread_cache(thread_cache),
        quarantine(quarantine),
        cookie(cookie),
        backup_ref_ptr(backup_ref_ptr),
        use_configurable_pool(use_configurable_pool) {}

  AlignedAlloc aligned_alloc;
  ThreadCache thread_cache;
  Quarantine quarantine;
  Cookie cookie;
  BackupRefPtr backup_ref_ptr;
  UseConfigurablePool use_configurable_pool;
};

// Never instantiate a PartitionRoot directly, instead use
// PartitionAllocator.
template <bool thread_safe>
struct alignas(64) BASE_EXPORT PartitionRoot {
  using SlotSpan = internal::SlotSpanMetadata<thread_safe>;
  using Page = internal::PartitionPage<thread_safe>;
  using Bucket = internal::PartitionBucket<thread_safe>;
  using FreeListEntry = internal::PartitionFreelistEntry;
  using SuperPageExtentEntry =
      internal::PartitionSuperPageExtentEntry<thread_safe>;
  using DirectMapExtent = internal::PartitionDirectMapExtent<thread_safe>;
  using PCScan = internal::PCScan;

  enum class QuarantineMode : uint8_t {
    kAlwaysDisabled,
    kDisabledByDefault,
    kEnabled,
  };

  enum class ScanMode : uint8_t {
    kDisabled,
    kEnabled,
  };

#if !defined(PA_EXTRAS_REQUIRED)
  // Teach the compiler that code can be optimized in builds that use no
  // extras.
  static constexpr inline uint32_t extras_size = 0;
  static constexpr inline uint32_t extras_offset = 0;
#endif  // !defined(PA_EXTRAS_REQUIRED)

  // Read-mostly flags.
  union {
    // Flags accessed on fast paths.
    //
    // Careful! PartitionAlloc's performance is sensitive to its layout.  Please
    // put the fast-path objects in the struct below, and the other ones after
    // the union..
    struct {
      // Defines whether objects should be quarantined for this root.
      QuarantineMode quarantine_mode;

      // Defines whether the root should be scanned.
      ScanMode scan_mode;

      bool with_thread_cache = false;

      bool allow_aligned_alloc;
      bool allow_cookie;
#if BUILDFLAG(USE_BACKUP_REF_PTR)
      bool brp_enabled_;
#endif
      bool use_configurable_pool;

#if defined(PA_EXTRAS_REQUIRED)
      uint32_t extras_size;
      uint32_t extras_offset;
#endif  // defined(PA_EXTRAS_REQUIRED)
    };

    // The flags above are accessed for all (de)allocations, and are mostly
    // read-only. They should not share a cacheline with the data below, which
    // is only touched when the lock is taken.
    uint8_t one_cacheline[kPartitionCachelineSize];
  };

  // Not used on the fastest path (thread cache allocations), but on the fast
  // path of the central allocator.
  static_assert(thread_safe, "Only the thread-safe root is supported.");
  ::partition_alloc::Lock lock_;

  Bucket buckets[kNumBuckets] = {};
  Bucket sentinel_bucket{};

  // All fields below this comment are not accessed on the fast path.
  bool initialized = false;

  // Bookkeeping.
  // - total_size_of_super_pages - total virtual address space for normal bucket
  //     super pages
  // - total_size_of_direct_mapped_pages - total virtual address space for
  //     direct-map regions
  // - total_size_of_committed_pages - total committed pages for slots (doesn't
  //     include metadata, bitmaps (if any), or any data outside or regions
  //     described in #1 and #2)
  // Invariant: total_size_of_allocated_bytes <=
  //            total_size_of_committed_pages <
  //                total_size_of_super_pages +
  //                total_size_of_direct_mapped_pages.
  // Invariant: total_size_of_committed_pages <= max_size_of_committed_pages.
  // Invariant: total_size_of_allocated_bytes <= max_size_of_allocated_bytes.
  // Invariant: max_size_of_allocated_bytes <= max_size_of_committed_pages.
  // Since all operations on the atomic variables have relaxed semantics, we
  // don't check these invariants with DCHECKs.
  std::atomic<size_t> total_size_of_committed_pages{0};
  std::atomic<size_t> max_size_of_committed_pages{0};
  std::atomic<size_t> total_size_of_super_pages{0};
  std::atomic<size_t> total_size_of_direct_mapped_pages{0};
  size_t total_size_of_allocated_bytes GUARDED_BY(lock_) = 0;
  size_t max_size_of_allocated_bytes GUARDED_BY(lock_) = 0;
  // Atomic, because system calls can be made without the lock held.
  std::atomic<uint64_t> syscall_count{};
  std::atomic<uint64_t> syscall_total_time_ns{};
#if BUILDFLAG(USE_BACKUP_REF_PTR)
  std::atomic<size_t> total_size_of_brp_quarantined_bytes{0};
  std::atomic<size_t> total_count_of_brp_quarantined_slots{0};
#endif
  // Slot span memory which has been provisioned, and is currently unused as
  // it's part of an empty SlotSpan. This is not clean memory, since it has
  // either been used for a memory allocation, and/or contains freelist
  // entries. But it might have been moved to swap. Note that all this memory
  // can be decommitted at any time.
  size_t empty_slot_spans_dirty_bytes GUARDED_BY(lock_) = 0;

  // Only tolerate up to |total_size_of_committed_pages >>
  // max_empty_slot_spans_dirty_bytes_shift| dirty bytes in empty slot
  // spans. That is, the default value of 3 tolerates up to 1/8. Since
  // |empty_slot_spans_dirty_bytes| is never strictly larger than
  // total_size_of_committed_pages, setting this to 0 removes the cap. This is
  // useful to make tests deterministic and easier to reason about.
  int max_empty_slot_spans_dirty_bytes_shift = 3;

  uintptr_t next_super_page = 0;
  uintptr_t next_partition_page = 0;
  uintptr_t next_partition_page_end = 0;
  SuperPageExtentEntry* current_extent = nullptr;
  SuperPageExtentEntry* first_extent = nullptr;
  DirectMapExtent* direct_map_list GUARDED_BY(lock_) = nullptr;
  SlotSpan* global_empty_slot_span_ring[kMaxFreeableSpans] GUARDED_BY(
      lock_) = {};
  int16_t global_empty_slot_span_ring_index GUARDED_BY(lock_) = 0;
  int16_t global_empty_slot_span_ring_size GUARDED_BY(lock_) =
      kDefaultEmptySlotSpanRingSize;

  // Integrity check = ~reinterpret_cast<uintptr_t>(this).
  uintptr_t inverted_self = 0;
  std::atomic<int> thread_caches_being_constructed_{0};

  bool quarantine_always_for_testing = false;

  PartitionRoot()
      : quarantine_mode(QuarantineMode::kAlwaysDisabled),
        scan_mode(ScanMode::kDisabled) {}
  explicit PartitionRoot(PartitionOptions opts) { Init(opts); }
  ~PartitionRoot();

  // Public API
  //
  // Allocates out of the given bucket. Properly, this function should probably
  // be in PartitionBucket, but because the implementation needs to be inlined
  // for performance, and because it needs to inspect SlotSpanMetadata,
  // it becomes impossible to have it in PartitionBucket as this causes a
  // cyclical dependency on SlotSpanMetadata function implementations.
  //
  // Moving it a layer lower couples PartitionRoot and PartitionBucket, but
  // preserves the layering of the includes.
  void Init(PartitionOptions);

  void EnableThreadCacheIfSupported();

  ALWAYS_INLINE static bool IsValidSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSlotSpan(SlotSpan* slot_span);
  // These two functions work unconditionally for normal buckets.
  // For direct map, they only work for the first super page of a reservation,
  // (see partition_alloc_constants.h for the direct map allocation layout).
  // In particular, the functions always work for a pointer to the start of a
  // reservation.
  ALWAYS_INLINE static PartitionRoot* FromFirstSuperPage(uintptr_t super_page);
  ALWAYS_INLINE static PartitionRoot* FromAddrInFirstSuperpage(
      uintptr_t address);

  ALWAYS_INLINE void IncreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecommitSystemPagesForData(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void RecommitSystemPagesForData(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE bool TryRecommitSystemPagesForData(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      LOCKS_EXCLUDED(lock_);

  [[noreturn]] NOINLINE void OutOfMemory(size_t size);

  // Returns a pointer aligned on |alignment|, or nullptr.
  //
  // |alignment| has to be a power of two and a multiple of sizeof(void*) (as in
  // posix_memalign() for POSIX systems). The returned pointer may include
  // padding, and can be passed to |Free()| later.
  //
  // NOTE: This is incompatible with anything that adds extras before the
  // returned pointer, such as ref-count.
  ALWAYS_INLINE void* AlignedAllocFlags(int flags,
                                        size_t alignment,
                                        size_t requested_size);

  // PartitionAlloc supports multiple partitions, and hence multiple callers to
  // these functions. Setting ALWAYS_INLINE bloats code, and can be detrimental
  // to performance, for instance if multiple callers are hot (by increasing
  // cache footprint).
  // Set NOINLINE on the "basic" top-level functions to mitigate that for
  // "vanilla" callers.
  NOINLINE MALLOC_FN void* Alloc(size_t requested_size,
                                 const char* type_name) MALLOC_ALIGNED;
  ALWAYS_INLINE MALLOC_FN void* AllocFlags(int flags,
                                           size_t requested_size,
                                           const char* type_name)
      MALLOC_ALIGNED;
  // Same as |AllocFlags()|, but allows specifying |slot_span_alignment|. It has
  // to be a multiple of partition page size, greater than 0 and no greater than
  // kMaxSupportedAlignment. If it equals exactly 1 partition page, no special
  // action is taken as PartitoinAlloc naturally guarantees this alignment,
  // otherwise a sub-optimial allocation strategy is used to guarantee the
  // higher-order alignment.
  ALWAYS_INLINE MALLOC_FN void* AllocFlagsInternal(int flags,
                                                   size_t requested_size,
                                                   size_t slot_span_alignment,
                                                   const char* type_name)
      MALLOC_ALIGNED;
  // Same as |AllocFlags()|, but bypasses the allocator hooks.
  //
  // This is separate from AllocFlags() because other callers of AllocFlags()
  // should not have the extra branch checking whether the hooks should be
  // ignored or not. This is the same reason why |FreeNoHooks()|
  // exists. However, |AlignedAlloc()| and |Realloc()| have few callers, so
  // taking the extra branch in the non-malloc() case doesn't hurt. In addition,
  // for the malloc() case, the compiler correctly removes the branch, since
  // this is marked |ALWAYS_INLINE|.
  ALWAYS_INLINE MALLOC_FN void* AllocFlagsNoHooks(int flags,
                                                  size_t requested_size,
                                                  size_t slot_span_alignment)
      MALLOC_ALIGNED;

  NOINLINE void* Realloc(void* ptr,
                         size_t newize,
                         const char* type_name) MALLOC_ALIGNED;
  // Overload that may return nullptr if reallocation isn't possible. In this
  // case, |ptr| remains valid.
  NOINLINE void* TryRealloc(void* ptr,
                            size_t new_size,
                            const char* type_name) MALLOC_ALIGNED;
  NOINLINE void* ReallocFlags(int flags,
                              void* ptr,
                              size_t new_size,
                              const char* type_name) MALLOC_ALIGNED;
  NOINLINE static void Free(void* ptr);
  // Same as |Free()|, bypasses the allocator hooks.
  ALWAYS_INLINE static void FreeNoHooks(void* ptr);
  // Immediately frees the pointer bypassing the quarantine.
  ALWAYS_INLINE void FreeNoHooksImmediate(uintptr_t address,
                                          SlotSpan* slot_span,
                                          uintptr_t slot_start);

  ALWAYS_INLINE static size_t GetUsableSize(void* ptr);

  ALWAYS_INLINE size_t AllocationCapacityFromPtr(void* ptr) const;
  ALWAYS_INLINE size_t AllocationCapacityFromRequestedSize(size_t size) const;

  // Frees memory from this partition, if possible, by decommitting pages or
  // even entire slot spans. |flags| is an OR of base::PartitionPurgeFlags.
  void PurgeMemory(int flags);

  // Reduces the size of the empty slot spans ring, until the dirty size is <=
  // |limit|.
  void ShrinkEmptySlotSpansRing(size_t limit) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // The empty slot span ring starts "small", can be enlarged later. This
  // improves performance by performing fewer system calls, at the cost of more
  // memory usage.
  void EnableLargeEmptySlotSpanRing() {
    ::partition_alloc::ScopedGuard locker{lock_};
    global_empty_slot_span_ring_size = kMaxFreeableSpans;
  }

  void DumpStats(const char* partition_name,
                 bool is_light_dump,
                 PartitionStatsDumper* partition_stats_dumper);

  static void DeleteForTesting(PartitionRoot* partition_root);
  void ResetBookkeepingForTesting();

  static uint16_t SizeToBucketIndex(size_t size);

  ALWAYS_INLINE void FreeInSlotSpan(uintptr_t slot_start, SlotSpan* slot_span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Frees memory, with |slot_start| as returned by |RawAlloc()|.
  ALWAYS_INLINE void RawFree(uintptr_t slot_start);
  ALWAYS_INLINE void RawFree(uintptr_t slot_start, SlotSpan* slot_span)
      LOCKS_EXCLUDED(lock_);

  ALWAYS_INLINE void RawFreeBatch(FreeListEntry* head,
                                  FreeListEntry* tail,
                                  size_t size,
                                  SlotSpan* slot_span) LOCKS_EXCLUDED(lock_);

  ALWAYS_INLINE void RawFreeWithThreadCache(uintptr_t slot_start,
                                            SlotSpan* slot_span);

  internal::ThreadCache* thread_cache_for_testing() const {
    return with_thread_cache ? internal::ThreadCache::Get() : nullptr;
  }
  size_t get_total_size_of_committed_pages() const {
    return total_size_of_committed_pages.load(std::memory_order_relaxed);
  }
  size_t get_max_size_of_committed_pages() const {
    return max_size_of_committed_pages.load(std::memory_order_relaxed);
  }

  size_t get_total_size_of_allocated_bytes() const {
    // Since this is only used for bookkeeping, we don't care if the value is
    // stale, so no need to get a lock here.
    return TS_UNCHECKED_READ(total_size_of_allocated_bytes);
  }

  size_t get_max_size_of_allocated_bytes() const {
    // Since this is only used for bookkeeping, we don't care if the value is
    // stale, so no need to get a lock here.
    return TS_UNCHECKED_READ(max_size_of_allocated_bytes);
  }

  internal::pool_handle ChoosePool() const {
    if (use_configurable_pool) {
      return internal::GetConfigurablePool();
    }
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    return brp_enabled() ? internal::GetBRPPool() : internal::GetRegularPool();
#else
    return internal::GetRegularPool();
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  }

  ALWAYS_INLINE bool IsQuarantineAllowed() const {
    return quarantine_mode != QuarantineMode::kAlwaysDisabled;
  }

  ALWAYS_INLINE bool IsQuarantineEnabled() const {
    return quarantine_mode == QuarantineMode::kEnabled;
  }

  ALWAYS_INLINE bool ShouldQuarantine(uintptr_t slot_start) const {
    if (UNLIKELY(quarantine_mode != QuarantineMode::kEnabled))
      return false;
#if HAS_MEMORY_TAGGING
    if (UNLIKELY(quarantine_always_for_testing))
      return true;
    // If quarantine is enabled and tag overflows, move slot to quarantine, to
    // prevent the attacker from exploiting a pointer that has old tag.
    return HasOverflowTag(slot_start);
#else
    return true;
#endif
  }

  ALWAYS_INLINE void SetQuarantineAlwaysForTesting(bool value) {
    quarantine_always_for_testing = value;
  }

  ALWAYS_INLINE bool IsScanEnabled() const {
    // Enabled scan implies enabled quarantine.
    PA_DCHECK(scan_mode != ScanMode::kEnabled || IsQuarantineEnabled());
    return scan_mode == ScanMode::kEnabled;
  }

  static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
  GetDirectMapMetadataAndGuardPagesSize() {
    // Because we need to fake a direct-map region to look like a super page, we
    // need to allocate more pages around the payload:
    // - The first partition page is a combination of metadata and guard region.
    // - We also add a trailing guard page. In most cases, a system page would
    //   suffice. But on 32-bit systems when BRP is on, we need a partition page
    //   to match granularity of the BRP pool bitmap. For cosistency, we'll use
    //   a partition page everywhere, which is cheap as it's uncommitted address
    //   space anyway.
    return 2 * PartitionPageSize();
  }

  static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
  GetDirectMapSlotSize(size_t raw_size) {
    // Caller must check that the size is not above the MaxDirectMapped()
    // limit before calling. This also guards against integer overflow in the
    // calculation here.
    PA_DCHECK(raw_size <= MaxDirectMapped());
    return bits::AlignUp(raw_size, SystemPageSize());
  }

  static ALWAYS_INLINE size_t
  GetDirectMapReservationSize(size_t padded_raw_size) {
    // Caller must check that the size is not above the MaxDirectMapped()
    // limit before calling. This also guards against integer overflow in the
    // calculation here.
    PA_DCHECK(padded_raw_size <= MaxDirectMapped());
    return bits::AlignUp(
        padded_raw_size + GetDirectMapMetadataAndGuardPagesSize(),
        DirectMapAllocationGranularity());
  }

  ALWAYS_INLINE size_t AdjustSize0IfNeeded(size_t size) const {
    // There are known cases where allowing size 0 would lead to problems:
    // 1. If extras are present only before allocation (e.g. BRP ref-count), the
    //    extras will fill the entire kAlignment-sized slot, leading to
    //    returning a pointer to the next slot. FreeNoHooks() and ReallocFlags()
    //    call SlotSpan::FromSlotInnerPtr(ptr) prior to subtracting extras, thus
    //    potentially getting a wrong slot span.
    // 2. If we put BRP ref-count in the previous slot, that slot may be free.
    //    In this case, the slot needs to fit both, a free-list entry and a
    //    ref-count. If sizeof(PartitionRefCount) is 8, it fills the entire
    //    smallest slot on 32-bit systems (kSmallestBucket is 8), thus not
    //    leaving space for the free-list entry.
    // 3. On macOS and iOS, PartitionGetSizeEstimate() is used for two purposes:
    //    as a zone dispatcher and as an underlying implementation of
    //    malloc_size(3). As a zone dispatcher, zero has a special meaning of
    //    "doesn't belong to this zone". When extras fill out the entire slot,
    //    the usable size is 0, thus confusing the zone dispatcher.
    //
    // To save ourselves a branch on this hot path, we could eliminate this
    // check at compile time for cases not listed above. The #if statement would
    // be rather complex. Then there is also the fear of the unknown. The
    // existing cases were discovered through obscure, painful-to-debug crashes.
    // Better save ourselves trouble with not-yet-discovered cases.
    if (UNLIKELY(size == 0))
      return 1;
    return size;
  }

  // Adjusts the size by adding extras. Also include the 0->1 adjustment if
  // needed.
  ALWAYS_INLINE size_t AdjustSizeForExtrasAdd(size_t size) const {
    size = AdjustSize0IfNeeded(size);
    PA_DCHECK(size + extras_size >= size);
    return size + extras_size;
  }

  // Adjusts the size by subtracing extras. Doesn't include the 0->1 adjustment,
  // which leads to an asymmetry with AdjustSizeForExtrasAdd, but callers of
  // AdjustSizeForExtrasSubtract either expect the adjustment to be included, or
  // are indifferent.
  ALWAYS_INLINE size_t AdjustSizeForExtrasSubtract(size_t size) const {
    return size - extras_size;
  }

  // TODO(bartekn): Consider |void* SlotStartToObjectStart(uintptr_t)|.
  ALWAYS_INLINE void* AdjustPointerForExtrasAdd(uintptr_t address) const {
    return reinterpret_cast<void*>(address + extras_offset);
  }

  // TODO(bartekn): Consider |uintptr_t ObjectStartToSlotStart(void*)|.
  ALWAYS_INLINE uintptr_t AdjustPointerForExtrasSubtract(void* ptr) const {
    return reinterpret_cast<uintptr_t>(ptr) - extras_offset;
  }

  bool brp_enabled() const {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    return brp_enabled_;
#else
    return false;
#endif
  }

  ALWAYS_INLINE bool uses_configurable_pool() const {
    return use_configurable_pool;
  }

  // To make tests deterministic, it is necessary to uncap the amount of memory
  // waste incurred by empty slot spans. Otherwise, the size of various
  // freelists, and committed memory becomes harder to reason about (and
  // brittle) with a single thread, and non-deterministic with several.
  void UncapEmptySlotSpanMemoryForTesting() {
    max_empty_slot_spans_dirty_bytes_shift = 0;
  }

 private:
  // |buckets| has `kNumBuckets` elements, but we sometimes access it at index
  // `kNumBuckets`, which is occupied by the sentinel bucket. The correct layout
  // is enforced by a static_assert() in partition_root.cc, so this is
  // fine. However, UBSAN is correctly pointing out that there is an
  // out-of-bounds access, so disable it for these accesses.
  //
  // See crbug.com/1150772 for an instance of Clusterfuzz / UBSAN detecting
  // this.
  ALWAYS_INLINE const Bucket& NO_SANITIZE("undefined")
      bucket_at(size_t i) const {
    PA_DCHECK(i <= kNumBuckets);
    return buckets[i];
  }

  // Returns whether a |bucket| from |this| root is direct-mapped. This function
  // does not touch |bucket|, contrary to  PartitionBucket::is_direct_mapped().
  //
  // This is meant to be used in hot paths, and particularly *before* going into
  // the thread cache fast path. Indeed, real-world profiles show that accessing
  // an allocation's bucket is responsible for a sizable fraction of *total*
  // deallocation time. This can be understood because
  // - All deallocations have to access the bucket to know whether it is
  //   direct-mapped. If not (vast majority of allocations), it can go through
  //   the fast path, i.e. thread cache.
  // - The bucket is relatively frequently written to, by *all* threads
  //   (e.g. every time a slot span becomes full or empty), so accessing it will
  //   result in some amount of cacheline ping-pong.
  ALWAYS_INLINE bool IsDirectMappedBucket(Bucket* bucket) const {
    // All regular allocations are associated with a bucket in the |buckets_|
    // array. A range check is then sufficient to identify direct-mapped
    // allocations.
    bool ret = !(bucket >= this->buckets && bucket <= &this->sentinel_bucket);
    PA_DCHECK(ret == bucket->is_direct_mapped());
    return ret;
  }

  // Allocates memory, without initializing extras.
  //
  // - |flags| are as in AllocFlags().
  // - |raw_size| accommodates for extras on top of AllocFlags()'s
  //   |requested_size|.
  // - |usable_size| and |is_already_zeroed| are output only. |usable_size| is
  //   guaranteed to be larger or equal to AllocFlags()'s |requested_size|.
  ALWAYS_INLINE uintptr_t RawAlloc(Bucket* bucket,
                                   int flags,
                                   size_t raw_size,
                                   size_t slot_span_alignment,
                                   size_t* usable_size,
                                   bool* is_already_zeroed);
  ALWAYS_INLINE uintptr_t AllocFromBucket(Bucket* bucket,
                                          int flags,
                                          size_t raw_size,
                                          size_t slot_span_alignment,
                                          size_t* usable_size,
                                          bool* is_already_zeroed)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool TryReallocInPlaceForNormalBuckets(void* ptr,
                                         SlotSpan* slot_span,
                                         size_t new_size);
  bool TryReallocInPlaceForDirectMap(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      size_t requested_size) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecommitEmptySlotSpans() EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void RawFreeLocked(uintptr_t slot_start)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  uintptr_t MaybeInitThreadCacheAndAlloc(uint16_t bucket_index,
                                         size_t* slot_size);

  friend class internal::ThreadCache;
};

namespace internal {

class ScopedSyscallTimer {
 public:
#if defined(PA_COUNT_SYSCALL_TIME)
  explicit ScopedSyscallTimer(PartitionRoot<>* root)
      : root_(root), tick_(base::TimeTicks::Now()) {}

  ~ScopedSyscallTimer() {
    root_->syscall_count.fetch_add(1, std::memory_order_relaxed);

    uint64_t elapsed_nanos = (base::TimeTicks::Now() - tick_).InNanoseconds();
    root_->syscall_total_time_ns.fetch_add(elapsed_nanos,
                                           std::memory_order_relaxed);
  }

 private:
  PartitionRoot<>* root_;
  const base::TimeTicks tick_;
#else
  explicit ScopedSyscallTimer(PartitionRoot<>* root) {
    root->syscall_count.fetch_add(1, std::memory_order_relaxed);
  }
#endif
};

// Gets the SlotSpanMetadata object of the slot span that contains |address|.
// It's used with intention to do obtain the slot size.
//
// CAUTION! For direct-mapped allocation, |address| has to be within the first
// partition page.
template <bool thread_safe>
ALWAYS_INLINE internal::SlotSpanMetadata<thread_safe>*
PartitionAllocGetSlotSpanForSizeQuery(uintptr_t address) {
  // No need to lock here. Only |address| being freed by another thread could
  // cause trouble, and the caller is responsible for that not happening.
  auto* slot_span = internal::SlotSpanMetadata<thread_safe>::FromSlotInnerPtr(
      reinterpret_cast<void*>(address));
  // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
  return slot_span;
}

#if BUILDFLAG(USE_BACKUP_REF_PTR)

ALWAYS_INLINE uintptr_t
PartitionAllocGetDirectMapSlotStartInBRPPool(uintptr_t address) {
  PA_DCHECK(IsManagedByPartitionAllocBRPPool(address));
#if defined(PA_HAS_64_BITS_POINTERS)
  // Use this variant of GetDirectMapReservationStart as it has better
  // performance.
  uintptr_t offset = OffsetInBRPPool(address);
  uintptr_t reservation_start =
      GetDirectMapReservationStart(address, kBRPPoolHandle, offset);
#else
  uintptr_t reservation_start = GetDirectMapReservationStart(address);
#endif
  if (!reservation_start)
    return 0;

  // The direct map allocation may not start exactly from the first page, as
  // there may be padding for alignment. The first page metadata holds an offset
  // to where direct map metadata, and thus direct map start, are located.
  auto* first_page = PartitionPage<ThreadSafe>::FromPtr(
      reinterpret_cast<void*>(reservation_start + PartitionPageSize()));
  auto* page = first_page + first_page->slot_span_metadata_offset;
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  uintptr_t slot_start =
      SlotSpanMetadata<ThreadSafe>::ToSlotSpanStart(&page->slot_span_metadata);
#if DCHECK_IS_ON()
  auto* metadata =
      reinterpret_cast<PartitionDirectMapMetadata<ThreadSafe>*>(page);
  size_t padding_for_alignment =
      metadata->direct_map_extent.padding_for_alignment;
  PA_DCHECK(padding_for_alignment == (page - first_page) * PartitionPageSize());
  PA_DCHECK(slot_start ==
            reservation_start + PartitionPageSize() + padding_for_alignment);
#endif  // DCHECK_IS_ON()
  return slot_start;
}

// Gets the pointer to the beginning of the allocated slot.
//
// This isn't a general purpose function, it is used specifically for obtaining
// BackupRefPtr's ref-count. The caller is responsible for ensuring that the
// ref-count is in place for this allocation.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
ALWAYS_INLINE uintptr_t PartitionAllocGetSlotStartInBRPPool(uintptr_t address) {
  // Adjust to support pointers right past the end of an allocation, which in
  // some cases appear to point outside the designated allocation slot.
  //
  // If ref-count is present before the allocation, then adjusting a valid
  // pointer down will not cause us to go down to the previous slot, otherwise
  // no adjustment is needed (and likely wouldn't be correct as there is
  // a risk of going down to the previous slot). Either way,
  // kPartitionPastAllocationAdjustment takes care of that detail.
  address -= kPartitionPastAllocationAdjustment;
  PA_DCHECK(IsManagedByNormalBucketsOrDirectMap(address));
  DCheckIfManagedByPartitionAllocBRPPool(address);

  uintptr_t directmap_slot_start =
      PartitionAllocGetDirectMapSlotStartInBRPPool(address);
  if (UNLIKELY(directmap_slot_start))
    return directmap_slot_start;
  auto* slot_span = PartitionAllocGetSlotSpanForSizeQuery<ThreadSafe>(address);
  auto* root = PartitionRoot<ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->brp_enabled());

  // Get the offset from the beginning of the slot span.
  uintptr_t slot_span_start =
      SlotSpanMetadata<ThreadSafe>::ToSlotSpanStart(slot_span);
  PA_DCHECK(slot_span_start == memory::UnmaskPtr(slot_span_start));
  size_t offset_in_slot_span = address - slot_span_start;

  auto* bucket = slot_span->bucket;
  return memory::RemaskPtr(slot_span_start +
                           bucket->slot_size *
                               bucket->GetSlotNumber(offset_in_slot_span));
}

// Checks whether a given pointer stays within the same allocation slot after
// modification.
//
// This isn't a general purpose function. The caller is responsible for ensuring
// that the ref-count is in place for this allocation.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
ALWAYS_INLINE bool PartitionAllocIsValidPtrDelta(uintptr_t address,
                                                 ptrdiff_t delta_in_bytes) {
  // Required for pointers right past an allocation. See
  // |PartitionAllocGetSlotStartInBRPPool()|.
  uintptr_t adjusted_address = address - kPartitionPastAllocationAdjustment;
  PA_DCHECK(IsManagedByNormalBucketsOrDirectMap(adjusted_address));
  DCheckIfManagedByPartitionAllocBRPPool(adjusted_address);

  uintptr_t slot_start = PartitionAllocGetSlotStartInBRPPool(adjusted_address);
  // Get |slot_span| from |slot_start| instead of |adjusted_address|, because
  // for direct map, PartitionAllocGetSlotSpanForSizeQuery() only works on the
  // first partition page of the allocation.
  //
  // As a matter of fact, don't use |adjusted_address| beyond this point at all.
  // It was needed to pick the right slot, but now we're dealing with very
  // concrete addresses. Nullify it just in case, to catch errors.
  adjusted_address = 0;
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          slot_start);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->brp_enabled());

  uintptr_t object_start =
      reinterpret_cast<uintptr_t>(root->AdjustPointerForExtrasAdd(slot_start));
  uintptr_t new_address = address + delta_in_bytes;
  return object_start <= new_address &&
         // We use "greater then or equal" below because we want to include
         // pointers right past the end of an allocation.
         new_address <= object_start + slot_span->GetUsableSize(root);
}

ALWAYS_INLINE void PartitionAllocFreeForRefCounting(uintptr_t slot_start) {
  PA_DCHECK(!internal::PartitionRefCountPointer(slot_start)->IsAlive());

  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStart(slot_start);
  auto* root = PartitionRoot<ThreadSafe>::FromSlotSpan(slot_span);
  // PartitionRefCount is required to be allocated inside a `PartitionRoot` that
  // supports reference counts.
  PA_DCHECK(root->brp_enabled());

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(reinterpret_cast<void*>(slot_start), kFreedByte,
         slot_span->GetUtilizedSlotSize()
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#endif

  root->total_size_of_brp_quarantined_bytes.fetch_sub(
      slot_span->GetSlotSizeForBookkeeping(), std::memory_order_relaxed);
  root->total_count_of_brp_quarantined_slots.fetch_sub(
      1, std::memory_order_relaxed);

  root->RawFreeWithThreadCache(slot_start, slot_span);
}
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

}  // namespace internal

template <bool thread_safe>
ALWAYS_INLINE uintptr_t
PartitionRoot<thread_safe>::AllocFromBucket(Bucket* bucket,
                                            int flags,
                                            size_t raw_size,
                                            size_t slot_span_alignment,
                                            size_t* usable_size,
                                            bool* is_already_zeroed) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));
  SlotSpan* slot_span = bucket->active_slot_spans_head;
  // There always must be a slot span on the active list (could be a sentinel).
  PA_DCHECK(slot_span);
  // Check that it isn't marked full, which could only be true if the span was
  // removed from the active list.
  PA_DCHECK(!slot_span->marked_full);

  uintptr_t slot_start =
      reinterpret_cast<uintptr_t>(slot_span->get_freelist_head());
  // Use the fast path when a slot is readily available on the free list of the
  // first active slot span. However, fall back to the slow path if a
  // higher-order alignment is requested, because an inner slot of an existing
  // slot span is unlikely to satisfy it.
  if (LIKELY(slot_span_alignment <= PartitionPageSize() && slot_start)) {
    *is_already_zeroed = false;
    // This is a fast path, so avoid calling GetUsableSize() on Release builds
    // as it is more costly. Copy its small bucket path instead.
    *usable_size = AdjustSizeForExtrasSubtract(bucket->slot_size);
    PA_DCHECK(*usable_size == slot_span->GetUsableSize(this));

    // If these DCHECKs fire, you probably corrupted memory.
    // TODO(crbug.com/1257655): See if we can afford to make these CHECKs.
    PA_DCHECK(IsValidSlotSpan(slot_span));

    // All large allocations must go through the slow path to correctly update
    // the size metadata.
    PA_DCHECK(!slot_span->CanStoreRawSize());
    PA_DCHECK(!slot_span->bucket->is_direct_mapped());
    void* entry = slot_span->PopForAlloc(bucket->slot_size);
    PA_DCHECK(reinterpret_cast<uintptr_t>(entry) == slot_start);

    PA_DCHECK(slot_span->bucket == bucket);
  } else {
    slot_start = bucket->SlowPathAlloc(this, flags, raw_size,
                                       slot_span_alignment, is_already_zeroed);
    if (UNLIKELY(!slot_start))
      return 0;

    slot_span = SlotSpan::FromSlotStart(slot_start);
    // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
    PA_DCHECK(IsValidSlotSpan(slot_span));
    // For direct mapped allocations, |bucket| is the sentinel.
    PA_DCHECK((slot_span->bucket == bucket) ||
              (slot_span->bucket->is_direct_mapped() &&
               (bucket == &sentinel_bucket)));

    *usable_size = slot_span->GetUsableSize(this);
  }
  PA_DCHECK(slot_span->GetUtilizedSlotSize() <= slot_span->bucket->slot_size);
  total_size_of_allocated_bytes += slot_span->GetSlotSizeForBookkeeping();
  max_size_of_allocated_bytes =
      std::max(max_size_of_allocated_bytes, total_size_of_allocated_bytes);
  return slot_start;
}

// static
template <bool thread_safe>
NOINLINE void PartitionRoot<thread_safe>::Free(void* ptr) {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  free(ptr);
#else
  if (UNLIKELY(!ptr))
    return;

  if (PartitionAllocHooks::AreHooksEnabled()) {
    PartitionAllocHooks::FreeObserverHookIfEnabled(ptr);
    if (PartitionAllocHooks::FreeOverrideHookIfEnabled(ptr))
      return;
  }

  FreeNoHooks(ptr);
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
}

// static
template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeNoHooks(void* ptr) {
  if (UNLIKELY(!ptr))
    return;
  // Almost all calls to FreeNoNooks() will end up writing to |*ptr|, the only
  // cases where we don't would be delayed free() in PCScan, but |*ptr| can be
  // cold in cache.
  PA_PREFETCH(ptr);
  uintptr_t address = reinterpret_cast<uintptr_t>(ptr);

  // On Android, malloc() interception is more fragile than on other
  // platforms, as we use wrapped symbols. However, the GigaCage allows us to
  // quickly tell that a pointer was allocated with PartitionAlloc.
  //
  // This is a crash to detect imperfect symbol interception. However, we can
  // forward allocations we don't own to the system malloc() implementation in
  // these rare cases, assuming that some remain.
  //
  // On Chromecast, this is already checked in PartitionFree() in the shim.
  //
  // On Linux, this is intended to ease debugging of crbug.com/1266412. Enabled
  // on 64 bit only, as the check is pretty cheap in this case (range check,
  // essentially).
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&              \
    ((BUILDFLAG(IS_ANDROID) && !BUILDFLAG(IS_CHROMECAST)) || \
     (BUILDFLAG(IS_LINUX) && defined(ARCH_CPU_64_BITS)))
  PA_CHECK(IsManagedByPartitionAlloc(address));
#endif

  // Fetch the root from the pointer, and not the SlotSpan. This is important,
  // as getting to the SlotSpan is a slow operation (looking into the metadata
  // area, and following a pointer), and SlotSpans can induce cache coherency
  // traffic (since they're read on every free(), and written to on any
  // malloc()/free() that is not a hit in the thread cache). This way we change
  // the critical path from ptr -> slot_span -> root into two *parallel* ones:
  // 1. address -> root
  // 2. ptr -> slot_span
  auto* root = FromAddrInFirstSuperpage(address);

  // Call FromSlotInnerPtr instead of FromSlotStart because the pointer hasn't
  // been adjusted yet.
  SlotSpan* slot_span = SlotSpan::FromSlotInnerPtr(ptr);
  // We are going to read from |*slot_span| in all branches. Since
  // |FromSlotSpan()| below doesn't touch *slot_span, there is some time for the
  // prefetch to be useful.
  //
  // TODO(crbug.com/1207307): It would be much better to avoid touching
  // |*slot_span| at all on the fast path, or at least to separate its read-only
  // parts (i.e. bucket pointer) from the rest. Indeed, every thread cache miss
  // (or batch fill) will *write* to |slot_span->freelist_head|, leading to
  // cacheline ping-pong.
  PA_PREFETCH(slot_span);

  // TODO(crbug.com/1257655): See if we can afford to make this a CHECK.
  PA_DCHECK(IsValidSlotSpan(slot_span));
  PA_DCHECK(FromSlotSpan(slot_span) == root);

  const size_t slot_size = slot_span->bucket->slot_size;
  uintptr_t slot_start = root->AdjustPointerForExtrasSubtract(ptr);
  if (LIKELY(slot_size <= kMaxMemoryTaggingSize)) {
    // Incrementing the memory range returns the true underlying tag, so
    // RemaskPtr is not required here.
    slot_start = memory::TagMemoryRangeIncrement(slot_start, slot_size);
    address = memory::RemaskPtr(address);
  }

  // TODO(bikineev): Change the condition to LIKELY once PCScan is enabled by
  // default.
  if (UNLIKELY(root->ShouldQuarantine(slot_start))) {
    // PCScan safepoint. Call before potentially scheduling scanning task.
    PCScan::JoinScanIfNeeded();
    if (LIKELY(internal::IsManagedByNormalBuckets(address))) {
      PCScan::MoveToQuarantine(ptr, slot_span->GetUsableSize(root), slot_start,
                               slot_span->bucket->slot_size);
      return;
    }
  }

  root->FreeNoHooksImmediate(address, slot_span, slot_start);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeNoHooksImmediate(
    uintptr_t address,
    SlotSpan* slot_span,
    uintptr_t slot_start) {
  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/ref-count management
  // - Before the "raw" allocator.
  //
  // On the deallocation side:
  // 1. Check cookie/ref-count, adjust the pointer
  // 2. Deallocation
  //   a. Return to the thread cache if possible. If it succeeds, return.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  PA_DCHECK(address);
  PA_DCHECK(slot_span);
  PA_DCHECK(IsValidSlotSpan(slot_span));
  PA_DCHECK(slot_start);

  // |address| points after the ref-count.
  //
  // Layout inside the slot:
  //  <-extras->                  <-extras->
  //  <-------GetUtilizedSlotSize()-------->
  //           <-GetUsableSize()-->
  //  |[refcnt]|...data...|[empty]|[cookie]|[unused]|
  //           ^
  //        address
  //
  // Note: ref-count and cookie can be 0-sized.
  //
  // For more context, see the other "Layout inside the slot" comment below.

#if DCHECK_IS_ON()
  if (allow_cookie) {
    // Verify the cookie after the allocated region.
    // If this assert fires, you probably corrupted memory.
    internal::PartitionCookieCheckValue(
        reinterpret_cast<unsigned char*>(address) +
        slot_span->GetUsableSize(this));
  }
#endif

  // TODO(bikineev): Change the condition to LIKELY once PCScan is enabled by
  // default.
  if (UNLIKELY(IsQuarantineEnabled())) {
    if (LIKELY(internal::IsManagedByNormalBuckets(address))) {
      uintptr_t unmasked_slot_start = memory::UnmaskPtr(slot_start);
      // Mark the state in the state bitmap as freed.
      internal::StateBitmapFromAddr(unmasked_slot_start)
          ->Free(unmasked_slot_start);
    }
  }

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  // TODO(keishi): Add LIKELY when brp is fully enabled as |brp_enabled| will be
  // false only for the aligned partition.
  if (brp_enabled()) {
    auto* ref_count = internal::PartitionRefCountPointer(slot_start);
    // If there are no more references to the allocation, it can be freed
    // immediately. Otherwise, defer the operation and zap the memory to turn
    // potential use-after-free issues into unexploitable crashes.
    if (UNLIKELY(!ref_count->IsAliveWithNoKnownRefs()))
      internal::SecureMemset(reinterpret_cast<void*>(address), kQuarantinedByte,
                             slot_span->GetUsableSize(this));

    if (UNLIKELY(!(ref_count->ReleaseFromAllocator()))) {
      total_size_of_brp_quarantined_bytes.fetch_add(
          slot_span->GetSlotSizeForBookkeeping(), std::memory_order_relaxed);
      total_count_of_brp_quarantined_slots.fetch_add(1,
                                                     std::memory_order_relaxed);
      return;
    }
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(reinterpret_cast<void*>(slot_start), kFreedByte,
         slot_span->GetUtilizedSlotSize()
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#elif defined(PA_ZERO_RANDOMLY_ON_FREE)
  // `memset` only once in a while: we're trading off safety for time
  // efficiency.
  if (UNLIKELY(internal::RandomPeriod()) &&
      !IsDirectMappedBucket(slot_span->bucket)) {
    internal::SecureMemset(reinterpret_cast<void*>(slot_start), 0,
                           slot_span->GetUtilizedSlotSize()
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                               - sizeof(internal::PartitionRefCount)
#endif
    );
  }
#endif  // defined(PA_ZERO_RANDOMLY_ON_FREE)

  RawFreeWithThreadCache(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeInSlotSpan(
    uintptr_t slot_start,
    SlotSpan* slot_span) {
  // An underflow here means we've miscounted |total_size_of_allocated_bytes|
  // somewhere.
  PA_DCHECK(total_size_of_allocated_bytes >=
            slot_span->GetSlotSizeForBookkeeping());
  total_size_of_allocated_bytes -= slot_span->GetSlotSizeForBookkeeping();
  return slot_span->Free(slot_start);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(uintptr_t slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStart(slot_start);
  RawFree(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(uintptr_t slot_start,
                                                       SlotSpan* slot_span) {
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
  *reinterpret_cast<volatile uintptr_t*>(slot_start) = 0;
  // Note: even though we write to slot_start + sizeof(void*) as well, due to
  // alignment constraints, the two locations are always going to be in the same
  // OS page. No need to write to the second one as well.
  //
  // Do not move the store above inside the locked section.
  __asm__ __volatile__("" : : "r"(slot_start) : "memory");

  ::partition_alloc::ScopedGuard guard{lock_};
  FreeInSlotSpan(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeBatch(
    FreeListEntry* head,
    FreeListEntry* tail,
    size_t size,
    SlotSpan* slot_span) {
  PA_DCHECK(head);
  PA_DCHECK(tail);
  PA_DCHECK(size > 0);
  PA_DCHECK(slot_span);
  PA_DCHECK(IsValidSlotSpan(slot_span));
  // The passed freelist is likely to be just built up, which means that the
  // corresponding pages were faulted in (without acquiring the lock). So there
  // is no need to touch pages manually here before the lock.
  ::partition_alloc::ScopedGuard guard{lock_};
  total_size_of_allocated_bytes -=
      (slot_span->GetSlotSizeForBookkeeping() * size);
  slot_span->AppendFreeList(head, tail, size);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeWithThreadCache(
    uintptr_t slot_start,
    SlotSpan* slot_span) {
  // TLS access can be expensive, do a cheap local check first.
  //
  // LIKELY: performance-sensitive partitions have a thread cache, direct-mapped
  // allocations are uncommon.
  if (LIKELY(with_thread_cache && !IsDirectMappedBucket(slot_span->bucket))) {
    size_t bucket_index = slot_span->bucket - this->buckets;
    auto* thread_cache = internal::ThreadCache::Get();
    if (LIKELY(internal::ThreadCache::IsValid(thread_cache) &&
               thread_cache->MaybePutInCache(slot_start, bucket_index))) {
      return;
    }
  }

  RawFree(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeLocked(
    uintptr_t slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStart(slot_start);
  // Direct-mapped deallocation releases then re-acquires the lock. The caller
  // may not expect that, but we never call this function on direct-mapped
  // allocations.
  PA_DCHECK(!IsDirectMappedBucket(slot_span->bucket));
  FreeInSlotSpan(slot_start, slot_span);
}

// static
template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::IsValidSlotSpan(
    SlotSpan* slot_span) {
  slot_span = memory::UnmaskPtr(slot_span);
  PartitionRoot* root = FromSlotSpan(slot_span);
  return root->inverted_self == ~reinterpret_cast<uintptr_t>(root);
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromSlotSpan(SlotSpan* slot_span) {
  auto* extent_entry = reinterpret_cast<SuperPageExtentEntry*>(
      reinterpret_cast<uintptr_t>(slot_span) & SystemPageBaseMask());
  return extent_entry->root;
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromFirstSuperPage(uintptr_t super_page) {
  PA_DCHECK(internal::IsReservationStart(super_page));
  auto* extent_entry =
      internal::PartitionSuperPageToExtent<thread_safe>(super_page);
  PartitionRoot* root = extent_entry->root;
  PA_DCHECK(root->inverted_self == ~reinterpret_cast<uintptr_t>(root));
  return root;
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromAddrInFirstSuperpage(uintptr_t address) {
  uintptr_t super_page = address & kSuperPageBaseMask;
  PA_DCHECK(internal::IsReservationStart(super_page));
  return FromFirstSuperPage(super_page);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::IncreaseCommittedPages(
    size_t len) {
  const auto old_total =
      total_size_of_committed_pages.fetch_add(len, std::memory_order_relaxed);

  const auto new_total = old_total + len;

  // This function is called quite frequently; to avoid performance problems, we
  // don't want to hold a lock here, so we use compare and exchange instead.
  size_t expected = max_size_of_committed_pages.load(std::memory_order_relaxed);
  size_t desired;
  do {
    desired = std::max(expected, new_total);
  } while (!max_size_of_committed_pages.compare_exchange_weak(
      expected, desired, std::memory_order_relaxed, std::memory_order_relaxed));
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::DecreaseCommittedPages(
    size_t len) {
  total_size_of_committed_pages.fetch_sub(len, std::memory_order_relaxed);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::DecommitSystemPagesForData(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  internal::ScopedSyscallTimer timer{this};
  DecommitSystemPages(address, length, accessibility_disposition);
  DecreaseCommittedPages(length);
}

// Not unified with TryRecommitSystemPagesForData() to preserve error codes.
template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RecommitSystemPagesForData(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  internal::ScopedSyscallTimer timer{this};

  bool ok = TryRecommitSystemPages(address, length, PageReadWriteTagged,
                                   accessibility_disposition);
  if (UNLIKELY(!ok)) {
    // Decommit some memory and retry. The alternative is crashing.
    DecommitEmptySlotSpans();
    RecommitSystemPages(address, length, PageReadWriteTagged,
                        accessibility_disposition);
  }

  IncreaseCommittedPages(length);
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::TryRecommitSystemPagesForData(
    uintptr_t address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  internal::ScopedSyscallTimer timer{this};
  bool ok = TryRecommitSystemPages(address, length, PageReadWriteTagged,
                                   accessibility_disposition);
#if defined(PA_COMMIT_CHARGE_IS_LIMITED)
  if (UNLIKELY(!ok)) {
    {
      ::partition_alloc::ScopedGuard guard(lock_);
      DecommitEmptySlotSpans();
    }
    ok = TryRecommitSystemPages(address, length, PageReadWriteTagged,
                                accessibility_disposition);
  }
#endif  // defined(PA_COMMIT_CHARGE_IS_LIMITED)

  if (ok)
    IncreaseCommittedPages(length);

  return ok;
}

// static
// Returns the size available to the app. It can be equal or higher than the
// requested size. If higher, the overage won't exceed what's actually usable
// by the app without a risk of running out of an allocated region or into
// PartitionAlloc's internal data. Used as malloc_usable_size.
template <bool thread_safe>
ALWAYS_INLINE size_t PartitionRoot<thread_safe>::GetUsableSize(void* ptr) {
  // malloc_usable_size() is expected to handle NULL gracefully and return 0.
  if (!ptr)
    return 0;
  auto* slot_span = SlotSpan::FromSlotInnerPtr(ptr);
  auto* root = FromSlotSpan(slot_span);
  return slot_span->GetUsableSize(root);
}

// Return the capacity of the underlying slot (adjusted for extras). This
// doesn't mean this capacity is readily available. It merely means that if
// a new allocation (or realloc) happened with that returned value, it'd use
// the same amount of underlying memory.
//
// CAUTION! For direct-mapped allocation, |ptr| has to be within the first
// partition page.
template <bool thread_safe>
ALWAYS_INLINE size_t
PartitionRoot<thread_safe>::AllocationCapacityFromPtr(void* ptr) const {
  uintptr_t address = AdjustPointerForExtrasSubtract(ptr);
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<thread_safe>(address);
  size_t size = AdjustSizeForExtrasSubtract(slot_span->bucket->slot_size);
  return size;
}

// static
template <bool thread_safe>
ALWAYS_INLINE uint16_t
PartitionRoot<thread_safe>::SizeToBucketIndex(size_t size) {
  return internal::BucketIndexLookup::GetIndex(size);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFlags(
    int flags,
    size_t requested_size,
    const char* type_name) {
  return AllocFlagsInternal(flags, requested_size, PartitionPageSize(),
                            type_name);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFlagsInternal(
    int flags,
    size_t requested_size,
    size_t slot_span_alignment,
    const char* type_name) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));

  PA_DCHECK(flags < PartitionAllocLastFlag << 1);
  PA_DCHECK((flags & PartitionAllocNoHooks) == 0);  // Internal only.
  PA_DCHECK(initialized);

#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  CHECK_MAX_SIZE_OR_RETURN_NULLPTR(requested_size, flags);
  const bool zero_fill = flags & PartitionAllocZeroFill;
  void* result = zero_fill ? calloc(1, requested_size) : malloc(requested_size);
  PA_CHECK(result || flags & PartitionAllocReturnNull);
  return result;
#else
  PA_DCHECK(initialized);
  void* ret = nullptr;
  const bool hooks_enabled = PartitionAllocHooks::AreHooksEnabled();
  if (UNLIKELY(hooks_enabled)) {
    if (PartitionAllocHooks::AllocationOverrideHookIfEnabled(
            &ret, flags, requested_size, type_name)) {
      PartitionAllocHooks::AllocationObserverHookIfEnabled(ret, requested_size,
                                                           type_name);
      return ret;
    }
  }

  ret = AllocFlagsNoHooks(flags, requested_size, slot_span_alignment);

  if (UNLIKELY(hooks_enabled)) {
    PartitionAllocHooks::AllocationObserverHookIfEnabled(ret, requested_size,
                                                         type_name);
  }

  return ret;
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFlagsNoHooks(
    int flags,
    size_t requested_size,
    size_t slot_span_alignment) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));

  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/ref-count management
  // - Before the "raw" allocator.
  //
  // That is, the general allocation flow is:
  // 1. Adjustment of requested size to make room for extras
  // 2. Allocation:
  //   a. Call to the thread cache, if it succeeds, go to step 3.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  // 3. Handle cookie/ref-count, zero allocation if required

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  PA_CHECK(raw_size >= requested_size);  // check for overflows

  uint16_t bucket_index = SizeToBucketIndex(raw_size);
  size_t usable_size;
  bool is_already_zeroed = false;
  uintptr_t slot_start = 0;
  size_t slot_size;

  const bool is_quarantine_enabled = IsQuarantineEnabled();
  // PCScan safepoint. Call before trying to allocate from cache.
  // TODO(bikineev): Change the condition to LIKELY once PCScan is enabled by
  // default.
  if (UNLIKELY(is_quarantine_enabled)) {
    PCScan::JoinScanIfNeeded();
  }

  // Don't use thread cache if higher order alignment is requested, because the
  // thread cache will not be able to satisfy it.
  //
  // LIKELY: performance-sensitive partitions use the thread cache.
  if (LIKELY(with_thread_cache && slot_span_alignment <= PartitionPageSize())) {
    auto* tcache = internal::ThreadCache::Get();
    // LIKELY: Typically always true, except for the very first allocation of
    // this thread.
    if (LIKELY(internal::ThreadCache::IsValid(tcache))) {
      slot_start = tcache->GetFromCache(bucket_index, &slot_size);
    } else {
      slot_start = MaybeInitThreadCacheAndAlloc(bucket_index, &slot_size);
    }

    // LIKELY: median hit rate in the thread cache is 95%, from metrics.
    if (LIKELY(slot_start)) {
      // This follows the logic of SlotSpanMetadata::GetUsableSize for small
      // buckets, which is too expensive to call here.
      // Keep it in sync!
      usable_size = AdjustSizeForExtrasSubtract(slot_size);

#if DCHECK_IS_ON()
      // Make sure that the allocated pointer comes from the same place it would
      // for a non-thread cache allocation.
      SlotSpan* slot_span = SlotSpan::FromSlotStart(slot_start);
      PA_DCHECK(IsValidSlotSpan(slot_span));
      PA_DCHECK(slot_span->bucket == &bucket_at(bucket_index));
      PA_DCHECK(slot_span->bucket->slot_size == slot_size);
      PA_DCHECK(usable_size == slot_span->GetUsableSize(this));
      // All large allocations must go through the RawAlloc path to correctly
      // set |usable_size|.
      PA_DCHECK(!slot_span->CanStoreRawSize());
      PA_DCHECK(!slot_span->bucket->is_direct_mapped());
#endif
    } else {
      slot_start =
          RawAlloc(buckets + bucket_index, flags, raw_size, slot_span_alignment,
                   &usable_size, &is_already_zeroed);
    }
  } else {
    slot_start =
        RawAlloc(buckets + bucket_index, flags, raw_size, slot_span_alignment,
                 &usable_size, &is_already_zeroed);
  }

  if (UNLIKELY(!slot_start))
    return nullptr;

  // Layout inside the slot:
  //  |[refcnt]|...data...|[empty]|[cookie]|[unused]|
  //           <---(a)---->
  //           <-------(b)-------->
  //  <--(c)--->                  <--(c)--->
  //  <--------(d)-------->   +   <--(d)--->
  //  <----------------(e)----------------->
  //  <---------------------(f)--------------------->
  //   (a) requested_size
  //   (b) usable_size
  //   (c) extras
  //   (d) raw_size
  //   (e) utilized_slot_size
  //   (f) slot_size
  //
  // - Ref-count may or may not exist in the slot, depending on raw_ptr<T>
  //   implementation.
  // - Cookie exists only when DCHECK is on.
  // - Think of raw_size as the minimum size required internally to satisfy
  //   the allocation request (i.e. requested_size + extras)
  // - Note, at most one "empty" or "unused" space can occur at a time. It
  //   occurs when slot_size is larger than raw_size. "unused" applies only to
  //   large allocations (direct-mapped and single-slot slot spans) and "empty"
  //   only to small allocations.
  //   Why either-or, one might ask? We make an effort to put the trailing
  //   cookie as close to data as possible to catch overflows (often
  //   off-by-one), but that's possible only if we have enough space in metadata
  //   to save raw_size, i.e. only for large allocations. For small allocations,
  //   we have no other choice than putting the cookie at the very end of the
  //   slot, thus creating the "empty" space.
  //
  // If BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) is true, Layout inside the
  // slot of small buckets:
  //  |...data...|[empty]|[cookie]|[refcnt]|
  //  <---(a)---->
  //  <-------(b)-------->
  //                     <-------(c)------->
  //  <---(d)---->   +   <-------(d)------->
  //  <----------------(e)----------------->
  //  <----------------(f)----------------->
  //
  // If the slot start address is not SystemPageSize() aligned (this also means,
  // the slot size is small), [refcnt] of this slot is stored at the end of
  // the previous slot. So this makes us to obtain refcount address with slot
  // start address minus sizeof(refcount).
  // If the slot start address is SystemPageSize() aligned (regarding single
  // slot span, the slot start address is always SystemPage size-aligned),
  // [refcnt] is stored in refcount bitmap placed after SuperPage metadata.
  // However, the space for refcnt is still reserved at the end of slot, even
  // though redundant. Because, regarding not single slot span, it is a little
  // difficult to change usable_size if refcnt serves the slot in the next
  // system page.
  // TODO(tasak): we don't need to add/subtract sizeof(refcnt) to requested size
  // in single slot span case.

  // The value given to the application is just after the ref-count.
  void* ret = AdjustPointerForExtrasAdd(slot_start);

#if DCHECK_IS_ON()
  // Add the cookie after the allocation.
  if (allow_cookie) {
    internal::PartitionCookieWriteValue(static_cast<unsigned char*>(ret) +
                                        usable_size);
  }
#endif

  // Fill the region kUninitializedByte (on debug builds, if not requested to 0)
  // or 0 (if requested and not 0 already).
  bool zero_fill = flags & PartitionAllocZeroFill;
  // LIKELY: operator new() calls malloc(), not calloc().
  if (LIKELY(!zero_fill)) {
    // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
    memset(ret, kUninitializedByte, usable_size);
#endif
  } else if (!is_already_zeroed) {
    memset(ret, 0, usable_size);
  }

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  // TODO(keishi): Add LIKELY when brp is fully enabled as |brp_enabled| will be
  // false only for the aligned partition.
  if (brp_enabled()) {
    new (internal::PartitionRefCountPointer(
        reinterpret_cast<uintptr_t>(slot_start))) internal::PartitionRefCount();
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // TODO(bikineev): Change the condition to LIKELY once PCScan is enabled by
  // default.
  if (UNLIKELY(is_quarantine_enabled)) {
    if (LIKELY(internal::IsManagedByNormalBuckets(
            reinterpret_cast<uintptr_t>(ret)))) {
      uintptr_t unmasked_slot_start =
          memory::UnmaskPtr(reinterpret_cast<uintptr_t>(slot_start));
      // Mark the corresponding bits in the state bitmap as allocated.
      internal::StateBitmapFromAddr(unmasked_slot_start)
          ->Allocate(unmasked_slot_start);
    }
  }

  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE uintptr_t
PartitionRoot<thread_safe>::RawAlloc(Bucket* bucket,
                                     int flags,
                                     size_t raw_size,
                                     size_t slot_span_alignment,
                                     size_t* usable_size,
                                     bool* is_already_zeroed) {
  ::partition_alloc::ScopedGuard guard{lock_};
  return AllocFromBucket(bucket, flags, raw_size, slot_span_alignment,
                         usable_size, is_already_zeroed);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AlignedAllocFlags(
    int flags,
    size_t alignment,
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
  // Extras before the allocation are forbidden as they shift the returned
  // allocation from the beginning of the slot, thus messing up alignment.
  // Extras after the allocation are acceptable, but they have to be taken into
  // account in the request size calculation to avoid crbug.com/1185484.
  PA_DCHECK(allow_aligned_alloc);
  PA_DCHECK(!extras_offset);
  // This is mandated by |posix_memalign()|, so should never fire.
  PA_CHECK(base::bits::IsPowerOfTwo(alignment));
  // Catch unsupported alignment requests early.
  PA_CHECK(alignment <= kMaxSupportedAlignment);
  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);

  size_t adjusted_size = requested_size;
  if (alignment <= PartitionPageSize()) {
    // Handle cases such as size = 16, alignment = 64.
    // Wastes memory when a large alignment is requested with a small size, but
    // this is hard to avoid, and should not be too common.
    if (UNLIKELY(raw_size < alignment)) {
      raw_size = alignment;
    } else {
      // PartitionAlloc only guarantees alignment for power-of-two sized
      // allocations. To make sure this applies here, round up the allocation
      // size.
      raw_size = static_cast<size_t>(1)
                 << (sizeof(size_t) * 8 -
                     base::bits::CountLeadingZeroBits(raw_size - 1));
    }
    PA_DCHECK(base::bits::IsPowerOfTwo(raw_size));
    // Adjust back, because AllocFlagsNoHooks/Alloc will adjust it again.
    adjusted_size = AdjustSizeForExtrasSubtract(raw_size);

    // Overflow check. adjusted_size must be larger or equal to requested_size.
    if (UNLIKELY(adjusted_size < requested_size)) {
      if (flags & PartitionAllocReturnNull)
        return nullptr;
      // OutOfMemoryDeathTest.AlignedAlloc requires
      // base::TerminateBecauseOutOfMemory (invoked by
      // PartitionExcessiveAllocationSize).
      internal::PartitionExcessiveAllocationSize(requested_size);
      // internal::PartitionExcessiveAllocationSize(size) causes OOM_CRASH.
      PA_NOTREACHED();
    }
  }

  // Slot spans are naturally aligned on partition page size, but make sure you
  // don't pass anything less, because it'll mess up callee's calculations.
  size_t slot_span_alignment = std::max(alignment, PartitionPageSize());
  bool no_hooks = flags & PartitionAllocNoHooks;
  void* ptr =
      no_hooks ? AllocFlagsNoHooks(0, adjusted_size, slot_span_alignment)
               : AllocFlagsInternal(0, adjusted_size, slot_span_alignment, "");

  // |alignment| is a power of two, but the compiler doesn't necessarily know
  // that. A regular % operation is very slow, make sure to use the equivalent,
  // faster form.
  PA_CHECK(!(reinterpret_cast<uintptr_t>(ptr) & (alignment - 1)));

  return ptr;
}

template <bool thread_safe>
NOINLINE void* PartitionRoot<thread_safe>::Alloc(size_t requested_size,
                                                 const char* type_name) {
  return AllocFlags(0, requested_size, type_name);
}

template <bool thread_safe>
NOINLINE void* PartitionRoot<thread_safe>::Realloc(void* ptr,
                                                   size_t new_size,
                                                   const char* type_name) {
  return ReallocFlags(0, ptr, new_size, type_name);
}

template <bool thread_safe>
NOINLINE void* PartitionRoot<thread_safe>::TryRealloc(void* ptr,
                                                      size_t new_size,
                                                      const char* type_name) {
  return ReallocFlags(PartitionAllocReturnNull, ptr, new_size, type_name);
}

// Return the capacity of the underlying slot (adjusted for extras) that'd be
// used to satisfy a request of |size|. This doesn't mean this capacity would be
// readily available. It merely means that if an allocation happened with that
// returned value, it'd use the same amount of underlying memory as the
// allocation with |size|.
template <bool thread_safe>
ALWAYS_INLINE size_t
PartitionRoot<thread_safe>::AllocationCapacityFromRequestedSize(
    size_t size) const {
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
  return size;
#else
  PA_DCHECK(PartitionRoot<thread_safe>::initialized);
  size = AdjustSizeForExtrasAdd(size);
  auto& bucket = bucket_at(SizeToBucketIndex(size));
  PA_DCHECK(!bucket.slot_size || bucket.slot_size >= size);
  PA_DCHECK(!(bucket.slot_size % kSmallestBucket));

  if (LIKELY(!bucket.is_direct_mapped())) {
    size = bucket.slot_size;
  } else if (size > MaxDirectMapped()) {
    // Too large to allocate => return the size unchanged.
  } else {
    size = GetDirectMapSlotSize(size);
  }
  size = AdjustSizeForExtrasSubtract(size);
  return size;
#endif
}

using ThreadSafePartitionRoot = PartitionRoot<internal::ThreadSafe>;

static_assert(offsetof(ThreadSafePartitionRoot, lock_) ==
                  kPartitionCachelineSize,
              "Padding is incorrect");

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
