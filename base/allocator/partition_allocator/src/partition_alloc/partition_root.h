// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ROOT_H_
#define PARTITION_ALLOC_PARTITION_ROOT_H_

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
#include <cstddef>
#include <cstdint>
#include <limits>
#include <utility>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/free_hint.h"
#include "partition_alloc/page_allocator.h"
#include "partition_alloc/partition_alloc_allocation_data.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_base/thread_annotations.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_forward.h"
#include "partition_alloc/partition_bucket.h"
#include "partition_alloc/partition_lock.h"
#include "partition_alloc/reservation_offset_table.h"
#include "partition_alloc/scheduler_loop_quarantine.h"
#include "partition_alloc/thread_cache.h"

// When a memory tool is replacing malloc to keep aligned behaviour working we
// use window's aligned_malloc and aligned_free, but otherwise we need memalign.
#if defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
#if PA_BUILDFLAG(PA_COMPILER_MSVC)
#include <malloc.h>
#else
#include <stdlib.h>
#endif  // PA_BUILDFLAG(PA_COMPILER_MSVC)
#endif  // defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {

class BatchFreeQueue;
class PartitionRootEnumerator;
struct SlotSpanMetadata;
struct PartitionDirectMapExtent;
class InSlotMetadata;
class FreelistEntry;
class ThreadCache;

}  // namespace partition_alloc::internal

namespace partition_alloc {

// Bit flag constants used to purge memory.  See PartitionRoot::PurgeMemory.
//
// In order to support bit operations like `flag_a | flag_b`, the old-fashioned
// enum (+ surrounding named struct) is used instead of enum class.
struct PurgeFlags {
  enum : int {
    // Decommitting the ring list of empty slot spans is reasonably fast.
    kDecommitEmptySlotSpans = 1 << 0,
    // Discarding unused system pages is slower, because it involves walking all
    // freelists in all active slot spans of all buckets_ >= system page
    // size. It often frees a similar amount of memory to decommitting the empty
    // slot spans, though.
    kDiscardUnusedSystemPages = 1 << 1,
    // Aggressively reclaim memory. This is meant to be used in low-memory
    // situations, not for periodic memory reclaiming.
    kAggressiveReclaim = 1 << 2,
    // Limit the total duration of reclaim to 2ms, then return even if reclaim
    // is incomplete.
    kLimitDuration = 1 << 3,
  };
};

// Options struct used to configure PartitionRoot and PartitionAllocator.
struct PartitionOptions {
  // Marked inline so that the chromium style plugin doesn't complain that a
  // "complex constructor" has an inline body. This warning is disabled when
  // the constructor is explicitly marked "inline". Note that this is a false
  // positive of the plugin, since constexpr implies inline.
  inline constexpr PartitionOptions();
  inline constexpr PartitionOptions(const PartitionOptions& other);
  inline PA_CONSTEXPR_DTOR ~PartitionOptions();

  enum class AllowToggle : uint8_t {
    kDisallowed,
    kAllowed,
  };
  enum class EnableToggle : uint8_t {
    kDisabled,
    kEnabled,
  };

  // Expose the enum arms directly at the level of `PartitionOptions`,
  // since the variant names are already sufficiently descriptive.
  static constexpr auto kAllowed = AllowToggle::kAllowed;
  static constexpr auto kDisallowed = AllowToggle::kDisallowed;
  static constexpr auto kDisabled = EnableToggle::kDisabled;
  static constexpr auto kEnabled = EnableToggle::kEnabled;

  // Partitions with a thread cache cannot be destroyed.
  EnableToggle thread_cache = kDisabled;
  size_t thread_cache_index = internal::kInvalidThreadCacheIndex;
  EnableToggle use_cookie_if_supported = kEnabled;
  EnableToggle backup_ref_ptr = kDisabled;
  AllowToggle use_configurable_pool = kDisallowed;

  // TODO(https://crbug.com/371135823): Remove after the investigation.
  size_t backup_ref_ptr_extra_extras_size = 0;

  // Configuration for the global quarantine branch. Used when a thread local
  // instance is not available.
  internal::SchedulerLoopQuarantineConfig
      scheduler_loop_quarantine_global_config;
  // Configuration for the thread-local quarantine branch. Associated with
  // each `ThreadCache` instance.
  internal::SchedulerLoopQuarantineConfig
      scheduler_loop_quarantine_thread_local_config;
  // Configuration for the AMSC quarantine branch. Used when
  // `FreeFlags::kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks` is
  // specified.
  internal::SchedulerLoopQuarantineConfig
      scheduler_loop_quarantine_for_advanced_memory_safety_checks_config;

  // As the name implies, this is not a security measure, as there is no
  // guarantee that memorys has been zeroed out when handed back to the
  // application, or when free() returns. This is intended to improve the
  // compression ratio of freed memory inside partially allocated pages (due to
  // fragmentation).
  EnableToggle eventually_zero_freed_memory = kDisabled;

  struct {
    EnableToggle enabled = kDisabled;
    EnableToggle random_memory_tagging = kDisabled;
    TagViolationReportingMode reporting_mode =
        TagViolationReportingMode::kUndefined;
  } memory_tagging;
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  ThreadIsolationOption thread_isolation;
#endif

