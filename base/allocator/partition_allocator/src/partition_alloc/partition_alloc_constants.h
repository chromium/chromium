// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_CONSTANTS_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_CONSTANTS_H_

#include <algorithm>
#include <climits>
#include <cstddef>
#include <limits>

#include "partition_alloc/bucket_lookup.h"
#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/flags.h"
#include "partition_alloc/page_allocator_constants.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_public_constants.h"

#if PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
#include <mach/vm_page_size.h>
#endif

#if PA_BUILDFLAG(HAS_MEMORY_TAGGING)
#include "partition_alloc/tagging.h"
#endif

namespace partition_alloc {

namespace internal {
// Bit flag constants used as `flag` argument of PartitionRoot::Alloc<flags>,
// AlignedAlloc, etc.
enum class AllocFlags {
  kNone = 0,
  kReturnNull = 1 << 0,
  kZeroFill = 1 << 1,
  // Don't allow allocation override hooks. Override hooks are expected to
  // check for the presence of this flag and return false if it is active.
  kNoOverrideHooks = 1 << 2,
  // Never let a memory tool like ASan (if active) perform the allocation.
  kNoMemoryToolOverride = 1 << 3,
  // Don't allow any hooks (override or observers).
  kNoHooks = 1 << 4,  // Internal.
  // If the allocation requires a "slow path" (such as allocating/committing a
  // new slot span), return nullptr instead. Note this makes all large
  // allocations return nullptr, such as direct-mapped ones, and even for
  // smaller ones, a nullptr value is common.
  kFastPathOrReturnNull = 1 << 5,  // Internal.
  // An allocation override hook should tag the allocated memory for MTE.
  kMemoryShouldBeTaggedForMte = 1 << 6,  // Internal.
  // An explicitly aligned allocation.
  kAlignedAlloc = 1 << 7,  // Internal.
  // Allow allocations up to 16GiB. This is intended for ArrayBuffers, and
  // should not be used for other allocations.
  kAllowGigaAllocations = 1 << 8,
  kMaxValue = kAllowGigaAllocations,
};
PA_DEFINE_OPERATORS_FOR_FLAGS(AllocFlags);

// Bit flag constants used as `flag` argument of PartitionRoot::Free<flags>.
enum class FreeFlags {
  kNone = 0,
  // See AllocFlags::kNoMemoryToolOverride.
  kNoMemoryToolOverride = 1 << 0,
  // Don't allow any hooks (override or observers).
  kNoHooks = 1 << 1,  // Internal.
  // Quarantine for a while to ensure no UaF from on-stack pointers.
  kSchedulerLoopQuarantine = 1 << 2,
  // Quarantine for a while to ensure no UaF from on-stack pointers.
  kSchedulerLoopQuarantineForAdvancedMemorySafetyChecks = 1 << 3,
  // `kWith[A-Za-z]+Hint` shows whether `FreeHint`'s member is available or not.
  kWithSizeHint = 1 << 4,       // `FreeHint::size` is available.
  kWithAlignmentHint = 1 << 5,  // `FreeHint::alignment` is available.
  kWithTypeIdHint = 1 << 6,     // `FreeHint::type_id` is available.
  // Only used when MEMORY_TOOL_REPLACES_ALLOCATOR is defined, we will attempt
  // to use an aligned free function.
  kAlignedFreeForMemoryTool = 1 << 7,  // Internal.
  kIntendedLeak = 1 << 8,              // Internal.
  kMaxValue = kIntendedLeak,
};
PA_DEFINE_OPERATORS_FOR_FLAGS(FreeFlags);
}  // namespace internal

using internal::AllocFlags;
using internal::FreeFlags;

namespace internal {

// Underlying partition storage pages (`PartitionPage`s) are a power-of-2 size.
// It is typical for a `PartitionPage` to be based on multiple system pages.
// Most references to "page" refer to `PartitionPage`s.
//
// *Super pages* are the underlying system allocations we make. Super pages
// contain multiple partition pages and include space for a small amount of
// metadata per partition page.
//
// Inside super pages, we store *slot spans*. A slot span is a continguous range
// of one or more `PartitionPage`s that stores allocations of the same size.
// Slot span sizes are adjusted depending on the allocation size, to make sure
// the packing does not lead to unused (wasted) space at the end of the last
// system page of the span. For our current maximum slot span size of 64 KiB and
// other constant values, we pack _all_ `PartitionRoot::Alloc` sizes perfectly
// up against the end of a system page.

#if (PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)) || \
    defined(PARTITION_ALLOCATOR_CONSTANTS_POSIX_NONCONST_PAGE_SIZE)
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return PageAllocationGranularityShift() + 2;
}
#elif defined(_MIPS_ARCH_LOONGSON)
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 16;  // 64 KiB
}
#elif PA_BUILDFLAG(PA_ARCH_CPU_PPC64)
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 18;  // 256 KiB
}
#else
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageShift() {
  return 14;  // 16 KiB
}
#endif
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageSize() {
  return 1 << PartitionPageShift();
}
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageOffsetMask() {
  return PartitionPageSize() - 1;
}
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
PartitionPageBaseMask() {
  return ~PartitionPageOffsetMask();
}

