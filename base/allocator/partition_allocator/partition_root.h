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

#include <atomic>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/address_pool_manager_types.h"
#include "base/allocator/partition_allocator/page_allocator.h"
#include "base/allocator/partition_allocator/page_allocator_constants.h"
#include "base/allocator/partition_allocator/partition_address_space.h"
#include "base/allocator/partition_allocator/partition_alloc-inl.h"
#include "base/allocator/partition_allocator/partition_alloc_check.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_alloc_hooks.h"
#include "base/allocator/partition_allocator/partition_bucket_lookup.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_oom.h"
#include "base/allocator/partition_allocator/partition_page.h"
#include "base/allocator/partition_allocator/partition_ref_count.h"
#include "base/allocator/partition_allocator/reservation_offset_table.h"
#include "base/allocator/partition_allocator/starscan/pcscan.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/compiler_specific.h"
#include "build/build_config.h"

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
BASE_EXPORT void DCheckIfManagedByPartitionAllocBRPPool(void* ptr);
#else
ALWAYS_INLINE void DCheckIfManagedByPartitionAllocBRPPool(void*) {}
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

  enum class Cookies : uint8_t {
    kDisallowed,
    kAllowed,
  };

  enum class RefCount : uint8_t {
    kDisallowed,
    kAllowed,
  };

  // Constructor to suppress aggregate initialization.
  constexpr PartitionOptions(AlignedAlloc aligned_alloc,
                             ThreadCache thread_cache,
                             Quarantine quarantine,
                             Cookies cookies,
                             RefCount ref_count)
      : aligned_alloc(aligned_alloc),
        thread_cache(thread_cache),
        quarantine(quarantine),
        cookies(cookies),
        ref_count(ref_count) {}

  AlignedAlloc aligned_alloc;
  ThreadCache thread_cache;
  Quarantine quarantine;
  Cookies cookies;
  RefCount ref_count;
};

// Never instantiate a PartitionRoot directly, instead use
// PartitionAllocator.
template <bool thread_safe>
struct BASE_EXPORT PartitionRoot {
  using SlotSpan = internal::SlotSpanMetadata<thread_safe>;
  using Page = internal::PartitionPage<thread_safe>;
  using Bucket = internal::PartitionBucket<thread_safe>;
  using SuperPageExtentEntry =
      internal::PartitionSuperPageExtentEntry<thread_safe>;
  using DirectMapExtent = internal::PartitionDirectMapExtent<thread_safe>;
  using ScopedGuard = internal::ScopedGuard<thread_safe>;
  using PCScan = internal::PCScan;

  // Defines whether objects should be quarantined for this root.
  enum class QuarantineMode : uint8_t {
    kAlwaysDisabled,
    kDisabledByDefault,
    kEnabled,
  } quarantine_mode = QuarantineMode::kAlwaysDisabled;

  // Defines whether the root should be scanned.
  enum class ScanMode : uint8_t {
    kDisabled,
    kEnabled,
  } scan_mode = ScanMode::kDisabled;

  // Flags accessed on fast paths.
  //
  // Careful! PartitionAlloc's performance is sensitive to its layout. Please
  // put the fast-path objects below, and the other ones further (see comment in
  // this file).
  bool with_thread_cache = false;
  const bool is_thread_safe = thread_safe;

  bool allow_aligned_alloc;
  bool allow_cookies;
  bool allow_ref_count;

  bool use_lazy_commit = true;

#if !defined(PA_EXTRAS_REQUIRED)
  // Teach the compiler that code can be optimized in builds that use no extras.
  static constexpr uint32_t extras_size = 0;
  static constexpr uint32_t extras_offset = 0;
#else
  uint32_t extras_size;
  uint32_t extras_offset;
#endif  // !defined(PA_EXTRAS_REQUIRED)

  // Not used on the fastest path (thread cache allocations), but on the fast
  // path of the central allocator.
  internal::MaybeSpinLock<thread_safe> lock_;

  Bucket buckets[kNumBuckets] = {};
  Bucket sentinel_bucket;

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

  char* next_super_page = nullptr;
  char* next_partition_page = nullptr;
  char* next_partition_page_end = nullptr;
  SuperPageExtentEntry* current_extent = nullptr;
  SuperPageExtentEntry* first_extent = nullptr;
  DirectMapExtent* direct_map_list GUARDED_BY(lock_) = nullptr;
  SlotSpan* global_empty_slot_span_ring[kMaxFreeableSpans] = {};
  int16_t global_empty_slot_span_ring_index = 0;

  // Integrity check = ~reinterpret_cast<uintptr_t>(this).
  uintptr_t inverted_self = 0;
  std::atomic<int> thread_caches_being_constructed_{0};

  PartitionRoot() = default;
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

  void ConfigureLazyCommit();

  ALWAYS_INLINE static bool IsValidSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSuperPage(char* super_page);
  ALWAYS_INLINE static PartitionRoot* FromPointerInNormalBuckets(char* ptr);

