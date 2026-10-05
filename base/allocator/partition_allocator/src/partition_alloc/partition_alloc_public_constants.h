// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef PARTITION_ALLOC_PARTITION_ALLOC_PUBLIC_CONSTANTS_H_
#define PARTITION_ALLOC_PARTITION_ALLOC_PUBLIC_CONSTANTS_H_

#include <cstddef>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/flags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"

namespace partition_alloc {

namespace internal {

// Size of a cache line. Not all CPUs in the world have a 64 bytes cache line
// size, but as of 2026, most do. This is in particular the case for almost all
// x86_64. Arm64 chips used by Mac and iOS (all M Series and modern A Series)
// have a 128 byte CacheLine (see section 5.6.5 Memory Cache of Apple Silicon
// CPU Optimization Guide Version 4).
//
// As this is used for static alignment, we cannot query the CPU at runtime to
// determine the actual alignment, so use 64 or 128 bytes everywhere. Since this
// is only used to avoid false sharing, getting this wrong only results in lower
// performance, not incorrect code.
#if PA_BUILDFLAG(IS_APPLE) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64)
inline constexpr size_t kPartitionCachelineSize = 128;
#else
inline constexpr size_t kPartitionCachelineSize = 64;
#endif

// We reserve virtual address space in 2 MiB chunks (aligned to 2 MiB as well).
// These chunks are called *super pages*. We do this so that we can store
// metadata in the first few pages of each 2 MiB-aligned section. This makes
// freeing memory very fast. 2 MiB size & alignment were chosen, because this
// virtual address block represents a full but single page table allocation on
// ARM, ia32 and x64, which may be slightly more performance&memory efficient.
// (Note, these super pages are backed by 4 KiB system pages and have nothing to
// do with OS concept of "huge pages"/"large pages", even though the size
// coincides.)
//
// The layout of the super page is as follows. The sizes below are the same for
// 32- and 64-bit platforms.
//
//     +-----------------------+
//     | Guard page (4 KiB)    |
//     | Metadata page (4 KiB) |
//     | Guard pages (8 KiB)   |
//     | Slot span             |
//     | Slot span             |
//     | ...                   |
//     | Slot span             |
//     | Guard pages (16 KiB)  |
//     +-----------------------+
//
// If ENABLE_BACKUP_REF_PTR_SUPPORT is on, InSlotMetadataTable(4KiB) is inserted
// after the Metadata page, which hosts what normally would be in-slot metadata,
// but for reasons described in InSlotMetadata::From() can't always be placed
// inside the slot. BRP ref-count is there, hence the connection with
// ENABLE_BACKUP_REF_PTR_SUPPORT.
// The guard page after the table is reduced to 4KiB.
//
//...
//     | Metadata page (4 KiB)       |
//     | InSlotMetadataTable (4 KiB) |
//     | Guard pages (4 KiB)         |
//...
//
// Each slot span is a contiguous range of one or more `PartitionPage`s. Note
// that slot spans of different sizes may co-exist with one super page. Even
// slot spans of the same size may support different slot sizes. However, all
// slots within a span have to be of the same size.
//
// The metadata page has the following format. Note that the `PartitionPage`
// that is not at the head of a slot span is "unused" (by most part, it only
// stores the offset from the head page). In other words, the metadata for the
// slot span is stored only in the first `PartitionPage` of the slot span.
// Metadata accesses to other `PartitionPage`s are redirected to the first
// `PartitionPage`.
//
//     +---------------------------------------------+
//     | SuperPageExtentEntry (32 B)                 |
//     | PartitionPage of slot span 1 (32 B, used)   |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 1 (32 B, unused) |
//     | PartitionPage of slot span 2 (32 B, used)   |
//     | PartitionPage of slot span 3 (32 B, used)   |
//     | ...                                         |
//     | PartitionPage of slot span N (32 B, used)   |
//     | PartitionPage of slot span N (32 B, unused) |
//     | PartitionPage of slot span N (32 B, unused) |
//     +---------------------------------------------+
//
// A direct-mapped page has an identical layout at the beginning to fake it
// looking like a super page:
//
//     +---------------------------------+
//     | Guard page (4 KiB)              |
//     | Metadata page (4 KiB)           |
//     | Guard pages (8 KiB)             |
//     | Direct mapped object            |
//     | Guard page (4 KiB, 32-bit only) |
//     +---------------------------------+
//
// A direct-mapped page's metadata page has the following layout (on 64 bit
// architectures. On 32 bit ones, the layout is identical, some sizes are
// different due to smaller pointers.):
//
//     +----------------------------------+
//     | SuperPageExtentEntry (32 B)      |
//     | PartitionPage (32 B)             |
//     | PartitionBucket (40 B)           |
//     | PartitionDirectMapExtent (32 B)  |
//     +----------------------------------+
//
// See |PartitionDirectMapMetadata| for details.

inline constexpr size_t kGiB = 1024 * 1024 * 1024ull;
inline constexpr size_t kSuperPageShift = 21;  // 2 MiB
inline constexpr size_t kSuperPageSize = 1 << kSuperPageShift;
inline constexpr size_t kSuperPageAlignment = kSuperPageSize;
inline constexpr size_t kSuperPageOffsetMask = kSuperPageAlignment - 1;
inline constexpr size_t kSuperPageBaseMask = ~kSuperPageOffsetMask;

// PartitionAlloc's address space is split into pools. See `glossary.md`.
enum pool_handle : unsigned {
  kNullPoolHandle = 0u,

