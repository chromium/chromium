// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <stddef.h>

#include <unordered_map>

#include "base/allocator/allocator_extension.h"
#include "base/allocator/buildflags.h"
#include "base/debug/profiler.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"

#if defined(OS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#if defined(OS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

namespace base {
namespace trace_event {

namespace {
#if defined(OS_WIN)
// A structure containing some information about a given heap.
struct WinHeapInfo {
  size_t committed_size;
  size_t uncommitted_size;
  size_t allocated_size;
  size_t block_count;
};

// NOTE: crbug.com/665516
// Unfortunately, there is no safe way to collect information from secondary
// heaps due to limitations and racy nature of this piece of WinAPI.
void WinHeapMemoryDumpImpl(WinHeapInfo* crt_heap_info) {
  // Iterate through whichever heap our CRT is using.
  HANDLE crt_heap = reinterpret_cast<HANDLE>(_get_heap_handle());
  ::HeapLock(crt_heap);
  PROCESS_HEAP_ENTRY heap_entry;
  heap_entry.lpData = nullptr;
  // Walk over all the entries in the main heap.
  while (::HeapWalk(crt_heap, &heap_entry) != FALSE) {
    if ((heap_entry.wFlags & PROCESS_HEAP_ENTRY_BUSY) != 0) {
      crt_heap_info->allocated_size += heap_entry.cbData;
      crt_heap_info->block_count++;
    } else if ((heap_entry.wFlags & PROCESS_HEAP_REGION) != 0) {
      crt_heap_info->committed_size += heap_entry.Region.dwCommittedSize;
      crt_heap_info->uncommitted_size += heap_entry.Region.dwUnCommittedSize;
    }
  }
  CHECK(::HeapUnlock(crt_heap) == TRUE);
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void ReportPartitionAllocStats(ProcessMemoryDump* pmd, bool detailed) {
  SimplePartitionStatsDumper allocator_dumper;
  internal::PartitionAllocMalloc::Allocator()->DumpStats(
      "malloc", !detailed /* is_light_dump */, &allocator_dumper);
  // TODO(bartekn): Dump OriginalAllocator() into "malloc" as well.

  if (allocator_dumper.stats().has_thread_cache) {
    const auto& stats = allocator_dumper.stats().all_thread_caches_stats;
    auto* thread_cache_dump = pmd->CreateAllocatorDump("malloc/thread_cache");
    ReportPartitionAllocThreadCacheStats(thread_cache_dump, stats);
    const auto& main_thread_stats =
        allocator_dumper.stats().current_thread_cache_stats;
    auto* main_thread_cache_dump =
        pmd->CreateAllocatorDump("malloc/thread_cache/main_thread");
    ReportPartitionAllocThreadCacheStats(main_thread_cache_dump,
                                         main_thread_stats);
  }

  // Not reported in UMA, detailed dumps only.
  if (detailed) {
    SimplePartitionStatsDumper aligned_allocator_dumper;
    internal::PartitionAllocMalloc::AlignedAllocator()->DumpStats(
        "malloc/aligned", !detailed /* is_light_dump */,
        &aligned_allocator_dumper);
    // These should be included in the overall figure, so using a child dump.
    auto* aligned_allocator_dump = pmd->CreateAllocatorDump("malloc/aligned");
    // See
    // //base/allocator/allocator_shim_default_dispatch_to_partition_alloc.cc
    // for the sum of the aligned and regular partitions.
    aligned_allocator_dump->AddScalar(
        "virtual_size", MemoryAllocatorDump::kUnitsBytes,
        aligned_allocator_dumper.stats().total_mmapped_bytes);
    aligned_allocator_dump->AddScalar(
        MemoryAllocatorDump::kNameSize, MemoryAllocatorDump::kUnitsBytes,
        aligned_allocator_dumper.stats().total_resident_bytes);
    aligned_allocator_dump->AddScalar(
        "allocated_size", MemoryAllocatorDump::kUnitsBytes,
        aligned_allocator_dumper.stats().total_active_bytes);
  }
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

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
    if (!emit_metrics_on_memory_dump_)
      return true;
  }

  size_t total_virtual_size = 0;
  size_t resident_size = 0;
  size_t allocated_objects_size = 0;
  size_t allocated_objects_count = 0;
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ReportPartitionAllocStats(
      pmd, args.level_of_detail == MemoryDumpLevelOfDetail::DETAILED);
#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if BUILDFLAG(USE_TCMALLOC)
  bool res =
      allocator::GetNumericProperty("generic.heap_size", &total_virtual_size);
  DCHECK(res);
  res = allocator::GetNumericProperty("generic.total_physical_bytes",
                                      &resident_size);
  DCHECK(res);
  res = allocator::GetNumericProperty("generic.current_allocated_bytes",
                                      &allocated_objects_size);
  DCHECK(res);
#elif defined(OS_APPLE)
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);
  total_virtual_size = stats.size_allocated;
  allocated_objects_size = stats.size_in_use;