// Number of system pages per regular slot span. Above this limit, we call it
// a single-slot span, as the span literally hosts only one slot, and has
// somewhat different implementation. At run-time, single-slot spans can be
// differentiated with a call to CanStoreRawSize().
// TODO: Should this be 1 on platforms with page size larger than 4kB, e.g.
// ARM macOS or defined(_MIPS_ARCH_LOONGSON)?
constexpr size_t kMaxPartitionPagesPerRegularSlotSpan = 4;

// To avoid fragmentation via never-used freelist entries, we hand out partition
// freelist sections gradually, in units of the dominant system page size. What
// we're actually doing is avoiding filling the full `PartitionPage` (16 KiB)
// with freelist pointers right away. Writing freelist pointers will fault and
// dirty a private page, which is very wasteful if we never actually store
// objects there.

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
NumSystemPagesPerPartitionPage() {
  return PartitionPageSize() >> SystemPageShift();
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MaxSystemPagesPerRegularSlotSpan() {
  return NumSystemPagesPerPartitionPage() *
         kMaxPartitionPagesPerRegularSlotSpan;
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
MaxRegularSlotSpanSize() {
  return kMaxPartitionPagesPerRegularSlotSpan << PartitionPageShift();
}

// PartitionAlloc's address space is split into pools. See `glossary.md`.

// kNullPoolHandle doesn't have metadata, hence - 1
constexpr size_t kNumPools = kMaxPoolHandle - 1;

constexpr size_t kMaxSuperPagesInPool = kPoolMaxSize / kSuperPageSize;

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
static_assert(kThreadIsolatedPoolHandle == kNumPools,
              "The thread isolated pool must come last since we write-protect "
              "its metadata.");
#endif

// Slots larger than this size will not receive MTE protection. Pages intended
// for allocations larger than this constant should not be backed with PROT_MTE
// (which saves shadow tag memory). We also save CPU cycles by skipping tagging
// of large areas which are less likely to benefit from MTE protection.
constexpr size_t kMaxMemoryTaggingSize = 1024;

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
NumPartitionPagesPerSuperPage() {
  return kSuperPageSize >> PartitionPageShift();
}

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
// In 64-bit mode, the direct map allocation granularity is super page size,
// because this is the reservation granularity of the pools.
PA_ALWAYS_INLINE constexpr size_t DirectMapAllocationGranularity() {
  return kSuperPageSize;
}

PA_ALWAYS_INLINE constexpr size_t DirectMapAllocationGranularityShift() {
  return kSuperPageShift;
}
#else   // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
// In 32-bit mode, address space is space is a scarce resource. Use the system
// allocation granularity, which is the lowest possible address space allocation
// unit. However, don't go below partition page size, so that pool bitmaps
// don't get too large. See kBytesPer1BitOfBRPPoolBitmap.
PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularity() {
  return std::max(PageAllocationGranularity(), PartitionPageSize());
}

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularityShift() {
  return std::max(PageAllocationGranularityShift(), PartitionPageShift());
}
#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)

PA_ALWAYS_INLINE PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR size_t
DirectMapAllocationGranularityOffsetMask() {
  return DirectMapAllocationGranularity() - 1;
}

// Limit when downsizing a direct mapping using `realloc`:
constexpr size_t kMinDirectMappedDownsize =
    BucketIndexLookup::kMaxBucketSize + 1;

// Max alignment supported by AlignedAlloc().
// kSuperPageSize alignment can't be easily supported, because each super page
// starts with guard pages & metadata.
// TODO(casey.smalley@arm.com): under 64k pages we can end up in a situation
// where a normal slot span will be large enough to contain multiple items,
// but the address will go over the final partition page after being aligned.
#if PA_BUILDFLAG(IS_LINUX) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
inline constexpr size_t kMaxSupportedAlignment = kSuperPageSize / 4;
#else
inline constexpr size_t kMaxSupportedAlignment = kSuperPageSize / 2;
#endif

enum SlotSpanRingMaxSize : int16_t {
  kSmall = 1 << 4,
  kMedium = 1 << 7,
  kLarge = 1 << 10,
};

// The constants below define the default empty ring size:
// - In foreground mode (see `PartitionRoot::AdjustForForeground`).
inline constexpr size_t kDefaultEmptySlotSpanRingSize =
    SlotSpanRingMaxSize::kSmall;

// This is the maximum ring size supported across all modes:
inline constexpr size_t kMaxEmptySlotSpanRingSize =
#if PA_BUILDFLAG(USE_LARGE_EMPTY_SLOT_SPAN_RING)
    SlotSpanRingMaxSize::kLarge;
#else
    SlotSpanRingMaxSize::kMedium;
#endif
static_assert(kMaxEmptySlotSpanRingSize >= kDefaultEmptySlotSpanRingSize);

// If the total size in bytes of allocated but not committed pages exceeds this
// value (probably it is a "out of virtual address space" crash), a special
// crash stack trace is generated at
// `PartitionOutOfMemoryWithLotsOfUncommitedPages`. This is to distinguish "out
// of virtual address space" from "out of physical memory" in crash reports.
inline constexpr size_t kReasonableSizeOfUnusedPages =
    1024 * 1024 * 1024;  // 1 GiB

// These byte values match tcmalloc.
inline constexpr unsigned char kUninitializedByte = 0xAB;
inline constexpr unsigned char kFreedByte = 0xCD;

inline constexpr unsigned char kQuarantinedByte = 0xEF;

// Each IntendedLeaked memory region: [0...slot_size) will be filled by:
// [0     ... 8):         |EB B0 00 "typeid (32bit)" "unused(8bit)"|
//   ...
// [8(n-1)... 8n):        |EB B0 00 "typeid (32bit)" "unused(8bit)"|
// [8n    ... slot_size): |EB EB ... EB| (remainder)
// (*) n = slot_size / sizeof(uint64_t)
inline constexpr uint64_t kIntendedLeakQuarantineMarker = 0xEBB0000000000000u;
inline constexpr uint64_t kIntendedLeakQuarantineMask = 0xFFFFFF0000000000u;
inline constexpr uint8_t kIntendedLeakQuarantineRemainder = 0xEB;
// Explicitly reserved sentinel type ID indicating an intended-leak retirement
// without a specific type ID hint (e.g. untyped RTH retirements). Real type ID
// tokens must not use 0.
inline constexpr uint32_t kIntendedLeakUnknownTypeId = 0;

}  // namespace internal

// Same as `MaxAllocationSize()` in partition_alloc_public_constants.h, but for
// allocations with `kAllowGigaAllocations`, which can be larger.
PA_ALWAYS_INLINE constexpr size_t MaxGigaAllocationSize() {
#if PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
  return (16ULL * internal::kGiB) - internal::kSuperPageSize;
#else
  return MaxAllocationSize();
#endif
}

// Same as `MaxAllocationSize()` in partition_alloc_public_constants.h, but for
// ArrayBuffers, which can be larger.
PA_ALWAYS_INLINE constexpr size_t MaxDirectMappedArrayBuffer() {
  return MaxGigaAllocationSize();
}

// When trying to conserve memory, set the thread cache limit to this.
static inline constexpr size_t kThreadCacheDefaultSizeThreshold = 512;

// 32kiB is chosen here as from local experiments, "zone" allocation in
// V8 is performance-sensitive, and zones can (and do) grow up to 32kiB for
// each individual allocation.
static inline constexpr size_t kThreadCacheLargeSizeThreshold = 1 << 15;
static_assert(kThreadCacheLargeSizeThreshold <=
                  std::numeric_limits<uint16_t>::max(),
              "");

// These constants are used outside PartitionAlloc itself, so we provide
// non-internal aliases here.
using ::partition_alloc::internal::PartitionPageSize;

#if PA_BUILDFLAG(ENABLE_AUTO_PARTITIONING)
inline constexpr size_t kNumPartitions = 2;
#else
inline constexpr size_t kNumPartitions = 1;
#endif

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_CONSTANTS_H_