  EnableToggle free_with_size = kDisabled;
  EnableToggle strict_free_size_check = kEnabled;
};

constexpr PartitionOptions::PartitionOptions() = default;
constexpr PartitionOptions::PartitionOptions(const PartitionOptions& other) =
    default;
PA_CONSTEXPR_DTOR PartitionOptions::~PartitionOptions() = default;

// When/if free lists should be "straightened" when calling
// PartitionRoot::PurgeMemory(..., accounting_only=false).
enum class StraightenLargerSlotSpanFreeListsMode {
  kNever,
  kOnlyWhenUnprovisioning,
  kAlways,
};

// Never instantiate a PartitionRoot directly, instead use
// PartitionAllocator.
class alignas(64) PA_COMPONENT_EXPORT(PARTITION_ALLOC) PartitionRoot {
 public:
  using SlotSpanMetadata = internal::SlotSpanMetadata;
  using Bucket = internal::PartitionBucket;
  using FreeListEntry = internal::FreelistEntry;
  using SuperPageExtentEntry = internal::PartitionSuperPageExtentEntry;
  using DirectMapExtent = internal::PartitionDirectMapExtent;

  enum class BucketDistribution : uint8_t { kNeutral, kDenser };

  // Root settings_ accessed on fast paths.
  //
  // Careful! PartitionAlloc's performance is sensitive to its layout.  Please
  // put the fast-path objects in the struct below.
  struct alignas(internal::kPartitionCachelineSize) Settings {
    // Chromium-style: Complex constructor needs an explicit out-of-line
    // constructor.
    Settings();

    // It's important to default to the 'neutral' distribution, otherwise a
    // switch from 'dense' -> 'neutral' would leave some buckets_ with dirty
    // memory forever, since no memory would be allocated from these, their
    // freelist would typically not be empty, making these unreclaimable.
    BucketDistribution bucket_distribution = BucketDistribution::kNeutral;

    bool with_thread_cache = false;
    size_t thread_cache_index = internal::kInvalidThreadCacheIndex;

#if PA_BUILDFLAG(USE_PARTITION_COOKIE)
    bool use_cookie = true;
#else
    static constexpr bool use_cookie = false;
#endif  // PA_BUILDFLAG(USE_PARTITION_COOKIE)
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    bool brp_enabled_ = false;
    size_t in_slot_metadata_size = 0;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

    internal::pool_handle pool_handle = internal::pool_handle::kNullPoolHandle;
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    internal::PoolOffsetLookup offset_lookup;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
    internal::ReservationOffsetTable reservation_offset_table;

    bool eventually_zero_freed_memory = false;
    internal::SchedulerLoopQuarantineConfig
        scheduler_loop_quarantine_thread_local_config;
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
    bool memory_tagging_enabled_ = false;
    bool use_random_memory_tagging_ = false;
    TagViolationReportingMode memory_tagging_reporting_mode_ =
        TagViolationReportingMode::kUndefined;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
    ThreadIsolationOption thread_isolation;
#endif

#if PA_CONFIG(EXTRAS_REQUIRED)
    uint32_t extras_size = 0;
#else
    // Teach the compiler that code can be optimized in builds that use no
    // extras.
    static inline constexpr uint32_t extras_size = 0;
#endif  // PA_CONFIG(EXTRAS_REQUIRED)

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
    std::ptrdiff_t metadata_offset_ = 0;
#endif

    bool enable_free_with_size = false;
    bool enable_strict_free_size_check = true;
  };

  Settings settings_;

  // Not used on the fastest path (thread cache allocations), but on the fast
  // path of the central allocator.
  alignas(internal::kPartitionCachelineSize) internal::Lock lock_;

  // Add last bucket as sentinel.
  Bucket buckets_[BucketIndexLookup::kNumBuckets] = {};
  Bucket sentinel_bucket_{};

  // All fields below this comment are not accessed on the fast path.
  bool initialized_ = false;

