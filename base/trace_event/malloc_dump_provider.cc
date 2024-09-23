// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40284755): Remove this and spanify to fix the errors.
#pragma allow_unsafe_buffers
#endif

#include "base/trace_event/malloc_dump_provider.h"

#include <stddef.h>

#include <unordered_map>

#include "base/allocator/buildflags.h"
#include "base/debug/profiler.h"
#include "base/format_macros.h"
#include "base/metrics/histogram_functions.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/process_memory_dump.h"
#include "base/trace_event/traced_value.h"
#include "build/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_bucket_lookup.h"

#if BUILDFLAG(IS_APPLE)
#include <malloc/malloc.h>
#else
#include <malloc.h>
#endif
#if BUILDFLAG(IS_WIN)
#include <windows.h>
#endif

#if BUILDFLAG(IS_LINUX) || BUILDFLAG(IS_CHROMEOS) || BUILDFLAG(IS_ANDROID)
#include <features.h>
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/no_destructor.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

#if PA_CONFIG(THREAD_CACHE_ALLOC_STATS)
#include "partition_alloc/partition_alloc_constants.h"
#endif

namespace base {
namespace trace_event {

namespace {
#if BUILDFLAG(IS_WIN)
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
  if (level_of_detail == MemoryDumpLevelOfDetail::kDetailed) {
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
      win_heap_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                               MemoryAllocatorDump::kUnitsBytes,
                               main_heap_info.allocated_size);
    }
  }
}
#endif  // BUILDFLAG(IS_WIN)

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void ReportPartitionAllocStats(ProcessMemoryDump* pmd,
                               MemoryDumpLevelOfDetail level_of_detail,
                               size_t* total_virtual_size,
                               size_t* resident_size,
                               size_t* allocated_objects_size,
                               size_t* allocated_objects_count,
                               uint64_t* syscall_count,
                               size_t* cumulative_brp_quarantined_size,
                               size_t* cumulative_brp_quarantined_count) {
  MemoryDumpPartitionStatsDumper partition_stats_dumper("malloc", pmd,
                                                        level_of_detail);
  bool is_light_dump = level_of_detail == MemoryDumpLevelOfDetail::kBackground;

  auto* allocator = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  allocator->DumpStats("allocator", is_light_dump, &partition_stats_dumper);

  auto* original_allocator =
      allocator_shim::internal::PartitionAllocMalloc::OriginalAllocator();
  if (original_allocator) {
    original_allocator->DumpStats("original", is_light_dump,
                                  &partition_stats_dumper);
  }

  *total_virtual_size += partition_stats_dumper.total_resident_bytes();
  *resident_size += partition_stats_dumper.total_resident_bytes();
  *allocated_objects_size += partition_stats_dumper.total_active_bytes();
  *allocated_objects_count += partition_stats_dumper.total_active_count();
  *syscall_count += partition_stats_dumper.syscall_count();
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  *cumulative_brp_quarantined_size +=
      partition_stats_dumper.cumulative_brp_quarantined_bytes();
  *cumulative_brp_quarantined_count +=
      partition_stats_dumper.cumulative_brp_quarantined_count();
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && BUILDFLAG(IS_APPLE)
void ReportAppleAllocStats(size_t* total_virtual_size,
                           size_t* resident_size,
                           size_t* allocated_objects_size) {
  malloc_statistics_t stats = {0};
  malloc_zone_statistics(nullptr, &stats);
  *total_virtual_size += stats.size_allocated;
  *allocated_objects_size += stats.size_in_use;

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
  *resident_size += stats.size_in_use;
}
#endif

#if (PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && BUILDFLAG(IS_ANDROID)) || \
    (!PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && !BUILDFLAG(IS_WIN) &&    \
     !BUILDFLAG(IS_APPLE) && !BUILDFLAG(IS_FUCHSIA))
void ReportMallinfoStats(ProcessMemoryDump* pmd,
                         size_t* total_virtual_size,
                         size_t* resident_size,
                         size_t* allocated_objects_size,
                         size_t* allocated_objects_count) {
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
  *total_virtual_size += checked_cast<size_t>(info.arena + info.hblkhd);
  size_t total_allocated_size = checked_cast<size_t>(info.uordblks);
  *resident_size += total_allocated_size;

  // Total allocated space is given by |uordblks|.
  *allocated_objects_size += total_allocated_size;

  if (pmd) {
    MemoryAllocatorDump* sys_alloc_dump =
        pmd->CreateAllocatorDump("malloc/sys_malloc");
    sys_alloc_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                              MemoryAllocatorDump::kUnitsBytes,
                              total_allocated_size);
  }
}
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
void ReportPartitionAllocThreadCacheStats(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* dump,
    const partition_alloc::ThreadCacheStats& stats,
    const std::string& metrics_suffix,
    bool detailed) {
  dump->AddScalar("alloc_count", MemoryAllocatorDump::kTypeScalar,
                  stats.alloc_count);
  dump->AddScalar("alloc_hits", MemoryAllocatorDump::kTypeScalar,
                  stats.alloc_hits);
  dump->AddScalar("alloc_misses", MemoryAllocatorDump::kTypeScalar,
                  stats.alloc_misses);

  dump->AddScalar("alloc_miss_empty", MemoryAllocatorDump::kTypeScalar,
                  stats.alloc_miss_empty);
  dump->AddScalar("alloc_miss_too_large", MemoryAllocatorDump::kTypeScalar,
                  stats.alloc_miss_too_large);

  dump->AddScalar("cache_fill_count", MemoryAllocatorDump::kTypeScalar,
                  stats.cache_fill_count);
  dump->AddScalar("cache_fill_hits", MemoryAllocatorDump::kTypeScalar,
                  stats.cache_fill_hits);
  dump->AddScalar("cache_fill_misses", MemoryAllocatorDump::kTypeScalar,
                  stats.cache_fill_misses);

  dump->AddScalar("batch_fill_count", MemoryAllocatorDump::kTypeScalar,
                  stats.batch_fill_count);

  dump->AddScalar(MemoryAllocatorDump::kNameSize,
                  MemoryAllocatorDump::kUnitsBytes, stats.bucket_total_memory);
  dump->AddScalar("metadata_overhead", MemoryAllocatorDump::kUnitsBytes,
                  stats.metadata_overhead);

#if PA_CONFIG(THREAD_CACHE_ALLOC_STATS)
  if (stats.alloc_count && detailed) {
    partition_alloc::internal::BucketIndexLookup lookup{};
    std::string name = dump->absolute_name();
    for (size_t i = 0; i < partition_alloc::kNumBuckets; i++) {
      size_t bucket_size = lookup.bucket_sizes()[i];
      if (bucket_size == partition_alloc::kInvalidBucketSize) {
        continue;
      }
      // Covers all normal buckets, that is up to ~1MiB, so 7 digits.
      std::string dump_name = base::StringPrintf(
          "%s/buckets_alloc/%07d", name.c_str(), static_cast<int>(bucket_size));
      auto* buckets_alloc_dump = pmd->CreateAllocatorDump(dump_name);
      buckets_alloc_dump->AddScalar("count", MemoryAllocatorDump::kUnitsObjects,
                                    stats.allocs_per_bucket_[i]);
    }
  }
#endif  // PA_CONFIG(THREAD_CACHE_ALLOC_STATS)
}

