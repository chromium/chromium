// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <malloc/malloc.h>

#include "base/trace_event/malloc_dump_provider_internal.h"
#include "partition_alloc/buildflags.h"

namespace base::trace_event {

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// static
// The pool: there is no backend dump.
const char MallocDumpProvider::kSuballocationTarget[] =
    "malloc/allocated_objects";
#endif

namespace internal {

MallocTotals ReportSystemAllocator(ProcessMemoryDump* pmd,
                                   MemoryDumpLevelOfDetail level_of_detail) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  // Only PartitionAlloc is reported.
  return {};
#else
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);

  return {
      .virtual_size = stats.size_allocated,
      // Resident size is approximated pretty well by stats.max_size_in_use.
      // However, on macOS, freed blocks are both resident and reusable, which
      // is semantically equivalent to deallocated. The implementation of
      // libmalloc will also only hold a fixed number of freed regions before
      // actually starting to deallocate them, so stats.max_size_in_use is also
      // not representative of the peak size. As a result,
      // stats.max_size_in_use is typically somewhere between actually resident
      // [non-reusable] pages, and peak size. This is not very useful, so we
      // just use stats.size_in_use for resident_size, even though it's an
      // underestimate and fails to account for fragmentation. See
      // https://bugs.chromium.org/p/chromium/issues/detail?id=695263#c1.
      .resident_size = stats.size_in_use,
      .allocated_objects_size = stats.size_in_use,
  };
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

void AttributeAllocatedObjectsPool(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* allocated_objects_dump) {}

}  // namespace internal

}  // namespace base::trace_event
