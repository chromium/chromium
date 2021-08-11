// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/trace_event/malloc_dump_provider.h"

#include <stddef.h>

#include <unordered_map>

#include "base/allocator/allocator_extension.h"
#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_bucket_lookup.h"
#include "base/debug/profiler.h"
#include "base/format_macros.h"
#include "base/memory/nonscannable_memory.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/stringprintf.h"
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

#if defined(OS_LINUX) || defined(OS_CHROMEOS) || defined(OS_ANDROID)
#include <features.h>
#endif

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/allocator/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

#if defined(PA_THREAD_CACHE_ALLOC_STATS)
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
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

void ReportWinHeapStats(MemoryDumpLevelOfDetail level_of_detail,
                        ProcessMemoryDump* pmd,
                        size_t* total_virtual_size,
                        size_t* resident_size,
                        size_t* allocated_objects_size,
                        size_t* allocated_objects_count) {
  // This is too expensive on Windows, crbug.com/780735.
  if (level_of_detail == MemoryDumpLevelOfDetail::DETAILED) {
    WinHeapInfo main_heap_info = {};
    WinHeapMemoryDumpImpl(&main_heap_info);
    *total_virtual_size +=
        main_heap_info.committed_size + main_heap_info.uncommitted_size;
    // Resident size is approximated with committed heap size. Note that it is
    // possible to do this with better accuracy on windows by intersecting the
    // working set with the virtual memory ranges occuipied by the heap. It's
    // not clear that this is worth it, as it's fairly expensive to do.
    *resident_size += main_heap_info.committed_size;
    *allocated_objects_size += main_heap_info.allocated_size;
    *allocated_objects_count += main_heap_info.block_count;

    if (pmd) {
      MemoryAllocatorDump* win_heap_dump =
          pmd->CreateAllocatorDump("malloc/win_heap");
      win_heap_dump->AddScalar("size", "bytes", main_heap_info.allocated_size);
    }
  }
}
#endif  // defined(OS_WIN)

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void ReportPartitionAllocStats(ProcessMemoryDump* pmd,
                               MemoryDumpLevelOfDetail level_of_detail,
                               size_t* total_virtual_size,
                               size_t* resident_size,
                               size_t* allocated_objects_size) {
  MemoryDumpPartitionStatsDumper partition_stats_dumper("malloc", pmd,
                                                        level_of_detail);
  bool is_light_dump = level_of_detail == MemoryDumpLevelOfDetail::BACKGROUND;

  auto* allocator = internal::PartitionAllocMalloc::Allocator();
  allocator->DumpStats("allocator", is_light_dump, &partition_stats_dumper);

  auto* original_allocator =
      internal::PartitionAllocMalloc::OriginalAllocator();
  if (original_allocator) {
    original_allocator->DumpStats("original", is_light_dump,
                                  &partition_stats_dumper);
  }
  auto* aligned_allocator = internal::PartitionAllocMalloc::AlignedAllocator();
  if (aligned_allocator != allocator) {
    aligned_allocator->DumpStats("aligned", is_light_dump,
                                 &partition_stats_dumper);
  }
  auto& nonscannable_allocator = internal::NonScannableAllocator::Instance();
  if (auto* root = nonscannable_allocator.root())
    root->DumpStats("nonscannable", is_light_dump, &partition_stats_dumper);

  *total_virtual_size += partition_stats_dumper.total_resident_bytes();
  *resident_size += partition_stats_dumper.total_active_bytes();
  *allocated_objects_size += partition_stats_dumper.total_active_bytes();
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
  ReportPartitionAllocStats(pmd, args.level_of_detail, &total_virtual_size,
                            &resident_size, &allocated_objects_size);

#if OS_WIN
  ReportWinHeapStats(args.level_of_detail, pmd, &total_virtual_size,
                     &resident_size, &allocated_objects_size,
                     &allocated_objects_count);
#endif
  // TODO(keishi): Add glibc malloc on Android
#elif BUILDFLAG(USE_TCMALLOC)
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
  ReportWinHeapStats(args.level_of_detail, nullptr, &total_virtual_size,
                     &resident_size, &allocated_objects_size,
                     &allocated_objects_count);
#elif defined(OS_FUCHSIA)
// TODO(fuchsia): Port, see https://crbug.com/706592.
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
#undef MALLINFO2_FOUND_IN_LIBC
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

  outer_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                        MemoryAllocatorDump::kUnitsBytes,
                        allocated_objects_size);
  if (allocated_objects_count != 0) {
    outer_dump->AddScalar(MemoryAllocatorDump::kNameObjectCount,
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
// static
const char* MemoryDumpPartitionStatsDumper::kPartitionsDumpName = "partitions";

std::string GetPartitionDumpName(const char* root_name,
                                 const char* partition_name) {
  return base::StringPrintf("%s/%s/%s", root_name,
                            MemoryDumpPartitionStatsDumper::kPartitionsDumpName,
                            partition_name);
}

void MemoryDumpPartitionStatsDumper::PartitionDumpTotals(
    const char* partition_name,
    const base::PartitionMemoryStats* memory_stats) {
  total_mmapped_bytes_ += memory_stats->total_mmapped_bytes;
  total_resident_bytes_ += memory_stats->total_resident_bytes;
  total_active_bytes_ += memory_stats->total_active_bytes;

  std::string dump_name = GetPartitionDumpName(root_name_, partition_name);
  MemoryAllocatorDump* allocator_dump =
      memory_dump_->CreateAllocatorDump(dump_name);
  allocator_dump->AddScalar("size", "bytes",
                            memory_stats->total_resident_bytes);
  allocator_dump->AddScalar("allocated_objects_size", "bytes",
                            memory_stats->total_active_bytes);
  allocator_dump->AddScalar("virtual_size", "bytes",
                            memory_stats->total_mmapped_bytes);
  allocator_dump->AddScalar("virtual_committed_size", "bytes",
                            memory_stats->total_committed_bytes);
  allocator_dump->AddScalar("max_committed_size", "bytes",
                            memory_stats->max_committed_bytes);
  allocator_dump->AddScalar("allocated_size", "bytes",
                            memory_stats->total_allocated_bytes);
  allocator_dump->AddScalar("max_allocated_size", "bytes",
                            memory_stats->max_allocated_bytes);
  allocator_dump->AddScalar("decommittable_size", "bytes",
                            memory_stats->total_decommittable_bytes);
  allocator_dump->AddScalar("discardable_size", "bytes",
                            memory_stats->total_discardable_bytes);
  if (memory_stats->has_thread_cache) {
    const auto& thread_cache_stats = memory_stats->current_thread_cache_stats;
    auto* thread_cache_dump = memory_dump_->CreateAllocatorDump(
        dump_name + "/thread_cache/main_thread");
    ReportPartitionAllocThreadCacheStats(memory_dump_, thread_cache_dump,
                                         thread_cache_stats, ".MainThread",
                                         detailed_);

    const auto& all_thread_caches_stats = memory_stats->all_thread_caches_stats;
    auto* all_thread_caches_dump =
        memory_dump_->CreateAllocatorDump(dump_name + "/thread_cache");
    ReportPartitionAllocThreadCacheStats(memory_dump_, all_thread_caches_dump,
                                         all_thread_caches_stats, "",
                                         detailed_);
  }
}

void MemoryDumpPartitionStatsDumper::PartitionsDumpBucketStats(
    const char* partition_name,
    const base::PartitionBucketMemoryStats* memory_stats) {
  DCHECK(memory_stats->is_valid);
  std::string dump_name = GetPartitionDumpName(root_name_, partition_name);
  if (memory_stats->is_direct_map) {
    dump_name.append(base::StringPrintf("/buckets/directMap_%" PRIu64, ++uid_));
  } else {
    dump_name.append(base::StringPrintf("/buckets/bucket_%" PRIu32,
                                        memory_stats->bucket_slot_size));
  }

  MemoryAllocatorDump* allocator_dump =
      memory_dump_->CreateAllocatorDump(dump_name);
  allocator_dump->AddScalar("size", "bytes", memory_stats->resident_bytes);
  allocator_dump->AddScalar("allocated_objects_size", "bytes",
                            memory_stats->active_bytes);
  allocator_dump->AddScalar("slot_size", "bytes",
                            memory_stats->bucket_slot_size);
  allocator_dump->AddScalar("decommittable_size", "bytes",
                            memory_stats->decommittable_bytes);
  allocator_dump->AddScalar("discardable_size", "bytes",
                            memory_stats->discardable_bytes);
  // TODO(bartekn): Rename the scalar names.
  allocator_dump->AddScalar("total_slot_span_size", "bytes",
                            memory_stats->allocated_slot_span_size);
  allocator_dump->AddScalar("active_slot_spans", "objects",
                            memory_stats->num_active_slot_spans);
  allocator_dump->AddScalar("full_slot_spans", "objects",
                            memory_stats->num_full_slot_spans);
  allocator_dump->AddScalar("empty_slot_spans", "objects",
                            memory_stats->num_empty_slot_spans);
  allocator_dump->AddScalar("decommitted_slot_spans", "objects",
                            memory_stats->num_decommitted_slot_spans);
}

void ReportPartitionAllocThreadCacheStats(ProcessMemoryDump* pmd,
                                          MemoryAllocatorDump* dump,
                                          const ThreadCacheStats& stats,
                                          const std::string& metrics_suffix,
                                          bool detailed) {
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

  if (stats.alloc_count) {
    int hit_rate_percent =
        static_cast<int>((100 * stats.alloc_hits) / stats.alloc_count);
    base::UmaHistogramPercentage(
        "Memory.PartitionAlloc.ThreadCache.HitRate" + metrics_suffix,
        hit_rate_percent);
    int batch_fill_rate_percent =
        static_cast<int>((100 * stats.batch_fill_count) / stats.alloc_count);
    base::UmaHistogramPercentage(
        "Memory.PartitionAlloc.ThreadCache.BatchFillRate" + metrics_suffix,
        batch_fill_rate_percent);

#if defined(PA_THREAD_CACHE_ALLOC_STATS)
    if (detailed) {
      base::internal::BucketIndexLookup lookup{};
      std::string name = dump->absolute_name();
      for (size_t i = 0; i < kNumBuckets; i++) {
        size_t bucket_size = lookup.bucket_sizes()[i];
        if (bucket_size == kInvalidBucketSize)
          continue;
        std::string dump_name = base::StringPrintf(
            "%s/buckets_alloc/%d", name.c_str(), static_cast<int>(bucket_size));
        auto* buckets_alloc_dump = pmd->CreateAllocatorDump(dump_name);
        buckets_alloc_dump->AddScalar("count", "objects",
                                      stats.allocs_per_bucket_[i]);
      }
    }
#endif  // defined(PA_THREAD_CACHE_ALLOC_STATS)
  }
}
#endif  // BUILDFLAG(USE_PARTITION_ALLOC)

}  // namespace trace_event
}  // namespace base