void ReportPartitionAllocLightweightQuarantineStats(
    MemoryAllocatorDump* dump,
    const partition_alloc::LightweightQuarantineStats& stats) {
  dump->AddScalar("count", MemoryAllocatorDump::kUnitsObjects, stats.count);
  dump->AddScalar("size_in_bytes", MemoryAllocatorDump::kUnitsBytes,
                  stats.size_in_bytes);
  dump->AddScalar("cumulative_count", MemoryAllocatorDump::kUnitsObjects,
                  stats.cumulative_count);
  dump->AddScalar("cumulative_size_in_bytes", MemoryAllocatorDump::kUnitsBytes,
                  stats.cumulative_size_in_bytes);
  dump->AddScalar("quarantine_miss_count", MemoryAllocatorDump::kUnitsObjects,
                  stats.quarantine_miss_count);
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

}  // namespace

// static
const char MallocDumpProvider::kAllocatedObjects[] = "malloc/allocated_objects";

// static
MallocDumpProvider* MallocDumpProvider::GetInstance() {
  return Singleton<MallocDumpProvider,
                   LeakySingletonTraits<MallocDumpProvider>>::get();
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// static
void MallocDumpProvider::SetExtremeLUDGetStatsCallback(
    ExtremeLUDGetStatsCallback callback) {
  DCHECK(!callback.is_null());
  auto& extreme_lud_get_stats_callback = GetExtremeLUDGetStatsCallback();
  DCHECK(extreme_lud_get_stats_callback.is_null());
  extreme_lud_get_stats_callback = std::move(callback);
}

// static
MallocDumpProvider::ExtremeLUDGetStatsCallback&
MallocDumpProvider::GetExtremeLUDGetStatsCallback() {
  static NoDestructor<MallocDumpProvider::ExtremeLUDGetStatsCallback>
      extreme_lud_get_stats_callback;
  return *extreme_lud_get_stats_callback;
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

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

  size_t total_virtual_size = 0;
  size_t resident_size = 0;
  size_t allocated_objects_size = 0;
  size_t allocated_objects_count = 0;
  uint64_t syscall_count = 0;
  size_t cumulative_brp_quarantined_size = 0;
  size_t cumulative_brp_quarantined_count = 0;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  uint64_t pa_only_resident_size;
  uint64_t pa_only_allocated_objects_size;
#endif

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  ReportPartitionAllocStats(
      pmd, args.level_of_detail, &total_virtual_size, &resident_size,
      &allocated_objects_size, &allocated_objects_count, &syscall_count,
      &cumulative_brp_quarantined_size, &cumulative_brp_quarantined_count);

  pa_only_resident_size = resident_size;
  pa_only_allocated_objects_size = allocated_objects_size;

  // Even when PartitionAlloc is used, WinHeap / System malloc is still used as
  // well, report its statistics.
#if BUILDFLAG(IS_ANDROID)
  ReportMallinfoStats(pmd, &total_virtual_size, &resident_size,
                      &allocated_objects_size, &allocated_objects_count);
#elif BUILDFLAG(IS_WIN)
  ReportWinHeapStats(args.level_of_detail, pmd, &total_virtual_size,
                     &resident_size, &allocated_objects_size,
                     &allocated_objects_count);
#endif  // BUILDFLAG(IS_ANDROID), BUILDFLAG(IS_WIN)

#elif BUILDFLAG(IS_APPLE)
  ReportAppleAllocStats(&total_virtual_size, &resident_size,
                        &allocated_objects_size);
#elif BUILDFLAG(IS_WIN)
  ReportWinHeapStats(args.level_of_detail, nullptr, &total_virtual_size,
                     &resident_size, &allocated_objects_size,
                     &allocated_objects_count);
#elif BUILDFLAG(IS_FUCHSIA)
// TODO(fuchsia): Port, see https://crbug.com/706592.
#else
  ReportMallinfoStats(/*pmd=*/nullptr, &total_virtual_size, &resident_size,
                      &allocated_objects_size, &allocated_objects_count);
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

  int64_t waste = static_cast<int64_t>(resident_size - allocated_objects_size);

  // With PartitionAlloc, reported size under malloc/partitions is the resident
  // size, so it already includes fragmentation. Meaning that "malloc/"'s size
  // would double-count fragmentation if we report it under
  // "malloc/metadata_fragmentation_caches" as well.
  //
  // Still report waste, as on some platforms, PartitionAlloc doesn't capture
  // all of malloc()'s memory footprint.
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  int64_t pa_waste = static_cast<int64_t>(pa_only_resident_size -
                                          pa_only_allocated_objects_size);
  waste -= pa_waste;
#endif

  if (waste > 0) {
    // Explicitly specify why is extra memory resident. In mac and ios it
    // accounts for the fragmentation and metadata.
    MemoryAllocatorDump* other_dump =
        pmd->CreateAllocatorDump("malloc/metadata_fragmentation_caches");
    other_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                          MemoryAllocatorDump::kUnitsBytes,
                          static_cast<uint64_t>(waste));
  }

  base::trace_event::MemoryAllocatorDump* partitions_dump = nullptr;
  base::trace_event::MemoryAllocatorDump* elud_dump_for_small_objects = nullptr;
  ExtremeLUDStats elud_stats_for_small_objects;
  base::trace_event::MemoryAllocatorDump* elud_dump_for_large_objects = nullptr;
  ExtremeLUDStats elud_stats_for_large_objects;
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  partitions_dump = pmd->CreateAllocatorDump("malloc/partitions");
  pmd->AddOwnershipEdge(inner_dump->guid(), partitions_dump->guid());

  auto& extreme_lud_get_stats_callback = GetExtremeLUDGetStatsCallback();
  if (!extreme_lud_get_stats_callback.is_null()) {
    // The Extreme LUD is enabled.
    elud_dump_for_small_objects =
        pmd->CreateAllocatorDump("malloc/extreme_lud/small_objects");
    elud_dump_for_large_objects =
        pmd->CreateAllocatorDump("malloc/extreme_lud/large_objects");
    const auto elud_stats_set = extreme_lud_get_stats_callback.Run();
    elud_stats_for_small_objects = elud_stats_set.for_small_objects;
    elud_stats_for_large_objects = elud_stats_set.for_large_objects;
    ReportPartitionAllocLightweightQuarantineStats(
        elud_dump_for_small_objects, elud_stats_for_small_objects.lq_stats);
    ReportPartitionAllocLightweightQuarantineStats(
        elud_dump_for_large_objects, elud_stats_for_large_objects.lq_stats);
  }
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

  ReportPerMinuteStats(
      syscall_count, cumulative_brp_quarantined_size,
      cumulative_brp_quarantined_count, elud_stats_for_small_objects,
      elud_stats_for_large_objects, outer_dump, partitions_dump,
      elud_dump_for_small_objects, elud_dump_for_large_objects);

  return true;
}