  // Bookkeeping.
  // - total_size_of_super_pages_ - total virtual address space for normal
  // bucket
  //     super pages
  // - total_size_of_direct_mapped_pages_ - total virtual address space for
  //     direct-map regions
  // - total_size_of_committed_pages_ - total committed pages for slots (doesn't
  //     include metadata, bitmaps (if any), or any data outside or regions
  //     described in #1 and #2)
  // Invariant: total_size_of_allocated_bytes_ <=
  //            total_size_of_committed_pages_ <
  //                total_size_of_super_pages_ +
  //                total_size_of_direct_mapped_pages_.
  // Invariant: total_size_of_committed_pages_ <= max_size_of_committed_pages_.
  // Invariant: total_size_of_allocated_bytes_ <= max_size_of_allocated_bytes_.
  // Invariant: max_size_of_allocated_bytes_ <= max_size_of_committed_pages_.
  // Since all operations on the atomic variables have relaxed semantics, we
  // don't check these invariants with DCHECKs.
  std::atomic<size_t> total_size_of_committed_pages_{0};
  std::atomic<size_t> max_size_of_committed_pages_{0};
  std::atomic<size_t> total_size_of_super_pages_{0};
  std::atomic<size_t> total_size_of_direct_mapped_pages_{0};
  std::atomic<size_t> total_size_of_allocated_bytes_{0};
  std::atomic<size_t> max_size_of_allocated_bytes_{0};
  // Atomic, because system calls can be made without the lock held.
  std::atomic<uint64_t> syscall_count_;
  std::atomic<uint64_t> syscall_total_time_ns_;
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  // TODO(https://crbug.com/475729821): Remove PDFium reference to this variable
  // and rename this to `total_size_of_brp_quarantined_bytes_`.
  std::atomic<size_t> total_size_of_brp_quarantined_bytes{0};
  std::atomic<size_t> total_count_of_brp_quarantined_slots_{0};
  std::atomic<size_t> cumulative_size_of_brp_quarantined_bytes_{0};
  std::atomic<size_t> cumulative_count_of_brp_quarantined_slots_{0};
#endif
  // Slot span memory which has been provisioned, and is currently unused as
  // it's part of an empty SlotSpan. This is not clean memory, since it has
  // either been used for a memory allocation, and/or contains freelist
  // entries. But it might have been moved to swap. Note that all this memory
  // can be decommitted at any time.
  size_t empty_slot_spans_dirty_bytes_
      PA_GUARDED_BY(internal::PartitionRootLock(this)) = 0;

  // Only tolerate up to |total_size_of_committed_pages_ >>
  // max_empty_slot_spans_dirty_bytes_shift_| dirty bytes in empty slot
  // spans. That is, the default value of 3 tolerates up to 1/8. Since
  // |empty_slot_spans_dirty_bytes_| is never strictly larger than
  // total_size_of_committed_pages_, setting this to 0 removes the cap. This is
  // useful to make tests deterministic and easier to reason about.
  int max_empty_slot_spans_dirty_bytes_shift_ = 3;

  uintptr_t next_super_page_ = 0;
  uintptr_t next_partition_page_ = 0;
  uintptr_t next_partition_page_end_ = 0;
  SuperPageExtentEntry* current_extent_ = nullptr;
  SuperPageExtentEntry* first_extent_ = nullptr;
  DirectMapExtent* direct_map_list_
      PA_GUARDED_BY(internal::PartitionRootLock(this)) = nullptr;
  SlotSpanMetadata* global_empty_slot_span_ring_
      [internal::kMaxEmptySlotSpanRingSize] PA_GUARDED_BY(
          internal::PartitionRootLock(this)) = {};
  int16_t global_empty_slot_span_ring_index_
      PA_GUARDED_BY(internal::PartitionRootLock(this)) = 0;
  int16_t global_empty_slot_span_ring_size_
      PA_GUARDED_BY(internal::PartitionRootLock(this)) =
          internal::kDefaultEmptySlotSpanRingSize;
  static_assert(BucketIndexLookup::kNumBuckets <
                std::numeric_limits<uint16_t>::max());

  // Integrity check = ~reinterpret_cast<uintptr_t>(this).
  uintptr_t inverted_self_ = 0;

  // A lock which is hold during thread cache construction.
  // Any (de)allocation code path should not try to `Acquire()` this lock to
  // prevent deadlocks. Instead, `TryAcquire()`.
  internal::Lock thread_cache_construction_lock_;

  size_t scheduler_loop_quarantine_branch_capacity_in_bytes_ = 0;
  internal::SchedulerLoopQuarantineRoot scheduler_loop_quarantine_root_;
  internal::GlobalSchedulerLoopQuarantineBranch scheduler_loop_quarantine_;
  internal::GlobalSchedulerLoopQuarantineBranch
      scheduler_loop_quarantine_for_advanced_memory_safety_checks_;

  static constexpr internal::base::TimeDelta kMaxPurgeDuration =
      internal::base::Milliseconds(2);
  // Not overriding the global one to only change it for this partition.
  internal::base::TimeTicks (*now_maybe_overridden_for_testing_)() =
      internal::base::TimeTicks::Now;

  PartitionRoot();
  explicit PartitionRoot(PartitionOptions opts);

  // TODO(tasak): remove ~PartitionRoot() after confirming all tests
  // don't need ~PartitionRoot().
  ~PartitionRoot();

  // This will unreserve any space in the pool that the PartitionRoot is
  // using. This is needed because many tests create and destroy many
  // PartitionRoots over the lifetime of a process, which can exhaust the
  // pool and cause tests to fail.
  void DestructForTesting()
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  void DecommitEmptySlotSpansForTesting();

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
  void Init(PartitionOptions) PA_LOCKS_EXCLUDED(lock_);

  void EnableThreadCacheIfSupported()
      PA_LOCKS_EXCLUDED(thread_cache_construction_lock_, lock_);

  PA_ALWAYS_INLINE static PartitionRoot* FromSlotSpanMetadata(
      const SlotSpanMetadata* slot_span);