  ALWAYS_INLINE void IncreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecreaseCommittedPages(size_t len);
  ALWAYS_INLINE void DecommitSystemPagesForData(
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  // Commits or recommits pages for user data (i.e. inside of slot spans) and
  // updates relevant stats.
  // If committing for the first time |accessibility_disposition| must be
  // PageUpdatePermissions, otherwise must be PageKeepPermissionsIfPossible.
  ALWAYS_INLINE void RecommitSystemPagesForData(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE bool TryRecommitSystemPagesForData(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition);

  void UpdateNumPreviouslyCommittedSystemPagesIfNeeded(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition);
  void AssertNumPreviouslyCommittedSystemPages(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      void* address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition);

  [[noreturn]] NOINLINE void OutOfMemory(size_t size);

  // Returns a pointer aligned on |alignment|, or nullptr.
  //
  // |alignment| has to be a power of two and a multiple of sizeof(void*) (as in
  // posix_memalign() for POSIX systems). The returned pointer may include
  // padding, and can be passed to |Free()| later.
  //
  // NOTE: This is incompatible with anything that adds extras before the
  // returned pointer, such as cookies (with DCHECK_IS_ON()), or reference
  // count.
  ALWAYS_INLINE void* AlignedAllocFlags(int flags,
                                        size_t alignment,
                                        size_t requested_size);

  ALWAYS_INLINE void* Alloc(size_t requested_size, const char* type_name);
  ALWAYS_INLINE void* AllocFlags(int flags,
                                 size_t requested_size,
                                 const char* type_name);
  // Same as |AllocFlags()|, but allows specifying |slot_span_alignment|. It has
  // to be a multiple of partition page size, greater than 0 and no greater than
  // kMaxSupportedAlignment. If it equals exactly 1 partition page, no special
  // action is taken as PartitoinAlloc naturally guarantees this alignment,
  // otherwise a sub-optimial allocation strategy is used to guarantee the
  // higher-order alignment.
  ALWAYS_INLINE void* AllocFlagsInternal(int flags,
                                         size_t requested_size,
                                         size_t slot_span_alignment,
                                         const char* type_name);
  // Same as |AllocFlags()|, but bypasses the allocator hooks.
  //
  // This is separate from AllocFlags() because other callers of AllocFlags()
  // should not have the extra branch checking whether the hooks should be
  // ignored or not. This is the same reason why |FreeNoHooks()|
  // exists. However, |AlignedAlloc()| and |Realloc()| have few callers, so
  // taking the extra branch in the non-malloc() case doesn't hurt. In addition,
  // for the malloc() case, the compiler correctly removes the branch, since
  // this is marked |ALWAYS_INLINE|.
  ALWAYS_INLINE void* AllocFlagsNoHooks(int flags,
                                        size_t requested_size,
                                        size_t slot_span_alignment);

  ALWAYS_INLINE void* Realloc(void* ptr, size_t newize, const char* type_name);
  // Overload that may return nullptr if reallocation isn't possible. In this
  // case, |ptr| remains valid.
  ALWAYS_INLINE void* TryRealloc(void* ptr,
                                 size_t new_size,
                                 const char* type_name);
  NOINLINE void* ReallocFlags(int flags,
                              void* ptr,
                              size_t new_size,
                              const char* type_name);
  ALWAYS_INLINE static void Free(void* ptr);
  // Same as |Free()|, bypasses the allocator hooks.
  ALWAYS_INLINE static void FreeNoHooks(void* ptr);
  // Immediately frees the pointer bypassing the quarantine.
  ALWAYS_INLINE void FreeNoHooksImmediate(void* ptr, SlotSpan* slot_span);

  ALWAYS_INLINE static size_t GetUsableSize(void* ptr);

  ALWAYS_INLINE size_t AllocationCapacityFromPtr(void* ptr) const;
  ALWAYS_INLINE size_t AllocationCapacityFromRequestedSize(size_t size) const;

  // Frees memory from this partition, if possible, by decommitting pages or
  // even etnire slot spans. |flags| is an OR of base::PartitionPurgeFlags.
  void PurgeMemory(int flags);

  void DumpStats(const char* partition_name,
                 bool is_light_dump,
                 PartitionStatsDumper* partition_stats_dumper);

  void ResetBookkeepingForTesting();

  static uint16_t SizeToBucketIndex(size_t size);

  ALWAYS_INLINE internal::DeferredUnmap FreeSlotSpan(void* slot_start,
                                                     SlotSpan* slot_span)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  // Frees memory, with |slot_start| as returned by |RawAlloc()|.
  ALWAYS_INLINE void RawFree(void* slot_start);
  ALWAYS_INLINE void RawFree(void* slot_start, SlotSpan* slot_span)
      LOCKS_EXCLUDED(lock_);

  ALWAYS_INLINE void RawFreeWithThreadCache(void* slot_start,
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

  internal::pool_handle ChooseGigaCagePool(bool is_direct_map) const {
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    return allow_ref_count ? internal::GetBRPPool() : internal::GetNonBRPPool();
#else
    // When BRP isn't used, all normal bucket allocations belong to the BRP pool
    // and direct map allocations belong to non-BRP pool. PCScan requires this.
    return is_direct_map ? internal::GetNonBRPPool() : internal::GetBRPPool();
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)
  }

  ALWAYS_INLINE bool IsQuarantineAllowed() const {
    return quarantine_mode != QuarantineMode::kAlwaysDisabled;
  }

  ALWAYS_INLINE bool IsQuarantineEnabled() const {
    return quarantine_mode == QuarantineMode::kEnabled;
  }

  ALWAYS_INLINE bool IsScanEnabled() const {
    // Enabled scan implies enabled quarantine.
    PA_DCHECK(scan_mode != ScanMode::kEnabled || IsQuarantineEnabled());
    return scan_mode == ScanMode::kEnabled;
  }

  static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR ALWAYS_INLINE size_t
  GetDirectMapMetadataAndGuardPagesSize() {
    // Because we need to fake a direct-map region to look like a super page, we
    // need to allocate a bunch of system pages more around the payload:
    // - The first few system pages are the partition page in which the super
    // page metadata is stored.
    // - We add a trailing guard page (one system page will suffice).
#if !defined(PA_HAS_64_BITS_POINTERS) && BUILDFLAG(USE_BACKUP_REF_PTR)
    // On 32-bit systems, we need PartitionPageSize() guard pages at both the
    // beginning and the end of each direct-map allocated memory. This is needed
    // for the BRP pool bitmap which excludes guard pages and operates at
    // PartitionPageSize() granularity. This is to match the behavior of normal
    // buckets allocations.
    return PartitionPageSize() + PartitionPageSize();
#else
    return PartitionPageSize() + SystemPageSize();
#endif
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

// PartitionRefCount contains a cookie if slow checks are enabled or
// DCHECK_IS_ON(), which makes it 8B in size. On 32-bit architectures it fills
// the entire smallest slot, which is also 8B there.
#if (BUILDFLAG(ENABLE_BACKUP_REF_PTR_SLOW_CHECKS) || DCHECK_IS_ON()) && \
    !defined(PA_HAS_64_BITS_POINTERS)
#define PA_REF_COUNT_FILLS_ENTIRE_SMALLEST_SLOT 1
#endif

  ALWAYS_INLINE size_t AdjustSize0IfNeeded(size_t size) const {
#if BUILDFLAG(USE_BACKUP_REF_PTR) &&               \
    (!BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) || \
     defined(PA_REF_COUNT_FILLS_ENTIRE_SMALLEST_SLOT))
    // The minimum slot size is base::kAlignment. If |requested_size| is 0 and
    // there are extras only before the allocation (which must be at least
    // kAlignment), then these extras will fill the slot, leading to returning a
    // pointer to the next slot. This is a problem, because e.g. FreeNoHooks()
    // or ReallocFlags() call SlotSpan::FromSlotInnerPtr(ptr) prior to
    // subtracting extras, thus getting a wrong, possibly non-existent, slot
    // span. Fake the size to be 1 in order to counteract it.
    //
    // Having any extras after the allocation nullifies the issue, so no need
    // for this adjustment in the PUT_REF_COUNT_IN_PREVIOUS_SLOT case. Same for
    // DCHECK_IS_ON(), but we prefer not to change codepaths between Release and
    // Debug.
    //
    // In theory, this can be further refined using run-time checks. No need for
    // this adjustment if |!extras_offset || (extras_size - extras_offset)|, but
    // we prefer not to add more checks, as this function may be called on hot
    // paths.
    //
    // We use this technique in another situation. When putting refcount in the
    // previous slot, the previous slot may be free. In this case, the slot
    // needs to fit both, a free-list entry and a ref-count. If
    // sizeof(PartitionRefCount) is 8, it fills the entire smallest slot on
    // 32-bit systems (kSmallestBucket is 8). Adjusting the request size from 0
    // to 1 guarantees that we'll never allocate the smallest slot.
    if (UNLIKELY(size == 0))
      return 1;
#else
    PA_DCHECK(!extras_offset || (extras_size - extras_offset));
#if BUILDFLAG(USE_BACKUP_REF_PTR)
    constexpr size_t kRefCountSize = sizeof(internal::PartitionRefCount);
#else
    constexpr size_t kRefCountSize = 0;
#endif
    static_assert(
        sizeof(internal::EncodedPartitionFreelistEntry) + kRefCountSize <=
            kSmallestBucket,
        "Ref-count and free-list entry must fit in the smallest slot");
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR) &&
        // (!BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) ||
        // defined(PA_REF_COUNT_FILLS_ENTIRE_SMALLEST_SLOT))

#if defined(OS_APPLE) && DCHECK_IS_ON()
    // On macOS and iOS, malloc zone's `size` function is used for two purposes;
    // as a zone dispatcher and as an underlying implementation of
    // malloc_size(3).  As a zone dispatcher, `size` function must not return
    // zero as long as the given pointer belongs to this zone.  At the same
    // time, the return value of `size` function is used as the result of
    // malloc_size(3), so we have to actually allocate at least that size of
    // memory.
    //
    // When DCHECK_IS_ON() and the requested size is zero, extras occupy the
    // allocated memory entirely and the size of user data will be zero.  In
    // order to avoid an allocation of zero bytes of user data, always allocate
    // at least 1 byte memory.  When DCHECK is off, there is no extras and
    // there is no case of zero bytes of user data.
    if (UNLIKELY(size == 0))
      return 1;
#endif  // defined(OS_APPLE) && DCHECK_IS_ON()

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

  ALWAYS_INLINE void* AdjustPointerForExtrasAdd(void* ptr) const {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) +
                                   extras_offset);
  }