  // Resident size is approximated pretty well by stats.max_size_in_use.
  // However, on macOS, freed blocks are both resident and reusable, which is
  // semantically equivalent to deallocated. The implementation of libmalloc
  // will also only hold a fixed number of freed regions before actually
  // starting to deallocate them, so stats.max_size_in_use is also not
  // representative of the peak size. As a result, stats.max_size_in_use is
  // typically somewhere between actually resident [non-reusable] pages, and
  // peak size. This is not very useful, so we just use stats.size_in_use for
  // resident_size, even though it's an underestimate and fails to account for
  // fragmentation. See
  // https://bugs.chromium.org/p/chromium/issues/detail?id=695263#c1.
  resident_size = stats.size_in_use;
#elif defined(OS_WIN)
  // This is too expensive on Windows, crbug.com/780735.
  if (args.level_of_detail == MemoryDumpLevelOfDetail::DETAILED) {
    WinHeapInfo main_heap_info = {};
    WinHeapMemoryDumpImpl(&main_heap_info);
    total_virtual_size =
        main_heap_info.committed_size + main_heap_info.uncommitted_size;
    // Resident size is approximated with committed heap size. Note that it is
    // possible to do this with better accuracy on windows by intersecting the
    // working set with the virtual memory ranges occuipied by the heap. It's
    // not clear that this is worth it, as it's fairly expensive to do.
    resident_size = main_heap_info.committed_size;
    allocated_objects_size = main_heap_info.allocated_size;
    allocated_objects_count = main_heap_info.block_count;
  }
#elif defined(OS_FUCHSIA)
// TODO(fuchsia): Port, see https://crbug.com/706592.
#else
  struct mallinfo info = mallinfo();
  // In case of Android's jemalloc |arena| is 0 and the outer pages size is
  // reported by |hblkhd|. In case of dlmalloc the total is given by
  // |arena| + |hblkhd|. For more details see link: http://goo.gl/fMR8lF.
  total_virtual_size = info.arena + info.hblkhd;
  resident_size = info.uordblks;

  // Total allocated space is given by |uordblks|.
  allocated_objects_size = info.uordblks;
#endif

  MemoryAllocatorDump* outer_dump = pmd->CreateAllocatorDump("malloc");
  outer_dump->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                        total_virtual_size);
  outer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes, resident_size);

  MemoryAllocatorDump* inner_dump = pmd->CreateAllocatorDump(kAllocatedObjects);
  inner_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes,
                        allocated_objects_size);
  if (allocated_objects_count != 0) {
    inner_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
                          MemoryAllocatorDump::kUnitsObjects,
                          allocated_objects_count);
  }

  if (resident_size > allocated_objects_size) {
    // Explicitly specify why is extra memory resident. In tcmalloc it accounts
    // for free lists and caches. In mac and ios it accounts for the
    // fragmentation and metadata.
    MemoryAllocatorDump* other_dump =
        pmd->CreateAllocatorDump("malloc/metadata_fragmentation_caches");
    other_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          resident_size - allocated_objects_size);
  }
  return true;
}

void MallocDumpProvider::EnableMetrics() {
  base::AutoLock auto_lock(emit_metrics_on_memory_dump_lock_);
  emit_metrics_on_memory_dump_ = true;
}

void MallocDumpProvider::DisableMetrics() {
  base::AutoLock auto_lock(emit_metrics_on_memory_dump_lock_);
  emit_metrics_on_memory_dump_ = false;
}

#if BUILDFLAG(USE_PARTITION_ALLOC)
void ReportPartitionAllocThreadCacheStats(MemoryAllocatorDump* dump,
                                          const ThreadCacheStats& stats) {
  dump->AddScalar("alloc_count", "scalar", stats.alloc_count);
  dump->AddScalar("alloc_hits", "scalar", stats.alloc_hits);
  dump->AddScalar("alloc_misses", "scalar", stats.alloc_misses);

  dump->AddScalar("alloc_miss_empty", "scalar", stats.alloc_miss_empty);
  dump->AddScalar("alloc_miss_too_large", "scalar", stats.alloc_miss_too_large);

  dump->AddScalar("cache_fill_count", "scalar", stats.cache_fill_count);
  dump->AddScalar("cache_fill_hits", "scalar", stats.cache_fill_hits);
  dump->AddScalar("cache_fill_misses", "scalar", stats.cache_fill_misses);

  dump->AddScalar("batch_fill_count", "scalar", stats.batch_fill_count);

  dump->AddScalar("size", "bytes", stats.bucket_total_memory);
  dump->AddScalar("metadata_overhead", "bytes", stats.metadata_overhead);
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

}  // namespace trace_event
}  // namespace base