  // These two functions work unconditionally for normal buckets_.
  // For direct map, they only work for the first super page of a reservation,
  // (see partition_alloc_constants.h for the direct map allocation layout).
  // In particular, the functions always work for a pointer to the start of a
  // reservation.
  PA_ALWAYS_INLINE static PartitionRoot* FromFirstSuperPage(
      uintptr_t super_page);
  PA_ALWAYS_INLINE static PartitionRoot* FromAddrInFirstSuperpage(
      uintptr_t address);

  PA_ALWAYS_INLINE void DecreaseTotalSizeOfAllocatedBytes(uintptr_t slot_start,
                                                          size_t len);
  PA_ALWAYS_INLINE void IncreaseTotalSizeOfAllocatedBytes(uintptr_t addr,
                                                          size_t len,
                                                          size_t raw_size);
  PA_ALWAYS_INLINE void IncreaseCommittedPages(size_t len);
  PA_ALWAYS_INLINE void DecreaseCommittedPages(size_t len);
  PA_ALWAYS_INLINE void DecommitSystemPagesForData(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));
  PA_ALWAYS_INLINE void RecommitSystemPagesForData(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition,
      bool request_tagging)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  template <bool already_locked>
  PA_ALWAYS_INLINE bool TryRecommitSystemPagesForDataInternal(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition,
      bool request_tagging);

  // TryRecommitSystemPagesForDataWithAcquiringLock() locks this root internally
  // before invoking DecommitEmptySlotSpans(), which needs the lock. So the root
  // must not be locked when invoking this method.
  PA_ALWAYS_INLINE bool TryRecommitSystemPagesForDataWithAcquiringLock(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition,
      bool request_tagging)
      PA_LOCKS_EXCLUDED(internal::PartitionRootLock(this));

  // TryRecommitSystemPagesForDataLocked() doesn't lock this root internally
  // before invoking DecommitEmptySlotSpans(), which needs the lock. So the root
  // must have been already locked when invoking this method.
  PA_ALWAYS_INLINE bool TryRecommitSystemPagesForDataLocked(
      uintptr_t address,
      size_t length,
      PageAccessibilityDisposition accessibility_disposition,
      bool request_tagging)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  [[noreturn]] PA_NOINLINE void OutOfMemory(size_t size);

  // Returns a pointer aligned on |alignment|, or nullptr.
  //
  // |alignment| has to be a power of two and a multiple of sizeof(void*) (as in
  // posix_memalign() for POSIX systems). The returned pointer may include
  // padding, and can be passed to |Free()| later.
  //
  // NOTE: This is incompatible with anything that adds extras before the
  // returned pointer, such as in-slot metadata.
  template <AllocFlags flags = AllocFlags::kNone>
  PA_NOINLINE void* AlignedAlloc(size_t alignment, size_t requested_size);

  PA_ALWAYS_INLINE size_t GetAdjustedSizeForAlignment(size_t alignment,
                                                      size_t requested_size);
  template <AllocFlags flags = AllocFlags::kNone>
  PA_ALWAYS_INLINE void* AlignedAllocInline(size_t alignment,
                                            size_t requested_size);

  // PartitionAlloc supports multiple partitions, and hence multiple callers to
  // these functions. Setting PA_ALWAYS_INLINE bloats code, and can be
  // detrimental to performance, for instance if multiple callers are hot (by
  // increasing cache footprint). Set PA_NOINLINE on the "basic" top-level
  // functions to mitigate that for "vanilla" callers.
  //
  // |type_name == nullptr|: ONLY FOR TESTS except internal uses.
  // You should provide |type_name| to make debugging easier.
  template <AllocFlags flags = AllocFlags::kNone>
  PA_ALWAYS_INLINE PA_MALLOC_FN void* Alloc(size_t requested_size,
                                            const char* type_name = nullptr) {
    return AllocInline<flags>(requested_size, type_name);
  }
  // PartitionAlloc should provide only NOINLINE methods as public interfaces.
  template <AllocFlags flags = AllocFlags::kNone>
  PA_NOINLINE PA_MALLOC_FN void* AllocInline(size_t requested_size,
                                             const char* type_name = nullptr);

  // AllocInternal exposed for testing.
  template <AllocFlags flags = AllocFlags::kNone>
  PA_NOINLINE PA_MALLOC_FN void* AllocInternalForTesting(
      size_t requested_size,
      size_t slot_span_alignment,
      const char* type_name);  // IN-TEST

  template <AllocFlags alloc_flags = AllocFlags::kNone,
            FreeFlags free_flags = FreeFlags::kNone>
  PA_NOINLINE void* Realloc(void* ptr, size_t new_size, const char* type_name);
  template <AllocFlags alloc_flags = AllocFlags::kNone,
            FreeFlags free_flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE void* ReallocInline(void* ptr,
                                       size_t new_size,
                                       const char* type_name);

  // Because clients directly invoke FreeInline(), temporary keep Free()
  // PA_ALWAYS_INLINE. Instead, FreeInline() is PA_NOINLINE.
  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE void Free(void* object) {
    FreeInline<flags>(object);
  }

