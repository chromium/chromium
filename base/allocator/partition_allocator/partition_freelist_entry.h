// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Indirection header that allows callers to use
// `PartitionFreelistEntry` without regard for the implementation.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_

#include <cstddef>

#include "base/allocator/partition_allocator/partition_alloc_base/compiler_specific.h"
#include "base/allocator/partition_allocator/partition_alloc_base/component_export.h"
#include "base/allocator/partition_allocator/partition_alloc_buildflags.h"

namespace partition_alloc::internal {

[[noreturn]] PA_NOINLINE PA_COMPONENT_EXPORT(
    PARTITION_ALLOC) void FreelistCorruptionDetected(size_t slot_size);

}  // namespace partition_alloc::internal

#if BUILDFLAG(USE_FREELIST_POOL_OFFSETS)
// New header goes here
#else
#include "base/allocator/partition_allocator/encoded_freelist.h"  // IWYU pragma: export
#endif  // BUILDFLAG(USE_FREELIST_POOL_OFFSETS)

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_FREELIST_ENTRY_H_
