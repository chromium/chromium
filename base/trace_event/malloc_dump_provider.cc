// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <stddef.h>
#include <stdint.h>

#include "base/trace_event/malloc_dump_provider_internal.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/buildflags.h"

namespace base::trace_event {

namespace internal {

MallocTotals& MallocTotals::operator+=(const MallocTotals& other) {
  virtual_size += other.virtual_size;
  resident_size += other.resident_size;
  allocated_objects_size += other.allocated_objects_size;
  allocated_objects_count += other.allocated_objects_count;
  excluded_waste += other.excluded_waste;
  return *this;
}

void AddBackendScalars(MemoryAllocatorDump* dump, const BackendStats& stats) {
  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, stats.resident_size);
  dump->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                  stats.virtual_size);
  dump->AddScalar("virtual_committed_size", MemoryAllocatorDump::kUnitsBytes,
                  stats.committed_size);
  dump->AddScalar("allocated_objects_size", MemoryAllocatorDump::kUnitsBytes,
                  stats.allocated_objects_size);
  dump->AddScalar("wasted", MemoryAllocatorDump::kUnitsBytes, stats.wasted);
  dump->AddScalar("fragmentation", "percent",
                  stats.committed_size == 0
                      ? 0
                      : 100 * stats.wasted / stats.committed_size);
}

MemoryAllocatorDump* CreateBackendDumps(ProcessMemoryDump* pmd,
                                        MemoryDumpLevelOfDetail level_of_detail,
                                        const char* backend_name,
                                        const char* objects_name,
                                        const BackendStats& stats,
                                        ObjectsEdge edge) {
  MemoryAllocatorDump* backend_dump = pmd->CreateAllocatorDump(backend_name);
  AddBackendScalars(backend_dump, stats);

  if (!objects_name) {
    return backend_dump;
  }
  MemoryAllocatorDump* objects_dump = pmd->CreateAllocatorDump(objects_name);
  objects_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          stats.allocated_objects_size);
  if (stats.allocated_objects_count.has_value()) {
    objects_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                            MemoryAllocatorDump::kUnitsObjects,
                            *stats.allocated_objects_count);
  }
  // AddSuballocation() creates no dumps at all in background mode, which would
  // count the objects twice under malloc, so own the backend dump there.
  if (edge == ObjectsEdge::kSuballocation &&
      level_of_detail != MemoryDumpLevelOfDetail::kBackground) {
    pmd->AddSuballocation(objects_dump->guid(), backend_name);
  } else {
    pmd->AddOwnershipEdge(objects_dump->guid(), backend_dump->guid());
  }
  return backend_dump;
}

}  // namespace internal

// static
const char MallocDumpProvider::kAllocatedObjects[] = "malloc/allocated_objects";

// static
MallocDumpProvider* MallocDumpProvider::GetInstance() {
  return Singleton<MallocDumpProvider,
                   LeakySingletonTraits<MallocDumpProvider>>::get();
}

MallocDumpProvider::MallocDumpProvider() = default;
MallocDumpProvider::~MallocDumpProvider() = default;

// Called at trace dump point time. Creates a snapshot the memory counters for
// the current process.
bool MallocDumpProvider::OnMemoryDump(const MemoryDumpArgs& args,
                                      ProcessMemoryDump* pmd) {
  {
    base::AutoLock auto_lock(emit_metrics_on_memory_dump_lock_);
    if (!emit_metrics_on_memory_dump_) {
      return true;
    }
  }

  internal::MallocTotals totals;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  const internal::PartitionAllocReport partition_alloc_report =
      internal::ReportPartitionAlloc(pmd, args.level_of_detail);
  totals += partition_alloc_report.totals;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  // Even when PartitionAlloc is used, WinHeap / System malloc is still used as
  // well, report its statistics.
  totals += internal::ReportSystemAllocator(pmd, args.level_of_detail);

  MemoryAllocatorDump* outer_dump = pmd->CreateAllocatorDump("malloc");
  outer_dump->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                        totals.virtual_size);
  outer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, totals.resident_size);
  // The same total as malloc/allocated_objects's size, kept here as well
  // because that size is recomputed when the dump graph is processed: other
  // providers (e.g. V8, Skia) suballocate from malloc/allocated_objects, which
  // makes it larger than what malloc itself reported. This scalar is not.
  outer_dump->AddScalar("allocated_objects_size",
                        MemoryAllocatorDump::kUnitsBytes,
                        totals.allocated_objects_size);

  MemoryAllocatorDump* inner_dump = pmd->CreateAllocatorDump(kAllocatedObjects);
  inner_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes,
                        totals.allocated_objects_size);
  if (totals.allocated_objects_count != 0) {
    inner_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                          MemoryAllocatorDump::kUnitsObjects,
                          totals.allocated_objects_count);
  }

  int64_t waste = static_cast<int64_t>(totals.resident_size -
                                       totals.allocated_objects_size);

  // A backend which reports its own waste has already accounted for it, so only
  // the remainder belongs here. Still report it, as on some platforms no
  // backend captures all of malloc()'s memory footprint.
  waste -= static_cast<int64_t>(totals.excluded_waste);

  if (waste > 0) {
    // Explicitly specify why is extra memory resident. In mac and ios it
    // accounts for the fragmentation and metadata.
    MemoryAllocatorDump* other_dump =
        pmd->CreateAllocatorDump("malloc/metadata_fragmentation_caches");
    other_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          static_cast<uint64_t>(waste));
  }

  // Must come after every backend dump, which it picks its target from.
  internal::AttributeAllocatedObjectsPool(pmd, inner_dump);

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ReportPerMinuteStats(pmd, outer_dump, partition_alloc_report);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  return true;
}

}  // namespace base::trace_event