  ALWAYS_INLINE void* AdjustPointerForExtrasSubtract(void* ptr) const {
    return reinterpret_cast<void*>(reinterpret_cast<uintptr_t>(ptr) -
                                   extras_offset);
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
  ALWAYS_INLINE void* RawAlloc(Bucket* bucket,
                               int flags,
                               size_t raw_size,
                               size_t slot_span_alignment,
                               size_t* usable_size,
                               bool* is_already_zeroed);
  ALWAYS_INLINE void* AllocFromBucket(Bucket* bucket,
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
  ALWAYS_INLINE void RawFreeLocked(void* slot_start)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void* MaybeInitThreadCacheAndAlloc(uint16_t bucket_index, size_t* slot_size);

  friend class internal::ThreadCache;
};

namespace internal {

// Gets the SlotSpanMetadata object of the slot span that contains |ptr|. It's
// used with intention to do obtain the slot size.
//
// CAUTION! For direct-mapped allocation, |ptr| has to be within the first
// partition page.
template <bool thread_safe>
ALWAYS_INLINE internal::SlotSpanMetadata<thread_safe>*
PartitionAllocGetSlotSpanForSizeQuery(void* ptr) {
  // No need to lock here. Only |ptr| being freed by another thread could
  // cause trouble, and the caller is responsible for that not happening.
  auto* slot_span =
      internal::SlotSpanMetadata<thread_safe>::FromSlotInnerPtr(ptr);
  // TODO(palmer): See if we can afford to make this a CHECK.
  PA_DCHECK(PartitionRoot<thread_safe>::IsValidSlotSpan(slot_span));
  return slot_span;
}

ALWAYS_INLINE void* PartitionAllocGetDirectMapSlotStart(void* ptr) {
  uintptr_t reservation_start = GetDirectMapReservationStart(ptr);
  if (!reservation_start)
    return nullptr;

  // The direct map allocation may not start exactly from the first page, as
  // there may be padding for alignment. The first page metadata holds an offset
  // to where direct map metadata, and thus direct map start, are located.
  auto* first_page = PartitionPage<ThreadSafe>::FromPtr(
      reinterpret_cast<void*>(reservation_start + PartitionPageSize()));
  auto* page = first_page + first_page->slot_span_metadata_offset;
  PA_DCHECK(page->is_valid);
  PA_DCHECK(!page->slot_span_metadata_offset);
  auto* ret = SlotSpanMetadata<ThreadSafe>::ToSlotSpanStartPtr(
      &page->slot_span_metadata);
#if DCHECK_IS_ON()
  auto* metadata =
      reinterpret_cast<PartitionDirectMapMetadata<ThreadSafe>*>(page);
  size_t padding_for_alignment =
      metadata->direct_map_extent.padding_for_alignment;
  PA_DCHECK(padding_for_alignment == (page - first_page) * PartitionPageSize());
  PA_DCHECK(ret ==
            reinterpret_cast<void*>(reservation_start + PartitionPageSize() +
                                    padding_for_alignment));
#endif  // DCHECK_IS_ON()
  return ret;
}

#if BUILDFLAG(USE_BACKUP_REF_PTR)

// Gets the pointer to the beginning of the allocated slot.
//
// This isn't a general purpose function, it is used specifically for obtaining
// BackupRefPtr's ref-count. The caller is responsible for ensuring that the
// ref-count is in place for this allocation.
//
// This function is not a template, and can be used on either variant
// (thread-safe or not) of the allocator. This relies on the two PartitionRoot<>
// having the same layout, which is enforced by static_assert().
ALWAYS_INLINE void* PartitionAllocGetSlotStart(void* ptr) {
  // Adjust to support pointers right past the end of an allocation, which in
  // some cases appear to point outside the designated allocation slot.
  //
  // If ref-count is present before the allocation, then adjusting a valid
  // pointer down will not cause us to go down to the previous slot, otherwise
  // no adjustment is needed (and likely wouldn't be correct as there is
  // a risk of going down to the previous slot). Either way,
  // kPartitionPastAllocationAdjustment takes care of that detail.
  ptr = reinterpret_cast<char*>(ptr) - kPartitionPastAllocationAdjustment;

  PA_DCHECK(IsManagedByNormalBucketsOrDirectMap(ptr));
  DCheckIfManagedByPartitionAllocBRPPool(ptr);

  void* directmap_slot_start = PartitionAllocGetDirectMapSlotStart(ptr);
  if (UNLIKELY(directmap_slot_start))
    return directmap_slot_start;
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          ptr);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->allow_ref_count);

  // Get the offset from the beginning of the slot span.
  uintptr_t ptr_addr = reinterpret_cast<uintptr_t>(ptr);
  uintptr_t slot_span_start = reinterpret_cast<uintptr_t>(
      internal::SlotSpanMetadata<internal::ThreadSafe>::ToSlotSpanStartPtr(
          slot_span));
  size_t offset_in_slot_span = ptr_addr - slot_span_start;

  auto* bucket = slot_span->bucket;
  return reinterpret_cast<void*>(
      slot_span_start +
      bucket->slot_size * bucket->GetSlotNumber(offset_in_slot_span));
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
ALWAYS_INLINE bool PartitionAllocIsValidPtrDelta(void* ptr, ptrdiff_t delta) {
  // Required for pointers right past an allocation. See
  // |PartitionAllocGetSlotStart()|.
  void* adjusted_ptr =
      reinterpret_cast<char*>(ptr) - kPartitionPastAllocationAdjustment;

  internal::DCheckIfManagedByPartitionAllocBRPPool(adjusted_ptr);

  void* directmap_old_slot_start =
      PartitionAllocGetDirectMapSlotStart(adjusted_ptr);
  if (UNLIKELY(directmap_old_slot_start)) {
    void* new_slot_start = PartitionAllocGetDirectMapSlotStart(
        reinterpret_cast<char*>(ptr) + delta);
    return directmap_old_slot_start == new_slot_start;
  }
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<internal::ThreadSafe>(
          adjusted_ptr);
  auto* root = PartitionRoot<internal::ThreadSafe>::FromSlotSpan(slot_span);
  // Double check that ref-count is indeed present.
  PA_DCHECK(root->allow_ref_count);

  uintptr_t user_data_start = reinterpret_cast<uintptr_t>(
      root->AdjustPointerForExtrasAdd(PartitionAllocGetSlotStart(ptr)));
  size_t user_data_size = slot_span->GetUsableSize(root);
  uintptr_t new_ptr = reinterpret_cast<uintptr_t>(ptr) + delta;

  return user_data_start <= new_ptr &&
         // We use "greater then or equal" below because we want to include
         // pointers right past the end of an allocation.
         new_ptr <= user_data_start + user_data_size;
}

// TODO(glazunov): Simplify the function once the non-thread-safe PartitionRoot
// is no longer used.
ALWAYS_INLINE void PartitionAllocFreeForRefCounting(void* slot_start) {
  PA_DCHECK(!internal::PartitionRefCountPointer(slot_start)->IsAlive());

  auto* slot_span = SlotSpanMetadata<ThreadSafe>::FromSlotStartPtr(slot_start);
  auto* root = PartitionRoot<ThreadSafe>::FromSlotSpan(slot_span);
  // PartitionRefCount is required to be allocated inside a `PartitionRoot` that
  // supports reference counts.
  PA_DCHECK(root->allow_ref_count);

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(slot_start, kFreedByte,
         slot_span->GetUtilizedSlotSize()
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#endif

  if (root->is_thread_safe) {
    root->RawFreeWithThreadCache(slot_start, slot_span);
    return;
  }

  auto* non_thread_safe_slot_span =
      reinterpret_cast<SlotSpanMetadata<NotThreadSafe>*>(slot_span);
  auto* non_thread_safe_root =
      reinterpret_cast<PartitionRoot<NotThreadSafe>*>(root);
  non_thread_safe_root->RawFreeWithThreadCache(slot_start,
                                               non_thread_safe_slot_span);
}
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

}  // namespace internal

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::AllocFromBucket(
    Bucket* bucket,
    int flags,
    size_t raw_size,
    size_t slot_span_alignment,
    size_t* usable_size,
    bool* is_already_zeroed) {
  PA_DCHECK((slot_span_alignment >= PartitionPageSize()) &&
            bits::IsPowerOfTwo(slot_span_alignment));
  SlotSpan* slot_span = bucket->active_slot_spans_head;
  // Check that this slot span is neither full nor freed.
  PA_DCHECK(slot_span);
  PA_DCHECK(slot_span->num_allocated_slots >= 0);

  void* slot_start = slot_span->freelist_head;
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

    // If these DCHECKs fire, you probably corrupted memory. TODO(palmer): See
    // if we can afford to make these CHECKs.
    PA_DCHECK(IsValidSlotSpan(slot_span));

    // All large allocations must go through the slow path to correctly update
    // the size metadata.
    PA_DCHECK(!slot_span->CanStoreRawSize());
    PA_DCHECK(!slot_span->bucket->is_direct_mapped());
    internal::PartitionFreelistEntry* new_head =
        slot_span->freelist_head->GetNext(bucket->slot_size);
    slot_span->SetFreelistHead(new_head);
    slot_span->num_allocated_slots++;

    PA_DCHECK(slot_span->bucket == bucket);
  } else {
    slot_start = bucket->SlowPathAlloc(this, flags, raw_size,
                                       slot_span_alignment, is_already_zeroed);
    if (UNLIKELY(!slot_start))
      return nullptr;

    slot_span = SlotSpan::FromSlotStartPtr(slot_start);
    // TODO(palmer): See if we can afford to make this a CHECK.
    PA_DCHECK(IsValidSlotSpan(slot_span));
    // For direct mapped allocations, |bucket| is the sentinel.
    PA_DCHECK((slot_span->bucket == bucket) ||
              (slot_span->bucket->is_direct_mapped() &&
               (bucket == &sentinel_bucket)));

    *usable_size = slot_span->GetUsableSize(this);
  }
  PA_DCHECK(slot_span->GetUtilizedSlotSize() <= slot_span->bucket->slot_size);
  total_size_of_allocated_bytes += slot_span->GetSizeForBookkeeping();
  max_size_of_allocated_bytes =
      std::max(max_size_of_allocated_bytes, total_size_of_allocated_bytes);
  return slot_start;
}

// static
template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::Free(void* ptr) {
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

  // On Android, malloc() interception is more fragile than on other
  // platforms, as we use wrapped symbols. However, the GigaCage allows us to
  // quickly tell that a pointer was allocated with PartitionAlloc.
  //
  // This is a crash to detect imperfect symbol interception. However, we can
  // forward allocations we don't own to the system malloc() implementation in
  // these rare cases, assuming that some remain.
#if defined(OS_ANDROID) && BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  PA_CHECK(IsManagedByPartitionAlloc(ptr));
#endif

  // Call FromSlotInnerPtr instead of FromSlotStartPtr because the pointer
  // hasn't been adjusted yet.
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

  // TODO(palmer): See if we can afford to make this a CHECK.
  PA_DCHECK(IsValidSlotSpan(slot_span));
  auto* root = FromSlotSpan(slot_span);

  // TODO(bikineev): Change the condition to LIKELY once PCScan is enabled by
  // default.
  if (UNLIKELY(root->IsQuarantineEnabled())) {
    // PCScan safepoint. Call before potentially scheduling scanning task.
    PCScan::JoinScanIfNeeded();
    if (LIKELY(!root->IsDirectMappedBucket(slot_span->bucket))) {
      PCScan::MoveToQuarantine(ptr, slot_span->GetUsableSize(root),
                               slot_span->bucket->slot_size);
      return;
    }
  }

  root->FreeNoHooksImmediate(ptr, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::FreeNoHooksImmediate(
    void* ptr,
    SlotSpan* slot_span) {
  // The thread cache is added "in the middle" of the main allocator, that is:
  // - After all the cookie/ref-count management
  // - Before the "raw" allocator.
  //
  // On the deallocation side:
  // 1. Check cookies/ref-count, adjust the pointer
  // 2. Deallocation
  //   a. Return to the thread cache if possible. If it succeeds, return.
  //   b. Otherwise, call the "raw" allocator <-- Locking
  PA_DCHECK(ptr);
  PA_DCHECK(slot_span);
  PA_DCHECK(IsValidSlotSpan(slot_span));

  // |ptr| points after the ref-count and the cookie.
  //
  // Layout inside the slot:
  //  <------extras----->                  <-extras->
  //  <--------------utilized_slot_size------------->
  //                    <----usable_size--->
  //  |[refcnt]|[cookie]|...data...|[empty]|[cookie]|[unused]|
  //                    ^
  //                   ptr
  //
  // Note: ref-count and cookies can be 0-sized.
  //
  // For more context, see the other "Layout inside the slot" comment below.
#if EXPENSIVE_DCHECKS_ARE_ON() || defined(PA_ZERO_RANDOMLY_ON_FREE)
  const size_t utilized_slot_size = slot_span->GetUtilizedSlotSize();
#endif
#if BUILDFLAG(USE_BACKUP_REF_PTR) || DCHECK_IS_ON()
  const size_t usable_size = slot_span->GetUsableSize(this);
#endif
  void* slot_start = AdjustPointerForExtrasSubtract(ptr);

#if DCHECK_IS_ON()
  if (allow_cookies) {
    // Verify 2 cookies surrounding the allocated region.
    // If these asserts fire, you probably corrupted memory.
    char* char_ptr = static_cast<char*>(ptr);
    internal::PartitionCookieCheckValue(char_ptr - internal::kCookieSize);
    internal::PartitionCookieCheckValue(char_ptr + usable_size);
  }
#endif

#if BUILDFLAG(USE_BACKUP_REF_PTR)
  if (allow_ref_count) {
    auto* ref_count = internal::PartitionRefCountPointer(slot_start);
    // If there are no more references to the allocation, it can be freed
    // immediately. Otherwise, defer the operation and zap the memory to turn
    // potential use-after-free issues into unexploitable crashes.
    if (UNLIKELY(!ref_count->IsAliveWithNoKnownRefs()))
      internal::SecureMemset(ptr, kQuarantinedByte, usable_size);

    if (UNLIKELY(!(ref_count->ReleaseFromAllocator())))
      return;
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  // memset() can be really expensive.
#if EXPENSIVE_DCHECKS_ARE_ON()
  memset(slot_start, kFreedByte,
         utilized_slot_size
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
             - sizeof(internal::PartitionRefCount)
#endif
  );
#elif defined(PA_ZERO_RANDOMLY_ON_FREE)
  // `memset` only once in a while: we're trading off safety for time
  // efficiency.
  if (UNLIKELY(internal::RandomPeriod()) &&
      !IsDirectMappedBucket(slot_span->bucket)) {
    internal::SecureMemset(slot_start, 0,
                           utilized_slot_size
#if BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT)
                               - sizeof(internal::PartitionRefCount)
#endif
    );
  }
#endif  // defined(PA_ZERO_RANDOMLY_ON_FREE)

  RawFreeWithThreadCache(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE internal::DeferredUnmap PartitionRoot<thread_safe>::FreeSlotSpan(
    void* slot_start,
    SlotSpan* slot_span) {
  total_size_of_allocated_bytes -= slot_span->GetSizeForBookkeeping();
  return slot_span->Free(slot_start);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(void* slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
  RawFree(slot_start, slot_span);
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFree(void* slot_start,
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

  internal::DeferredUnmap deferred_unmap;
  {
    ScopedGuard guard{lock_};
    deferred_unmap = FreeSlotSpan(slot_start, slot_span);
  }
  deferred_unmap.Run();
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeWithThreadCache(
    void* slot_start,
    SlotSpan* slot_span) {
  // TLS access can be expensive, do a cheap local check first.
  //
  // Also the thread-unsafe variant doesn't have a use for a thread cache, so
  // make it statically known to the compiler.
  //
  // LIKELY: performance-sensitive thread-safe partitions have a thread cache,
  // direct-mapped allocations are uncommon.
  if (thread_safe &&
      LIKELY(with_thread_cache && !IsDirectMappedBucket(slot_span->bucket))) {
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
ALWAYS_INLINE void PartitionRoot<thread_safe>::RawFreeLocked(void* slot_start) {
  SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
  auto deferred_unmap = FreeSlotSpan(slot_start, slot_span);
  // Only used with bucketed allocations.
  PA_DCHECK(!deferred_unmap.reservation_start);
  deferred_unmap.Run();
}

// static
template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::IsValidSlotSpan(
    SlotSpan* slot_span) {
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
PartitionRoot<thread_safe>::FromSuperPage(char* super_page) {
  auto* extent_entry = reinterpret_cast<SuperPageExtentEntry*>(
      internal::PartitionSuperPageToMetadataArea(super_page));
  PartitionRoot* root = extent_entry->root;
  PA_DCHECK(root->inverted_self == ~reinterpret_cast<uintptr_t>(root));
  return root;
}

template <bool thread_safe>
ALWAYS_INLINE PartitionRoot<thread_safe>*
PartitionRoot<thread_safe>::FromPointerInNormalBuckets(char* ptr) {
  PA_DCHECK(internal::IsManagedByNormalBuckets(ptr));
  char* super_page = reinterpret_cast<char*>(reinterpret_cast<uintptr_t>(ptr) &
                                             kSuperPageBaseMask);
  return FromSuperPage(super_page);
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
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  DecommitSystemPages(address, length, accessibility_disposition);
  DecreaseCommittedPages(length);
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::
    UpdateNumPreviouslyCommittedSystemPagesIfNeeded(
        internal::SlotSpanMetadata<thread_safe>* slot_span,
        size_t length,
        PageAccessibilityDisposition accessibility_disposition) {
  if (accessibility_disposition == PageUpdatePermissions &&
      !slot_span->bucket->is_direct_mapped()) {
    // It is the caller's responsibility to use PageUpdatePermissions only on
    // pages that have never been committed in the past. This requirement
    // doesn't apply to direct map and single-slot spans.
    slot_span->IncreasePreviouslyCommittedSize(length);
#if DCHECK_IS_ON()
    size_t num_uncommitted_slots =
        use_lazy_commit ? slot_span->num_unprovisioned_slots : 0;
    size_t num_committed_slots =
        slot_span->bucket->get_slots_per_span() - num_uncommitted_slots;
    PA_DCHECK(slot_span->GetPreviouslyCommittedSize() ==
              bits::AlignUp(num_committed_slots * slot_span->bucket->slot_size,
                            SystemPageSize()));
#endif
  }
}

template <bool thread_safe>
void PartitionRoot<thread_safe>::AssertNumPreviouslyCommittedSystemPages(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
#if DCHECK_IS_ON()
  if (slot_span->bucket->is_direct_mapped())
    return;

  char* base = reinterpret_cast<char*>(
      internal::SlotSpanMetadata<thread_safe>::ToSlotSpanStartPtr(slot_span));
  size_t previously_committed_size = slot_span->GetPreviouslyCommittedSize();
  char* previously_committed_watermark = base + previously_committed_size;
  if (accessibility_disposition == PageUpdatePermissions) {
    PA_DCHECK(address == previously_committed_watermark);
  } else {
    PA_DCHECK(address <= previously_committed_watermark - length);
  }
#endif  // DCHECK_IS_ON()
}

template <bool thread_safe>
ALWAYS_INLINE void PartitionRoot<thread_safe>::RecommitSystemPagesForData(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  AssertNumPreviouslyCommittedSystemPages(slot_span, address, length,
                                          accessibility_disposition);
  RecommitSystemPages(address, length, PageReadWrite,
                      accessibility_disposition);
  IncreaseCommittedPages(length);
  UpdateNumPreviouslyCommittedSystemPagesIfNeeded(slot_span, length,
                                                  accessibility_disposition);
}

template <bool thread_safe>
ALWAYS_INLINE bool PartitionRoot<thread_safe>::TryRecommitSystemPagesForData(
    internal::SlotSpanMetadata<thread_safe>* slot_span,
    void* address,
    size_t length,
    PageAccessibilityDisposition accessibility_disposition) {
  AssertNumPreviouslyCommittedSystemPages(slot_span, address, length,
                                          accessibility_disposition);
  bool ok = TryRecommitSystemPages(address, length, PageReadWrite,
                                   accessibility_disposition);
  if (ok) {
    IncreaseCommittedPages(length);
    UpdateNumPreviouslyCommittedSystemPagesIfNeeded(slot_span, length,
                                                    accessibility_disposition);
  }

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
  ptr = AdjustPointerForExtrasSubtract(ptr);
  auto* slot_span =
      internal::PartitionAllocGetSlotSpanForSizeQuery<thread_safe>(ptr);
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
  // 3. Handle cookies/ref-count, zero allocation if required

  size_t raw_size = AdjustSizeForExtrasAdd(requested_size);
  PA_CHECK(raw_size >= requested_size);  // check for overflows

  uint16_t bucket_index = SizeToBucketIndex(raw_size);
  size_t usable_size;
  bool is_already_zeroed = false;
  void* slot_start = nullptr;
  size_t slot_size;

  // PCScan safepoint. Call before trying to allocate from cache.
  if (IsQuarantineEnabled())
    PCScan::JoinScanIfNeeded();

  // !thread_safe => !with_thread_cache, but adding the condition allows the
  // compiler to statically remove this branch for the thread-unsafe variant.
  //
  // Don't use thread cache if higher order alignment is requested, because the
  // thread cache will not be able to satisfy it.
  //
  // LIKELY: performance-sensitive partitions are either thread-unsafe or use
  // the thread cache.
  if (thread_safe &&
      LIKELY(with_thread_cache && slot_span_alignment <= PartitionPageSize())) {
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
      SlotSpan* slot_span = SlotSpan::FromSlotStartPtr(slot_start);
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
  //  |[refcnt]|[cookie]|...data...|[empty]|[cookie]|[unused]|
  //                    <---(a)---->
  //                    <-------(b)-------->
  //  <-------(c)------->                  <--(c)--->
  //  <-------------(d)------------>   +   <--(d)--->
  //  <---------------------(e)--------------------->
  //  <-------------------------(f)-------------------------->
  //   (a) requested_size
  //   (b) usable_size
  //   (c) extras
  //   (d) raw_size
  //   (e) utilized_slot_size
  //   (f) slot_size
  //
  // - Ref-count may or may not exist in the slot, depending on raw_ptr<T>
  //   implementation.
  // - Cookies exist only when DCHECK is on.
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
  //  |[cookie]|...data...|[empty]|[cookie]|[refcnt]|
  //           <---(a)---->
  //           <-------(b)-------->
  //  <--(c)--->                  <-------(c)------->
  //  <---------(d)------->   +   <-------(d)------->
  //  <------------------(e)------------------------>
  //  <------------------(f)------------------------>
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

  // The value given to the application is just after the ref-count and cookie,
  // or the cookie (BUILDFLAG(PUT_REF_COUNT_IN_PREVIOUS_SLOT) is true).
  void* ret = AdjustPointerForExtrasAdd(slot_start);

#if DCHECK_IS_ON()
  // Surround the region with 2 cookies.
  if (allow_cookies) {
    char* char_ret = static_cast<char*>(ret);
    internal::PartitionCookieWriteValue(char_ret - internal::kCookieSize);
    internal::PartitionCookieWriteValue(char_ret + usable_size);
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
  // LIKELY: |allow_ref_count| is false only for the aligned partition.
  if (LIKELY(allow_ref_count)) {
    new (internal::PartitionRefCountPointer(slot_start))
        internal::PartitionRefCount();
  }
#endif  // BUILDFLAG(USE_BACKUP_REF_PTR)

  return ret;
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::RawAlloc(
    Bucket* bucket,
    int flags,
    size_t raw_size,
    size_t slot_span_alignment,
    size_t* usable_size,
    bool* is_already_zeroed) {
  internal::ScopedGuard<thread_safe> guard{lock_};
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
      NOTREACHED();
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
ALWAYS_INLINE void* PartitionRoot<thread_safe>::Alloc(size_t requested_size,
                                                      const char* type_name) {
  return AllocFlags(0, requested_size, type_name);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::Realloc(void* ptr,
                                                        size_t new_size,
                                                        const char* type_name) {
  return ReallocFlags(0, ptr, new_size, type_name);
}

template <bool thread_safe>
ALWAYS_INLINE void* PartitionRoot<thread_safe>::TryRealloc(
    void* ptr,
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
using ThreadUnsafePartitionRoot = PartitionRoot<internal::NotThreadSafe>;

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
