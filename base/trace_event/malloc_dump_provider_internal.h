// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_INTERNAL_H_
#define BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_INTERNAL_H_

#include <stddef.h>
#include <stdint.h>

#include <optional>

#include "base/base_export.h"
#include "base/memory/raw_ptr.h"
#include "base/trace_event/memory_dump_request_args.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"

namespace base::trace_event {

class MemoryAllocatorDump;
class ProcessMemoryDump;

namespace internal {

// What a malloc backend adds to the malloc and malloc/allocated_objects dumps.
struct MallocTotals {
  MallocTotals& operator+=(const MallocTotals& other);

  size_t virtual_size = 0;
  size_t resident_size = 0;
  size_t allocated_objects_size = 0;
  size_t allocated_objects_count = 0;
  // Waste already reported under the backend's own dump, and which therefore
  // must not be reported again in malloc/metadata_fragmentation_caches.
  size_t excluded_waste = 0;
};

// What a malloc backend reports in its own dump.
struct BackendStats {
  // The resident footprint of the backend.
  uint64_t resident_size = 0;
  uint64_t virtual_size = 0;
  uint64_t committed_size = 0;
  uint64_t allocated_objects_size = 0;
  // Not reported when the backend does not count its objects.
  std::optional<uint64_t> allocated_objects_count;
  // Committed bytes not handed out to a live allocation.
  uint64_t wasted = 0;
};

// How the objects dump of a backend is kept from counting the objects twice,
// once in it and once in the backend dump.
enum class ObjectsEdge {
  // The objects are suballocated from the backend dump. Not for a backend
  // whose children already sum to its size, as the suballocation adds one.
  kSuballocation,
  // The objects dump owns the backend dump.
  kOwnership,
};

// Adds the scalars every backend dump reports, PartitionAlloc's per-partition
// dumps included, to `dump`. Not the object count: `object_count` in the
// objects dump for backends, `allocated_objects_count` for partitions.
void AddBackendScalars(MemoryAllocatorDump* dump, const BackendStats& stats);

// Creates the dump of a malloc backend and, unless `objects_name` is null, the
// dump of its live objects under malloc/allocated_objects. Each objects dump
// has an edge to its own backend: a single edge out of malloc/allocated_objects
// would not scale to several backends, as a dump can only own one target.
MemoryAllocatorDump* CreateBackendDumps(ProcessMemoryDump* pmd,
                                        MemoryDumpLevelOfDetail level_of_detail,
                                        const char* backend_name,
                                        const char* objects_name,
                                        const BackendStats& stats,
                                        ObjectsEdge edge);

// Reports the platform allocator, which serves malloc() unless PartitionAlloc
// does, and is still used alongside it on some platforms. Defined in the
// platform's malloc_dump_provider_*.cc file, like the functions below.
MallocTotals ReportSystemAllocator(ProcessMemoryDump* pmd,
                                   MemoryDumpLevelOfDetail level_of_detail);

// Attributes `allocated_objects_dump`, the pool other providers suballocate
// from, to the platform allocator. Must come after every backend dump.
void AttributeAllocatedObjectsPool(ProcessMemoryDump* pmd,
                                   MemoryAllocatorDump* allocated_objects_dump);

#if !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)
// Totals derived from mallinfo(3).
struct MallinfoTotals {
  size_t held_size = 0;
  size_t allocated_size = 0;
};

// For mallinfo2(), and bionic's mallinfo(), whose fields are size_t.
BASE_EXPORT MallinfoTotals MallinfoTotalsFromSizeT(size_t arena,
                                                   size_t hblkhd,
                                                   size_t uordblks);

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
// For glibc's legacy mallinfo(), which truncates its size_t counters into int
// fields. Reading them as unsigned recovers values up to 4 GiB; larger ones
// still wrap, undetectably.
BASE_EXPORT MallinfoTotals MallinfoTotalsFromInt(int arena,
                                                 int hblkhd,
                                                 int uordblks);
#endif  // BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS)
#endif  // !BUILDFLAG(IS_WIN) && !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA)

#if BUILDFLAG(IS_WIN)
// Aggregated counts produced by walking a Windows heap with HeapWalk.
struct WinHeapInfo {
  size_t committed_size = 0;
  size_t uncommitted_size = 0;
  size_t allocated_size = 0;
  size_t block_count = 0;

  // Walks `heap` (a HANDLE) and accumulates the totals. Exposed for testing.
  // Production uses a specific path for CRT-heap.
  static BASE_EXPORT WinHeapInfo FromHandleForTesting(void* heap);
};
#endif  // BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
struct PartitionAllocReport {
  MallocTotals totals;
  // malloc/partitions.
  raw_ptr<MemoryAllocatorDump> partitions_dump = nullptr;
  uint64_t syscall_count = 0;
  size_t cumulative_brp_quarantined_size = 0;
  size_t cumulative_brp_quarantined_count = 0;
};

// Reports the partitions serving malloc(), and their sum in malloc/partitions.
PartitionAllocReport ReportPartitionAlloc(
    ProcessMemoryDump* pmd,
    MemoryDumpLevelOfDetail level_of_detail);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace internal

}  // namespace base::trace_event

#endif  // BASE_TRACE_EVENT_MALLOC_DUMP_PROVIDER_INTERNAL_H_