  kRegularPoolHandle,
  kBRPPoolHandle,
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
  kConfigurablePoolHandle,
#endif

// New pool_handles will be added here.

#if PA_BUILDFLAG(ENABLE_THREAD_ISOLATION)
  // The thread isolated pool must come last since we write-protect its entry in
  // the metadata tables, e.g. AddressPoolManager::aligned_pools_
  kThreadIsolatedPoolHandle,
#endif
  kMaxPoolHandle
};

// Maximum pool size. With exception of Configurable Pool, it is also
// the actual size, unless PA_DYNAMICALLY_SELECT_POOL_SIZE is set, which
// allows to choose a different size at initialization time for certain
// configurations.
//
// Special-case Android and iOS, which incur test failures with larger
// pools. Regardless, allocating >8GiB with malloc() on these platforms is
// unrealistic as of 2022.
//
// Special-case ARM64 ChromeOS, which only has a 512 GiB virtual address space.
// Allocating >8 GiB limits the success rate V8 sandbox reservation
// (https://b/537767842).
//
// When pointer compression is enabled, we cannot use large pools (at most
// 8GB for each of the glued pools).
#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)
#if PA_BUILDFLAG(IS_ANDROID) || PA_BUILDFLAG(IS_IOS) ||             \
    PA_BUILDFLAG(IS_CHROMEOS) && PA_BUILDFLAG(PA_ARCH_CPU_ARM64) || \
    PA_BUILDFLAG(ENABLE_POINTER_COMPRESSION)
constexpr size_t kPoolMaxSize = 8 * kGiB;
#else
constexpr size_t kPoolMaxSize = 16 * kGiB;
#endif
#else  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
constexpr size_t kPoolMaxSize = 4 * kGiB;
#endif

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
  kSchedulerLoopQuarantineForSanitizedObject = 1 << 3,
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

// Intentionally set to less than 2GiB to make sure that a 2GiB allocation
// fails. This is a security choice in Chrome, to help making size_t vs int bugs
// harder to exploit.
//
// The definition of MaxAllocationSize does only depend on constants that are
// unconditionally constexpr. Therefore it is not necessary to use
// PAGE_ALLOCATOR_CONSTANTS_DECLARE_CONSTEXPR here.
PA_ALWAYS_INLINE constexpr size_t MaxAllocationSize() {
  // Subtract kSuperPageSize to accommodate for granularity inside
  // PartitionRoot::GetDirectMapReservationSize.
  return (1UL << 31) - internal::kSuperPageSize;
}

}  // namespace partition_alloc

#endif  // PARTITION_ALLOC_PARTITION_ALLOC_PUBLIC_CONSTANTS_H_