void MallocDumpProvider::ReportPerMinuteStats(
    uint64_t syscall_count,
    size_t cumulative_brp_quarantined_bytes,
    size_t cumulative_brp_quarantined_count,
    const ExtremeLUDStats& elud_stats_for_small_objects,
    const ExtremeLUDStats& elud_stats_for_large_objects,
    MemoryAllocatorDump* malloc_dump,
    MemoryAllocatorDump* partition_alloc_dump,
    MemoryAllocatorDump* elud_dump_for_small_objects,
    MemoryAllocatorDump* elud_dump_for_large_objects) {
#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
  uint64_t new_syscalls = syscall_count - last_syscall_count_;
  size_t new_brp_quarantined_bytes =
      cumulative_brp_quarantined_bytes - last_cumulative_brp_quarantined_bytes_;
  size_t new_brp_quarantined_count =
      cumulative_brp_quarantined_count - last_cumulative_brp_quarantined_count_;
  base::TimeDelta time_since_last_dump =
      base::TimeTicks::Now() - last_memory_dump_time_;
  auto seconds_since_last_dump = time_since_last_dump.InSecondsF();
  uint64_t syscalls_per_minute =
      static_cast<uint64_t>((60 * new_syscalls) / seconds_since_last_dump);
  malloc_dump->AddScalar("syscalls_per_minute", "count", syscalls_per_minute);
  if (partition_alloc_dump) {
    size_t brp_quarantined_bytes_per_minute =
        (60 * new_brp_quarantined_bytes) / seconds_since_last_dump;
    size_t brp_quarantined_count_per_minute =
        (60 * new_brp_quarantined_count) / seconds_since_last_dump;
    partition_alloc_dump->AddScalar("brp_quarantined_bytes_per_minute",
                                    MemoryAllocatorDump::kUnitsBytes,
                                    brp_quarantined_bytes_per_minute);
    partition_alloc_dump->AddScalar("brp_quarantined_count_per_minute",
                                    MemoryAllocatorDump::kNameObjectCount,
                                    brp_quarantined_count_per_minute);
  }

  auto report_elud_per_minute_stats = [time_since_last_dump,
                                       seconds_since_last_dump](
                                          const ExtremeLUDStats& elud_stats,
                                          CumulativeEludStats&
                                              last_cumulative_elud_stats,
                                          MemoryAllocatorDump* elud_dump) {
    size_t bytes = elud_stats.lq_stats.cumulative_size_in_bytes -
                   last_cumulative_elud_stats.quarantined_bytes;
    size_t count = elud_stats.lq_stats.cumulative_count -
                   last_cumulative_elud_stats.quarantined_count;
    size_t miss_count = elud_stats.lq_stats.quarantine_miss_count -
                        last_cumulative_elud_stats.miss_count;
    elud_dump->AddScalar("bytes_per_minute", MemoryAllocatorDump::kUnitsBytes,
                         60ull * bytes / seconds_since_last_dump);
    elud_dump->AddScalar("count_per_minute",
                         MemoryAllocatorDump::kNameObjectCount,
                         60ull * count / seconds_since_last_dump);
    elud_dump->AddScalar("miss_count_per_minute",
                         MemoryAllocatorDump::kNameObjectCount,
                         60ull * miss_count / seconds_since_last_dump);
    // Given the following three:
    //   capacity := the quarantine storage space
    //   time     := the elapsed time since the last dump
    //   bytes    := the consumed/used bytes since the last dump
    // We can define/calculate the following.
    //   speed    := the consuming speed of the quarantine
    //            = bytes / time
    //   quarantined_time
    //            := the time to use up the capacity
    //               (near to how long an object may be quarantined)
    //            = capacity / speed
    //            = capacity / (bytes / time)
    //            = time * capacity / bytes
    //
    // Note that objects in the quarantine are randomly evicted. So objects may
    // stay in the qurantine longer or shorter depending on object sizes,
    // allocation/deallocation patterns, etc. in addition to pure randomness.
    // So, this is just a rough estimation, not necessarily to be the average.
    if (bytes > 0) {
      elud_dump->AddScalar(
          "quarantined_time", "msec",
          static_cast<uint64_t>(time_since_last_dump.InMilliseconds()) *
              elud_stats.capacity_in_bytes / bytes);
    }
    last_cumulative_elud_stats.quarantined_bytes =
        elud_stats.lq_stats.cumulative_size_in_bytes;
    last_cumulative_elud_stats.quarantined_count =
        elud_stats.lq_stats.cumulative_count;
    last_cumulative_elud_stats.miss_count =
        elud_stats.lq_stats.quarantine_miss_count;
  };
  if (elud_dump_for_small_objects) {
    report_elud_per_minute_stats(elud_stats_for_small_objects,
                                 last_cumulative_elud_stats_for_small_objects_,
                                 elud_dump_for_small_objects);
  }
  if (elud_dump_for_large_objects) {
    report_elud_per_minute_stats(elud_stats_for_large_objects,
                                 last_cumulative_elud_stats_for_large_objects_,
                                 elud_dump_for_large_objects);
  }

  last_memory_dump_time_ = base::TimeTicks::Now();
  last_syscall_count_ = syscall_count;
  last_cumulative_brp_quarantined_bytes_ = cumulative_brp_quarantined_bytes;
  last_cumulative_brp_quarantined_count_ = cumulative_brp_quarantined_count;
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC)
std::string GetPartitionDumpName(const char* root_name,
                                 const char* partition_name) {
  return base::StringPrintf("%s/%s/%s", root_name,
                            MemoryDumpPartitionStatsDumper::kPartitionsDumpName,
                            partition_name);
}