  // WithSize will be a FreeFlags::kSize
  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE void Free(void* object,
                             FreeHintType<FreeHintFlags(flags)> hint) {
    FreeInline<flags>(object, hint);
  }

  template <FreeFlags flags = FreeFlags::kNone>
  PA_NOINLINE void AlignedFree(void* object);

  // After making all callers depend on Free<flags>() instead of
  // FreeInline<flags>(), make FreeInline<flags> PA_ALWAYS_INLINE again. If
  // possible, make FreeInline private.
  template <FreeFlags flags = FreeFlags::kNone>
  PA_NOINLINE void FreeInline(void* object);
  template <FreeFlags flags = FreeFlags::kNone>
  PA_NOINLINE void FreeInline(void* object,
                              FreeHintType<FreeHintFlags(flags)> hint);
  // |object| must be a non-null pointer.
  PA_ALWAYS_INLINE std::pair<internal::SlotStart, internal::SlotSpanMetadata*>
  GetSlotStartAndSlotSpanFromAddress(void* object);

  template <FreeFlags flags = FreeFlags::kNone>
  PA_NOINLINE static void FreeInUnknownRoot(void* object);
  template <FreeFlags flags = FreeFlags::kNone>
  PA_NOINLINE static void FreeInUnknownRoot(
      void* object,
      FreeHintType<FreeHintFlags(flags)> hint);
  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE static void FreeInlineInUnknownRoot(void* object);
  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE static void FreeInlineInUnknownRoot(
      void* object,
      FreeHintType<FreeHintFlags(flags)> hint);
  // |object| must be a non-null pointer.
  PA_ALWAYS_INLINE static PartitionRoot* GetRootFromAddressInFirstSuperpage(
      void* object);

  PA_NOINLINE static PartitionRoot* GetRootFromAddress(void* object);

  template <FreeFlags flags>
  PA_ALWAYS_INLINE void FreeNoHooksImmediate(internal::SlotStart slot_start,
                                             SlotSpanMetadata* slot_span);
  template <FreeFlags flags>
  PA_ALWAYS_INLINE void FreeNoHooksImmediate(
      internal::SlotStart slot_start,
      SlotSpanMetadata* slot_span,
      FreeHintType<FreeHintFlags(flags)> hint);
  // Immediately frees the pointer bypassing the quarantine. `slot_start` is the
  // beginning of the slot that contains `object`.
  template <FreeFlags flags>
  PA_ALWAYS_INLINE void FreeNoHooksImmediateInternal(
      internal::SlotStart slot_start,
      SlotSpanMetadata* slot_span,
      const internal::BucketSizeDetails& size_details);

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  // Actual free operation on BRP dequarantine.
  PA_ALWAYS_INLINE static void FreeAfterBRPQuarantine(
      internal::UntaggedSlotStart slot_start,
      size_t slot_size);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  PA_ALWAYS_INLINE size_t GetSlotUsableSize(const SlotSpanMetadata* slot_span);

  // This function attempts to compute the slot_span's usable size without
  // touching `slot_span`, but if it fails it will fall back on
  // GetSlotUsableSize(slot_span).
  PA_ALWAYS_INLINE size_t
  GetSlotUsableSize(const internal::BucketSizeDetails& size_details,
                    SlotSpanMetadata* slot_span);

  PA_NOINLINE static size_t GetUsableSize(const void* ptr);

  PA_ALWAYS_INLINE PageAccessibilityConfiguration
  GetPageAccessibility(bool request_tagging) const;
  PA_ALWAYS_INLINE PageAccessibilityConfiguration
      PageAccessibilityWithThreadIsolationIfEnabled(
          PageAccessibilityConfiguration::Permissions) const;

  PA_ALWAYS_INLINE size_t
  AllocationCapacityFromSlotStart(internal::UntaggedSlotStart slot_start) const;
  PA_NOINLINE size_t AllocationCapacityFromRequestedSize(size_t size) const;

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_ALWAYS_INLINE static internal::InSlotMetadata*
  InSlotMetadataPointerFromSlotStartAndSize(
      internal::UntaggedSlotStart slot_start,
      size_t slot_size);
  PA_ALWAYS_INLINE internal::InSlotMetadata*
  InSlotMetadataPointerFromObjectForTesting(void* object) const;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  PA_ALWAYS_INLINE bool IsMemoryTaggingEnabled() const;
  PA_ALWAYS_INLINE bool UseRandomMemoryTagging() const;
  PA_ALWAYS_INLINE TagViolationReportingMode
  memory_tagging_reporting_mode() const;

  // Frees memory from this partition, if possible, by decommitting pages or
  // even entire slot spans. `flags` is an OR of base::PartitionPurgeFlags.
  // Caller is responsible to persist `purge_state` when calling this
  // periodically.
  // For single-time use, prefer one-param version.
  PA_NOINLINE void PurgeMemory(int flags, PurgeState& purge_state);
  PA_NOINLINE void PurgeMemory(int flags);

  // Reduces the size of the empty slot spans ring, until the dirty size is <=
  // |limit|.
  void ShrinkEmptySlotSpansRing(size_t limit)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  void DumpStats(const char* partition_name,
                 bool is_light_dump,
                 bool populate_discardable_bytes,
                 PartitionStatsDumper* partition_stats_dumper);

