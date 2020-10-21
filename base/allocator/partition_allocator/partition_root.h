// Copyright (c) 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_

#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/allocator/partition_allocator/partition_alloc_features.h"
#include "base/allocator/partition_allocator/partition_alloc_forward.h"
#include "base/allocator/partition_allocator/partition_direct_map_extent.h"
#include "base/allocator/partition_allocator/partition_lock.h"
#include "base/allocator/partition_allocator/partition_tag.h"
#include "base/allocator/partition_allocator/pcscan.h"
#include "base/allocator/partition_allocator/thread_cache.h"
#include "base/optional.h"

namespace base {

class PartitionStatsDumper;

// Options struct used to configure PartitionRoot and PartitionAllocator.
struct PartitionOptions {
  enum class Alignment {
    // By default all allocations will be aligned to 8B (16B if
    // BUILDFLAG_INTERNAL_USE_PARTITION_ALLOC_AS_MALLOC is true).
    kRegular,

    // In addition to the above alignment enforcement, this option allows using
    // AlignedAlloc() which can align at a larger boundary.  This option comes
    // at a cost of disallowing cookies on Debug builds and tags/ref-counts for
    // CheckedPtr. It also causes all allocations to go outside of GigaCage, so
    // that CheckedPtr can easily tell if a pointer comes with a tag/ref-count
    // or not.
    kAlignedAlloc,
  };

  enum class ThreadCache {
    kDisabled,
    kEnabled,
  };

  enum class PCScan {
    // Should be used for value partitions, i.e. partitions that are known to
    // not have pointers. No metadata (quarantine bitmaps) is allocated for such
    // partitions.
    kAlwaysDisabled,
    // PCScan is disabled by default, but can be enabled by calling
    // PartitionRoot::EnablePCScan().
    kDisabledByDefault,
    // PCScan is always enabled.
    kEnabled,
  };

  Alignment alignment = Alignment::kRegular;
  ThreadCache thread_cache = ThreadCache::kDisabled;
  PCScan pcscan = PCScan::kAlwaysDisabled;
};

// Never instantiate a PartitionRoot directly, instead use
// PartitionAllocator.
template <bool thread_safe>
struct BASE_EXPORT PartitionRoot {
  using SlotSpan = internal::SlotSpanMetadata<thread_safe>;
  using Bucket = internal::PartitionBucket<thread_safe>;
  using SuperPageExtentEntry =
      internal::PartitionSuperPageExtentEntry<thread_safe>;
  using DirectMapExtent = internal::PartitionDirectMapExtent<thread_safe>;
  using ScopedGuard = internal::ScopedGuard<thread_safe>;
  using PCScan = base::Optional<internal::PCScan<thread_safe>>;

  internal::MaybeSpinLock<thread_safe> lock_;

  // Flags accessed on fast paths.
  bool with_thread_cache = false;
  const bool is_thread_safe = thread_safe;
  // TODO(bartekn): Consider size of added extras (cookies and/or tag, or
  // nothing) instead of true|false, so that we can just add or subtract the
  // size instead of having an if branch on the hot paths.
  bool allow_extras;
  bool scannable = false;
  bool initialized = false;

#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_MTE_CHECKED_PTR
  internal::PartitionTag current_partition_tag = 0;
#endif
#if ENABLE_TAG_FOR_MTE_CHECKED_PTR
  char* next_tag_bitmap_page = nullptr;
#endif

  // Bookkeeping.
  // Invariant: total_size_of_committed_pages <=
  //                total_size_of_super_pages +
  //                total_size_of_direct_mapped_pages.
  size_t total_size_of_committed_pages GUARDED_BY(lock_) = 0;
  size_t total_size_of_super_pages GUARDED_BY(lock_) = 0;
  size_t total_size_of_direct_mapped_pages GUARDED_BY(lock_) = 0;

  char* next_super_page = nullptr;
  char* next_partition_page = nullptr;
  char* next_partition_page_end = nullptr;
  SuperPageExtentEntry* current_extent = nullptr;
  SuperPageExtentEntry* first_extent = nullptr;
  DirectMapExtent* direct_map_list = nullptr;
  SlotSpan* global_empty_slot_span_ring[kMaxFreeableSpans] = {};
  int16_t global_empty_slot_span_ring_index = 0;

  // Integrity check = ~reinterpret_cast<uintptr_t>(this).
  uintptr_t inverted_self = 0;
  PCScan pcscan;