MemoryDumpPartitionStatsDumper::MemoryDumpPartitionStatsDumper(
    const char* root_name,
    ProcessMemoryDump* memory_dump,
    MemoryDumpLevelOfDetail level_of_detail)
    : root_name_(root_name),
      memory_dump_(memory_dump),
      detailed_(level_of_detail != MemoryDumpLevelOfDetail::kBackground) {}

void MemoryDumpPartitionStatsDumper::PartitionDumpTotals(
    const char* partition_name,
    const partition_alloc::PartitionMemoryStats* memory_stats) {
  total_mmapped_bytes_ += memory_stats->total_mmapped_bytes;
  total_resident_bytes_ += memory_stats->total_resident_bytes;
  total_active_bytes_ += memory_stats->total_active_bytes;
  total_active_count_ += memory_stats->total_active_count;
  syscall_count_ += memory_stats->syscall_count;
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  cumulative_brp_quarantined_bytes_ +=
      memory_stats->cumulative_brp_quarantined_bytes;
  cumulative_brp_quarantined_count_ +=
      memory_stats->cumulative_brp_quarantined_count;
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  std::string dump_name = GetPartitionDumpName(root_name_, partition_name);
  MemoryAllocatorDump* allocator_dump =
      memory_dump_->CreateAllocatorDump(dump_name);

  auto total_committed_bytes = memory_stats->total_committed_bytes;
  auto total_active_bytes = memory_stats->total_active_bytes;
  size_t wasted = total_committed_bytes - total_active_bytes;
  DCHECK_GE(total_committed_bytes, total_active_bytes);
  size_t fragmentation =
      total_committed_bytes == 0 ? 0 : 100 * wasted / total_committed_bytes;

  allocator_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_resident_bytes);
  allocator_dump->AddScalar("allocated_objects_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_active_bytes);
  allocator_dump->AddScalar("allocated_objects_count", "count",
                            memory_stats->total_active_count);
  allocator_dump->AddScalar("virtual_size", MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_mmapped_bytes);
  allocator_dump->AddScalar("virtual_committed_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_committed_bytes);
  allocator_dump->AddScalar("max_committed_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->max_committed_bytes);
  allocator_dump->AddScalar("allocated_size", MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_allocated_bytes);
  allocator_dump->AddScalar("max_allocated_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->max_allocated_bytes);
  allocator_dump->AddScalar("decommittable_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_decommittable_bytes);
  allocator_dump->AddScalar("discardable_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_discardable_bytes);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  allocator_dump->AddScalar("brp_quarantined_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_brp_quarantined_bytes);
  allocator_dump->AddScalar("brp_quarantined_count", "count",
                            memory_stats->total_brp_quarantined_count);
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  allocator_dump->AddScalar("syscall_count", "count",
                            memory_stats->syscall_count);
  allocator_dump->AddScalar("syscall_total_time_ms", "ms",
                            memory_stats->syscall_total_time_ns / 1e6);
  allocator_dump->AddScalar("fragmentation", "percent", fragmentation);
  allocator_dump->AddScalar("wasted", MemoryAllocatorDump::kUnitsBytes, wasted);

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

  if (memory_stats->has_scheduler_loop_quarantine) {
    MemoryAllocatorDump* quarantine_dump_total =
        memory_dump_->CreateAllocatorDump(dump_name +
                                          "/scheduler_loop_quarantine");
    ReportPartitionAllocLightweightQuarantineStats(
        quarantine_dump_total,
        memory_stats->scheduler_loop_quarantine_stats_total);
  }
}

void MemoryDumpPartitionStatsDumper::PartitionsDumpBucketStats(
    const char* partition_name,
    const partition_alloc::PartitionBucketMemoryStats* memory_stats) {
  DCHECK(memory_stats->is_valid);
  std::string dump_name = GetPartitionDumpName(root_name_, partition_name);
  if (memory_stats->is_direct_map) {
    dump_name.append(base::StringPrintf("/buckets/directMap_%" PRIu64, ++uid_));
  } else {
    // Normal buckets go up to ~1MiB, 7 digits.
    dump_name.append(base::StringPrintf("/buckets/bucket_%07" PRIu32,
                                        memory_stats->bucket_slot_size));
  }

  MemoryAllocatorDump* allocator_dump =
      memory_dump_->CreateAllocatorDump(dump_name);
  allocator_dump->AddScalar(MemoryAllocatorDump::kNameSize,
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->resident_bytes);
  allocator_dump->AddScalar("allocated_objects_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->active_bytes);
  allocator_dump->AddScalar("slot_size", MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->bucket_slot_size);
  allocator_dump->AddScalar("decommittable_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->decommittable_bytes);
  allocator_dump->AddScalar("discardable_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->discardable_bytes);
  // TODO(bartekn): Rename the scalar names.
  allocator_dump->AddScalar("total_slot_span_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->allocated_slot_span_size);
  allocator_dump->AddScalar("active_slot_spans",
                            MemoryAllocatorDump::kUnitsObjects,
                            memory_stats->num_active_slot_spans);
  allocator_dump->AddScalar("full_slot_spans",
                            MemoryAllocatorDump::kUnitsObjects,
                            memory_stats->num_full_slot_spans);
  allocator_dump->AddScalar("empty_slot_spans",
                            MemoryAllocatorDump::kUnitsObjects,
                            memory_stats->num_empty_slot_spans);
  allocator_dump->AddScalar("decommitted_slot_spans",
                            MemoryAllocatorDump::kUnitsObjects,
                            memory_stats->num_decommitted_slot_spans);
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC)

}  // namespace trace_event
}  // namespace base