  static void DeleteForTesting(PartitionRoot* partition_root);
  void ResetForTesting(bool allow_leaks);
  void ResetBookkeepingForTesting();
  void SetGlobalEmptySlotSpanRingIndexForTesting(int16_t index);

  PA_ALWAYS_INLINE BucketDistribution GetBucketDistribution() const;

  static uint16_t SizeToBucketIndex(size_t size,
                                    BucketDistribution bucket_distribution);

  PA_ALWAYS_INLINE internal::BucketSizeDetails SlotSpanToBucketSizeDetails(
      SlotSpanMetadata* slot_span) const;

  PA_ALWAYS_INLINE internal::BucketSizeDetails SizeToBucketSizeDetails(
      size_t requested_size,
      SlotSpanMetadata* slot_span) const;

  PA_ALWAYS_INLINE void FreeInSlotSpan(internal::UntaggedSlotStart slot_start,
                                       SlotSpanMetadata* slot_span)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  // Frees memory, with |slot_start| as returned by |RawAlloc()|.
  PA_ALWAYS_INLINE void RawFree(internal::SlotStart slot_start,
                                SlotSpanMetadata* slot_span)
      PA_LOCKS_EXCLUDED(internal::PartitionRootLock(this));

  PA_ALWAYS_INLINE void RawFreeWithThreadCache(
      internal::SlotStart slot_start,
      const internal::BucketSizeDetails& size_details,
      SlotSpanMetadata* slot_span);

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  // Sets a new MTE tag on the slot. This must not be called when an object
  // enters BRP quarantine because it might cause a race with |raw_ptr|'s
  // ref-count decrement. (crbug.com/357526108)
  PA_ALWAYS_INLINE void RetagSlotIfNeeded(
      internal::UntaggedSlotStart slot_start_ptr,
      size_t slot_size);
#endif

  // This is safe to do because we are switching to a bucket distribution with
  // more buckets_, meaning any allocations we have done before the switch are
  // guaranteed to have a bucket under the new distribution when they are
  // eventually deallocated. We do not need synchronization here.
  PA_NOINLINE void SwitchToDenserBucketDistribution();
  // Switching back to the less dense bucket distribution is ok during tests.
  // At worst, we end up with deallocations that are sent to a bucket that we
  // cannot allocate from, which will not cause problems besides wasting
  // memory.
  PA_NOINLINE void ResetBucketDistributionForTesting();

  PA_NOINLINE internal::ThreadCache* thread_cache_for_testing() const;

  size_t get_total_size_of_committed_pages() const;
  size_t get_max_size_of_committed_pages() const;

  size_t get_total_size_of_allocated_bytes() const;

  size_t get_max_size_of_allocated_bytes() const;

  internal::pool_handle ChoosePool() const;
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_ALWAYS_INLINE const internal::PoolOffsetLookup& GetOffsetLookup() const;
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  PA_ALWAYS_INLINE const internal::ReservationOffsetTable&
  GetReservationOffsetTable() const;

  PA_ALWAYS_INLINE static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
  GetDirectMapMetadataAndGuardPagesSize();

  PA_ALWAYS_INLINE static PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
  GetDirectMapSlotSize(size_t raw_size);

  PA_ALWAYS_INLINE static size_t GetDirectMapReservationSize(
      size_t padded_raw_size);

  PA_ALWAYS_INLINE bool IsDirectMapped(
      partition_alloc::internal::SlotSpanMetadata* slot_span) const;

  PA_ALWAYS_INLINE size_t AdjustSize0IfNeeded(size_t size) const;

  // Adjusts the size by adding extras. Also include the 0->1 adjustment if
  // needed.
  PA_ALWAYS_INLINE size_t AdjustSizeForExtrasAdd(size_t size) const;

  // Adjusts the size by subtracing extras. Doesn't include the 0->1 adjustment,
  // which leads to an asymmetry with AdjustSizeForExtrasAdd, but callers of
  // AdjustSizeForExtrasSubtract either expect the adjustment to be included, or
  // are indifferent.
  PA_ALWAYS_INLINE size_t AdjustSizeForExtrasSubtract(size_t size) const;

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_ALWAYS_INLINE bool brp_enabled() const;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  // When a SlotSpan becomes empty, the allocator tries to avoid reusing it
  // immediately, to help with fragmentation. At this point, it becomes dirty
  // committed memory, which we want to minimize. This could be decommitted
  // immediately, but that would imply doing a lot of system calls. In
  // particular, for single-slot SlotSpans, a malloc() / free() loop would cause
  // a *lot* of system calls.
  //
  // As an intermediate step, empty SlotSpans are placed into a per-partition
  // global ring buffer, giving the newly-empty SlotSpan a chance to be reused
  // before getting decommitted. A new entry (i.e. a newly empty SlotSpan)
  // taking the place used by a previous one will lead the previous SlotSpan to
  // be decommitted immediately, provided that it is still empty.
  //
  // Increasing the ring size means giving more time for reuse to happen, at the
  // cost of possibly increasing peak committed memory usage (and increasing the
  // size of PartitionRoot a bit, since the ring buffer is there). Note that the
  // ring buffer doesn't necessarily contain an empty SlotSpan, as SlotSpans are
  // *not* removed from it when reused. So the ring buffer really is a buffer
  // of *possibly* empty SlotSpans.
  //
  // In all cases, PartitionRoot::PurgeMemory() with the
  // PurgeFlags::kDecommitEmptySlotSpans flag will eagerly decommit all entries
  // in the ring buffer, so with periodic purge enabled, this typically happens
  // every few seconds.
  PA_NOINLINE void AdjustSlotSpanRing(int16_t ring_size, int dirty_bytes_shift);

