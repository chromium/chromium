// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_

#include <cstddef>

#include "partition_alloc/partition_alloc_base/bits.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/component_export.h"
#include "partition_alloc/partition_alloc_buildflags.h"
#include "partition_alloc/partition_alloc_constants.h"

namespace partition_alloc::internal {

[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

}  // namespace partition_alloc::internal

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
#include "partition_alloc/pool_offset_freelist.h"  // IWYU pragma: export
#else
#include "partition_alloc/encoded_next_freelist.h"  // IWYU pragma: export
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

namespace partition_alloc::internal {

// Assertions that are agnostic to the implementation of the freelist.

static_assert(kSmallestBucket >= sizeof(EncodedNextFreelistEntry),
              "Need enough space for freelist entries in the smallest slot");
#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
static_assert(kSmallestBucket >= sizeof(PoolOffsetFreelistEntry),
              "Need enough space for freelist entries in the smallest slot");
#endif

// Since the free list pointer and in-slot metadata can share slot at the same
// time in the "previous slot" mode, make sure that the smallest bucket can fit
// both.
// TODO(crbug.com/1511221): Allow in the "same slot" mode. It should work just
// fine, because it's either-or. A slot never hosts both at the same time.
#if BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
namespace {
// The smallest bucket that is actually used. Note that the smallest request is
// 1 (if it's 0, it gets patched to 1), and in-slot metadata gets added to it.
constexpr size_t kSmallestUsedBucket =
    base::bits::AlignUp(1 + sizeof(InSlotMetadata), kSmallestBucket);
}

static_assert(
    kSmallestUsedBucket >=
        sizeof(EncodedNextFreelistEntry) + sizeof(InSlotMetadata),
    "Need enough space for freelist entries and the in-slot metadata in the "
    "smallest *used* slot");

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
static_assert(
    kSmallestUsedBucket >=
        sizeof(PoolOffsetFreelistEntry) + sizeof(InSlotMetadata),
    "Need enough space for freelist entries and the in-slot metadata in the "
    "smallest *used* slot");
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
#endif  // BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

using PartitionFreelistEntry = EncodedNextFreelistEntry;

}  // namespace partition_alloc::internal

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_SRC_PARTITION_ALLOC_PARTITION_FREELIST_ENTRY_H_
