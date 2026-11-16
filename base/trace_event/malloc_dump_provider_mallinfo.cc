// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <malloc.h>
#include <stddef.h>

#include "base/numerics/clamped_math.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/malloc_dump_provider_internal.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <features.h>
#endif

namespace base::trace_event {

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// static
// mallinfo(3) is the only backend.
const char MallocDumpProvider::kSuballocationTarget[] =
    "malloc/allocated_objects/sys_malloc";
#endif
// static
const char MallocDumpProvider::kSysMalloc[] = "malloc/sys_malloc";
const char MallocDumpProvider::kSysMallocAllocatedObjects[] =
    "malloc/allocated_objects/sys_malloc";

namespace internal {

MallinfoTotals MallinfoTotalsFromSizeT(size_t arena,
                                       size_t hblkhd,
                                       size_t uordblks) {
  MallinfoTotals totals;
  // In case of Android's jemalloc |arena| is 0 and the outer pages size is
  // reported by |hblkhd|. In case of dlmalloc the total is given by
  // |arena| + |hblkhd|. For more details see link: http://goo.gl/fMR8lF.
  totals.held_size = ClampAdd(arena, hblkhd);
  // Total allocated space is given by |uordblks|.
  totals.allocated_size = uordblks;
  return totals;
}

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
MallinfoTotals MallinfoTotalsFromInt(int arena, int hblkhd, int uordblks) {
  return MallinfoTotalsFromSizeT(static_cast<unsigned int>(arena),
                                 static_cast<unsigned int>(hblkhd),
                                 static_cast<unsigned int>(uordblks));
}
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)

MallocTotals ReportSystemAllocator(ProcessMemoryDump* pmd,
                                   MemoryDumpLevelOfDetail level_of_detail) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && !BUILDFLAG(IS_ANDROID)
  // Only PartitionAlloc is reported.
  return {};
#else
#if defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if __GLIBC_PREREQ(2, 33)
#define MALLINFO2_FOUND_IN_LIBC
  struct mallinfo2 info = mallinfo2();
#endif
#endif  // defined(__GLIBC__) && defined(__GLIBC_PREREQ)
#if !defined(MALLINFO2_FOUND_IN_LIBC)
  struct mallinfo info = mallinfo();
#endif
#if (BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)) && \
    !defined(MALLINFO2_FOUND_IN_LIBC)
  const MallinfoTotals mallinfo_totals =
      MallinfoTotalsFromInt(info.arena, info.hblkhd, info.uordblks);
#else
  const MallinfoTotals mallinfo_totals =
      MallinfoTotalsFromSizeT(info.arena, info.hblkhd, info.uordblks);
#endif
#undef MALLINFO2_FOUND_IN_LIBC
  const size_t held_size = mallinfo_totals.held_size;
  const size_t total_allocated_size = mallinfo_totals.allocated_size;

  // Held bytes not handed out to a live allocation: free lists, block headers,
  // alignment padding and released pages. Not part of the footprint below.
  const size_t wasted =
      held_size >= total_allocated_size ? held_size - total_allocated_size : 0;

  const MallocTotals totals = {
      .virtual_size = held_size,
      // mallinfo(3) has no resident figure, and the held bytes include pages
      // the allocator has already released with madvise(), so only the live
      // objects are taken to be resident. This underestimates the footprint.
      .resident_size = total_allocated_size,
      .allocated_objects_size = total_allocated_size,
  };

  if (!pmd) {
    return totals;
  }

  CreateBackendDumps(pmd, level_of_detail, MallocDumpProvider::kSysMalloc,
                     MallocDumpProvider::kSysMallocAllocatedObjects,
                     {.resident_size = total_allocated_size,
                      .virtual_size = held_size,
                      .committed_size = held_size,
                      .allocated_objects_size = total_allocated_size,
                      .wasted = wasted},
                     ObjectsEdge::kSuballocation);
  return totals;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) &&
        // !BUILDFLAG(IS_ANDROID)
}

void AttributeAllocatedObjectsPool(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* allocated_objects_dump) {}

}  // namespace internal

}  // namespace base::trace_event