  // To make tests deterministic, it is necessary to uncap the amount of memory
  // waste incurred by empty slot spans. Otherwise, the size of various
  // freelists, and committed memory becomes harder to reason about (and
  // brittle) with a single thread, and non-deterministic with several.
  void UncapEmptySlotSpanMemoryForTesting() {
    max_empty_slot_spans_dirty_bytes_shift_ = 0;
  }

  // Enables/disables the free list straightening for larger slot spans in
  // PurgeMemory().
  static void SetStraightenLargerSlotSpanFreeListsMode(
      StraightenLargerSlotSpanFreeListsMode new_value);
  // Enables/disables the free list sorting for smaller slot spans in
  // PurgeMemory().
  static void SetSortSmallerSlotSpanFreeListsEnabled(bool new_value);
  // Enables/disables the sorting of active slot spans in PurgeMemory().
  static void SetSortActiveSlotSpansEnabled(bool new_value);

  PA_ALWAYS_INLINE static StraightenLargerSlotSpanFreeListsMode
  GetStraightenLargerSlotSpanFreeListsMode();

  PA_NOINLINE void ReconfigureSchedulerLoopQuarantineForCurrentThread(
      const internal::SchedulerLoopQuarantineConfig& config);

#if PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)
  PA_ALWAYS_INLINE std::ptrdiff_t MetadataOffset() const;
#else
  PA_ALWAYS_INLINE
  PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR
  size_t MetadataOffset() const;
#endif  // PA_CONFIG(MOVE_METADATA_OUT_OF_GIGACAGE)

  PA_NOINLINE static void CheckMetadataIntegrity(const void* object);

 private:
  static inline StraightenLargerSlotSpanFreeListsMode
      straighten_larger_slot_span_free_lists_ =
          StraightenLargerSlotSpanFreeListsMode::kOnlyWhenUnprovisioning;
  static inline bool sort_smaller_slot_span_free_lists_ = true;
  static inline bool sort_active_slot_spans_ = false;

  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE void FreeInlineInternal(void* object);
  template <FreeFlags flags = FreeFlags::kNone>
  PA_ALWAYS_INLINE void FreeInlineInternal(
      void* object,
      FreeHintType<FreeHintFlags(flags)> hint);

  // Common path of Free() and FreeInUnknownRoot(). Returns
  // true if the caller should return immediately.
  template <FreeFlags flags>
  PA_ALWAYS_INLINE static bool FreeProlog(void* object,
                                          const PartitionRoot* root);

  // |buckets_| has `BucketIndexLookup::kNumBuckets` elements, but we
  // sometimes access it at index `BucketIndexLookup::kNumBuckets`, which is
  // occupied by the sentinel bucket. The correct layout is enforced by a
  // static_assert() in partition_root.cc, so this is fine. However, UBSAN is
  // correctly pointing out that there is an out-of-bounds access, so disable it
  // for these accesses.
  //
  // See crbug.com/1150772 for an instance of Clusterfuzz / UBSAN detecting
  // this.
  PA_NO_SANITIZE("undefined")
  PA_ALWAYS_INLINE const Bucket& bucket_at(size_t i) const;

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
  PA_ALWAYS_INLINE bool IsDirectMappedBucket(const Bucket* bucket) const;

  // Same as |Alloc()|, but allows specifying |slot_span_alignment|. It
  // has to be a multiple of partition page size, greater than 0 and no greater
  // than kMaxSupportedAlignment. If it equals exactly 1 partition page, no
  // special action is taken as PartitionAlloc naturally guarantees this
  // alignment, otherwise a sub-optimal allocation strategy is used to
  // guarantee the higher-order alignment.
  template <AllocFlags flags>
  PA_ALWAYS_INLINE PA_MALLOC_FN void* AllocInternal(size_t requested_size,
                                                    size_t slot_span_alignment,
                                                    const char* type_name);