  // The bucket lookup table lets us map a size_t to a bucket quickly.
  // The trailing +1 caters for the overflow case for very large allocation
  // sizes.  It is one flat array instead of a 2D array because in the 2D
  // world, we'd need to index array[blah][max+1] which risks undefined
  // behavior.
  static uint16_t
      bucket_index_lookup[((kBitsPerSizeT + 1) * kNumBucketsPerOrder) + 1];
  // Accessed on fast paths, but sizeof(Bucket) is large, so there is no real
  // benefit in packing it with other members.
  Bucket buckets[kNumBuckets] = {};
  Bucket sentinel_bucket;

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

  ALWAYS_INLINE static bool IsValidSlotSpan(SlotSpan* slot_span);
  ALWAYS_INLINE static PartitionRoot* FromSlotSpan(SlotSpan* slot_span);

  ALWAYS_INLINE void IncreaseCommittedPages(size_t len)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void DecreaseCommittedPages(size_t len)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void DecommitSystemPages(void* address, size_t length)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);
  ALWAYS_INLINE void RecommitSystemPages(void* address, size_t length)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  NOINLINE void OutOfMemory(size_t size);

  // Returns a pointer aligned on |alignment|, or nullptr.
  //
  // |alignment| has to be a power of two and a multiple of sizeof(void*) (as in
  // posix_memalign() for POSIX systems). The returned pointer may include
  // padding, and can be passed to |Free()| later.
  //
  // NOTE: Doesn't work when DCHECK_IS_ON(), as it is incompatible with cookies.
  ALWAYS_INLINE void* AlignedAllocFlags(int flags,
                                        size_t alignment,
                                        size_t size);

  ALWAYS_INLINE void* Alloc(size_t requested_size, const char* type_name);
  ALWAYS_INLINE void* AllocFlags(int flags,
                                 size_t requested_size,
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
  ALWAYS_INLINE void* AllocFlagsNoHooks(int flags, size_t requested_size);

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
  ALWAYS_INLINE size_t GetSize(void* ptr) const;
  ALWAYS_INLINE size_t ActualSize(size_t size);

  // Frees memory from this partition, if possible, by decommitting pages or
  // even etnire slot spans. |flags| is an OR of base::PartitionPurgeFlags.
  void PurgeMemory(int flags);

  void DumpStats(const char* partition_name,
                 bool is_light_dump,
                 PartitionStatsDumper* partition_stats_dumper);

  static uint16_t SizeToBucketIndex(size_t size);

  // Frees memory, with |ptr| as returned by |RawAlloc()|.
  ALWAYS_INLINE void RawFree(void* ptr, SlotSpan* slot_span);
  static void RawFreeStatic(void* ptr);

  internal::ThreadCache* thread_cache_for_testing() const {
    return with_thread_cache ? internal::ThreadCache::Get() : nullptr;
  }
  size_t total_size_of_committed_pages_for_testing() {
    ScopedGuard guard{lock_};
    return total_size_of_committed_pages;
  }

  ALWAYS_INLINE internal::PartitionTag GetNewPartitionTag() {
#if ENABLE_TAG_FOR_CHECKED_PTR2 || ENABLE_TAG_FOR_MTE_CHECKED_PTR
    auto tag = ++current_partition_tag;
    tag += !tag;  // Avoid 0.
    current_partition_tag = tag;
    return tag;
#else
    return 0;
#endif
  }

  bool UsesGigaCage() const {
    return features::IsPartitionAllocGigaCageEnabled() && allow_extras;
  }

  void EnablePCScan() {
    // TODO(bikineev): Make CHECK once PCScan is enabled.
    if (!scannable || pcscan.has_value())
      return;
    pcscan.emplace(this);
  }

 private:
  // Allocates memory, without initializing extras.
  //
  // - |flags| are as in AllocFlags().
  // - |raw_size| should accommodate extras on top of AllocFlags()'s
  //   |requested_size|.
  // - |utilized_slot_size| and |is_already_zeroed| are output only.
  //   |utilized_slot_size| is guaranteed to be larger or equal to
  //   |raw_size|.
  ALWAYS_INLINE void* RawAlloc(Bucket* bucket,
                               int flags,
                               size_t raw_size,
                               size_t* utilized_slot_size,
                               bool* is_already_zeroed);
  ALWAYS_INLINE void* AllocFromBucket(Bucket* bucket,
                                      int flags,
                                      size_t raw_size,
                                      size_t* utilized_slot_size,
                                      bool* is_already_zeroed)
      EXCLUSIVE_LOCKS_REQUIRED(lock_);

  bool ReallocDirectMappedInPlace(
      internal::SlotSpanMetadata<thread_safe>* slot_span,
      size_t requested_size) EXCLUSIVE_LOCKS_REQUIRED(lock_);
  void DecommitEmptySlotSpans() EXCLUSIVE_LOCKS_REQUIRED(lock_);

  friend class internal::ThreadCache;
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_ROOT_H_