  // Same as |AllocInternal()|, but don't handle allocation hooks.
  template <AllocFlags flags = AllocFlags::kNone>
  PA_ALWAYS_INLINE PA_MALLOC_FN void* AllocInternalNoHooks(
      size_t requested_size,
      size_t slot_span_alignment);
  // Allocates a memory slot, without initializing extras.
  //
  // - |flags| are as in Alloc().
  // - |raw_size| accommodates for extras on top of Alloc()'s
  //   |requested_size|.
  // - |usable_size|, |slot_size| and |is_already_zeroed| are output only.
  //   Note, |usable_size| is guaranteed to be no smaller than Alloc()'s
  //   |requested_size|, and no larger than |slot_size|.
  template <AllocFlags flags>
  PA_ALWAYS_INLINE internal::UntaggedSlotStart RawAlloc(
      Bucket* bucket,
      size_t raw_size,
      size_t slot_span_alignment,
      size_t* usable_size,
      size_t* slot_size,
      bool* is_already_zeroed);
  template <AllocFlags flags>
  PA_ALWAYS_INLINE internal::UntaggedSlotStart AllocFromBucket(
      Bucket* bucket,
      size_t raw_size,
      size_t slot_span_alignment,
      size_t* usable_size,
      size_t* slot_size,
      bool* is_already_zeroed)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));

  // We use this to make MEMORY_TOOL_REPLACES_ALLOCATOR behave the same for max
  // size as other alloc code.
  template <AllocFlags flags>
  PA_ALWAYS_INLINE static bool AllocWithMemoryToolProlog(size_t size);

  bool TryReallocInPlaceForNormalBuckets(void* object,
                                         SlotSpanMetadata* slot_span,
                                         size_t new_size)
      PA_LOCKS_EXCLUDED(thread_cache_construction_lock_);
  bool TryReallocInPlaceForDirectMap(SlotSpanMetadata* slot_span,
                                     size_t requested_size)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));
  void DecommitEmptySlotSpans()
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));
  PA_ALWAYS_INLINE void RawFreeLocked(internal::UntaggedSlotStart slot_start,
                                      SlotSpanMetadata* slot_span)
      PA_EXCLUSIVE_LOCKS_REQUIRED(internal::PartitionRootLock(this));
  internal::ThreadCache* MaybeInitThreadCache()
      PA_LOCKS_EXCLUDED(thread_cache_construction_lock_);
  internal::ThreadCache* ForceInitThreadCache()
      PA_LOCKS_EXCLUDED(thread_cache_construction_lock_);

  // May return an invalid thread cache.
  PA_ALWAYS_INLINE internal::ThreadCache* GetOrCreateThreadCache();
  PA_ALWAYS_INLINE internal::ThreadCache* GetThreadCache();
  // Similar to `GetOrCreateThreadCache()`, but this creates a new thread cache
  // with `ForceInitThreadCache()`. This can be slow since it acquires a lock,
  // and hence with a risk of deadlock.
  // Must NOT be used inside (de)allocation code path.
  PA_ALWAYS_INLINE internal::ThreadCache* EnsureThreadCache();

  PA_ALWAYS_INLINE internal::SchedulerLoopQuarantineRoot&
  GetSchedulerLoopQuarantineRoot();

  PA_ALWAYS_INLINE AllocationNotificationData
  CreateAllocationNotificationData(void* object,
                                   size_t size,
                                   const char* type_name) const;
  PA_ALWAYS_INLINE static FreeNotificationData
  CreateDefaultFreeNotificationData(void* address);
  PA_ALWAYS_INLINE FreeNotificationData
  CreateFreeNotificationData(void* address) const;

#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  PA_NOINLINE void QuarantineForBrp(const SlotSpanMetadata* slot_span,
                                    internal::SlotStart slot_start);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

#if PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)
  static internal::Lock& GetEnumeratorLock();

  PartitionRoot* PA_GUARDED_BY(GetEnumeratorLock()) next_root = nullptr;
  PartitionRoot* PA_GUARDED_BY(GetEnumeratorLock()) prev_root = nullptr;

  friend class internal::PartitionRootEnumerator;
#endif  // PA_CONFIG(USE_PARTITION_ROOT_ENUMERATOR)

  std::atomic<uint64_t> intended_leak_size_;

  friend class internal::ThreadCache;
  friend class internal::BatchFreeQueue;
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  friend class internal::InSlotMetadata;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  template <bool>
  friend class internal::SchedulerLoopQuarantineBranch;
};

PA_ALWAYS_INLINE bool PartitionRoot::IsMemoryTaggingEnabled() const {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  return settings_.memory_tagging_enabled_;
#else
  return false;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}
PA_ALWAYS_INLINE bool PartitionRoot::UseRandomMemoryTagging() const {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  return settings_.use_random_memory_tagging_;
#else
  return false;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}

PA_ALWAYS_INLINE TagViolationReportingMode
PartitionRoot::memory_tagging_reporting_mode() const {
#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
  return settings_.memory_tagging_reporting_mode_;
#else
  return TagViolationReportingMode::kUndefined;
#endif  // PA_BUILDFLAG(HAS_MEMORY_TAGGING)
}

#include "partition_alloc/internal/partition_root_exports.h"

#if PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionAllocMallocHookOnBeforeForkInParent();
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionAllocMallocHookOnAfterForkInParent();
PA_COMPONENT_EXPORT(PARTITION_ALLOC)
void PartitionAllocMallocHookOnAfterForkInChild();
#endif  // PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_PARTITION_ROOT_H_
