// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <stddef.h>
#include <stdint.h>

#include <string>

#include "base/feature_list.h"
#include "base/format_macros.h"
#include "base/strings/stringprintf.h"
#include "base/trace_event/malloc_dump_provider.h"
#include "base/trace_event/malloc_dump_provider_internal.h"
#include "base/trace_event/memory_allocator_dump.h"
#include "base/trace_event/process_memory_dump.h"
#include "partition_alloc/bucket_lookup.h"  // nogncheck
#include "partition_alloc/buildflags.h"
#include "partition_alloc/in_slot_metadata.h"        // nogncheck
#include "partition_alloc/partition_alloc_config.h"  // nogncheck

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
#include "base/memory/sanitized_object.h"
#include "base/no_destructor.h"
#include "base/time/time.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#endif

#if PA_CONFIG(THREAD_CACHE_ALLOC_STATS)
#include "partition_alloc/partition_alloc_constants.h"  // nogncheck
#endif

namespace base::trace_event {

namespace {

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
// Whether to populate "discardable bytes" in "light" stats reported via
// `MallocDumpProvider::OnMemoryDump`. This involves traversing the free list
// which is expensive.
BASE_FEATURE(kMallocDumpProviderPopulateDiscardableBytes,
             base::FEATURE_DISABLED_BY_DEFAULT);
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

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
    std::string name = dump->absolute_name();
    for (uint16_t i = 0; i < partition_alloc::BucketIndexLookup::kNumBuckets;
         i++) {
      size_t bucket_size = partition_alloc::BucketIndexLookup::GetBucketSize(i);
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

void ReportPartitionAllocSchedulerLoopQuarantineStats(
    MemoryAllocatorDump* dump,
    const partition_alloc::SchedulerLoopQuarantineStats& stats) {
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

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
void ReportExtremeLightweightDetectorQuarantineStats(
    MemoryAllocatorDump* dump,
    const MallocDumpProvider::ExtremeLUDStats& stats) {
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
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

}  // namespace

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
namespace internal {

PartitionAllocReport ReportPartitionAlloc(
    ProcessMemoryDump* pmd,
    MemoryDumpLevelOfDetail level_of_detail) {
  MemoryDumpPartitionStatsDumper partition_stats_dumper("malloc", pmd,
                                                        level_of_detail);
  const bool is_light_dump =
      level_of_detail == MemoryDumpLevelOfDetail::kBackground;
  const bool populate_discardable_bytes =
      !is_light_dump ||
      base::FeatureList::IsEnabled(kMallocDumpProviderPopulateDiscardableBytes);

  auto* allocator = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  allocator->DumpStats("allocator", is_light_dump, populate_discardable_bytes,
                       &partition_stats_dumper);

  auto* original_allocator =
      allocator_shim::internal::PartitionAllocMalloc::OriginalAllocator();
  if (original_allocator) {
    original_allocator->DumpStats("original", is_light_dump,
                                  populate_discardable_bytes,
                                  &partition_stats_dumper);
  }

  // Obtain information from an allocator for leaked security object.
  auto* leaked_security_object_allocator =
      base::internal::LeakedSecurityObjectAllocator();
  leaked_security_object_allocator->DumpStats("leaked", is_light_dump,
                                              populate_discardable_bytes,
                                              &partition_stats_dumper);

  if (!is_light_dump) {
    partition_alloc::PartitionRoot::DumpIntendedLeakStats(
        &partition_stats_dumper);
  }

  PartitionAllocReport report;
  // virtual_size is address space, not resident memory.
  report.totals.virtual_size = partition_stats_dumper.total_mmapped_bytes();
  report.totals.resident_size = partition_stats_dumper.total_resident_bytes();
  report.totals.allocated_objects_size =
      partition_stats_dumper.total_active_bytes();
  report.totals.allocated_objects_count =
      partition_stats_dumper.total_active_count();
  // Each partition reports its resident size, so the resident bytes not handed
  // out to a live allocation are already part of the malloc/partitions subtree.
  // This is not the reported `wasted`, which is relative to committed memory.
  if (report.totals.resident_size >= report.totals.allocated_objects_size) {
    report.totals.excluded_waste =
        report.totals.resident_size - report.totals.allocated_objects_size;
  }
  report.syscall_count = partition_stats_dumper.syscall_count();
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  report.cumulative_brp_quarantined_size =
      partition_stats_dumper.cumulative_brp_quarantined_bytes();
  report.cumulative_brp_quarantined_count =
      partition_stats_dumper.cumulative_brp_quarantined_count();
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  // Summed over the partitions, so that `wasted` and `fragmentation` mean the
  // same here as in each partition's own dump. Other providers suballocate
  // from the objects dump, so it must have no other children, and the
  // partitions' dumps already sum to the size of malloc/partitions.
  report.partitions_dump = CreateBackendDumps(
      pmd, level_of_detail, MallocDumpProvider::kPartitions,
      MallocDumpProvider::kPartitionsAllocatedObjects,
      {.resident_size = report.totals.resident_size,
       .virtual_size = report.totals.virtual_size,
       .committed_size = partition_stats_dumper.total_committed_bytes(),
       .allocated_objects_size = report.totals.allocated_objects_size,
       .allocated_objects_count = report.totals.allocated_objects_count,
       .wasted = partition_stats_dumper.total_wasted_bytes()},
      ObjectsEdge::kOwnership);
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
  if (uint32_t max_ref_count = partition_alloc::internal::InSlotMetadata::
          GetMaxRefCountForReporting();
      max_ref_count > 0) {
    report.partitions_dump->AddScalar("max_brp_ref_count", "count",
                                      max_ref_count);
  }
#endif  // PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)

  return report;
}

}  // namespace internal

// static
const char MallocDumpProvider::kSuballocationTarget[] =
    "malloc/allocated_objects/partitions";
const char MallocDumpProvider::kPartitions[] = "malloc/partitions";
const char MallocDumpProvider::kPartitionsAllocatedObjects[] =
    "malloc/allocated_objects/partitions";

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

void MallocDumpProvider::ReportPerMinuteStats(
    ProcessMemoryDump* pmd,
    MemoryAllocatorDump* malloc_dump,
    const internal::PartitionAllocReport& report) {
  MemoryAllocatorDump* elud_dump_for_small_objects = nullptr;
  ExtremeLUDStats elud_stats_for_small_objects;
  MemoryAllocatorDump* elud_dump_for_large_objects = nullptr;
  ExtremeLUDStats elud_stats_for_large_objects;
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
    ReportExtremeLightweightDetectorQuarantineStats(
        elud_dump_for_small_objects, elud_stats_for_small_objects);
    ReportExtremeLightweightDetectorQuarantineStats(
        elud_dump_for_large_objects, elud_stats_for_large_objects);
  }

  uint64_t new_syscalls = report.syscall_count - last_syscall_count_;
  size_t new_brp_quarantined_bytes = report.cumulative_brp_quarantined_size -
                                     last_cumulative_brp_quarantined_bytes_;
  size_t new_brp_quarantined_count = report.cumulative_brp_quarantined_count -
                                     last_cumulative_brp_quarantined_count_;
  base::TimeDelta time_since_last_dump =
      base::TimeTicks::Now() - last_memory_dump_time_;
  auto seconds_since_last_dump = time_since_last_dump.InSecondsF();
  uint64_t syscalls_per_minute =
      static_cast<uint64_t>((60 * new_syscalls) / seconds_since_last_dump);
  malloc_dump->AddScalar("syscalls_per_minute", "count", syscalls_per_minute);
  if (MemoryAllocatorDump* partition_alloc_dump = report.partitions_dump) {
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

  auto report_elud_per_minute_stats =
      [time_since_last_dump, seconds_since_last_dump](
          const ExtremeLUDStats& elud_stats,
          CumulativeEludStats& last_cumulative_elud_stats,
          MemoryAllocatorDump* elud_dump) {
        size_t bytes = elud_stats.cumulative_size_in_bytes -
                       last_cumulative_elud_stats.quarantined_bytes;
        size_t count = elud_stats.cumulative_count -
                       last_cumulative_elud_stats.quarantined_count;
        size_t miss_count = elud_stats.quarantine_miss_count -
                            last_cumulative_elud_stats.miss_count;
        elud_dump->AddScalar("bytes_per_minute",
                             MemoryAllocatorDump::kUnitsBytes,
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
        // Note that objects in the quarantine are randomly evicted. So objects
        // may stay in the qurantine longer or shorter depending on object
        // sizes, allocation/deallocation patterns, etc. in addition to pure
        // randomness. So, this is just a rough estimation, not necessarily to
        // be the average.
        if (bytes > 0) {
          elud_dump->AddScalar(
              "quarantined_time", "msec",
              static_cast<uint64_t>(time_since_last_dump.InMilliseconds()) *
                  elud_stats.capacity_in_bytes / bytes);
        }
        last_cumulative_elud_stats.quarantined_bytes =
            elud_stats.cumulative_size_in_bytes;
        last_cumulative_elud_stats.quarantined_count =
            elud_stats.cumulative_count;
        last_cumulative_elud_stats.miss_count =
            elud_stats.quarantine_miss_count;
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
  last_syscall_count_ = report.syscall_count;
  last_cumulative_brp_quarantined_bytes_ =
      report.cumulative_brp_quarantined_size;
  last_cumulative_brp_quarantined_count_ =
      report.cumulative_brp_quarantined_count;
}
#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)

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
  size_t wasted = 0;
  // This should always be true, but only if our accounting of committed bytes
  // is consistent, which it isn't. Indeed, with kUseFewerMemoryRegions, we may
  // allocate a slot span before the feature state is known, in which case we
  // commit less, then decommit it after, in which case we subtract the new
  // commit unit, which is larger.
  //
  // Properly handling this would require remembering how much was committed,
  // which complicates bookkeeping, especially as metadata space is
  // limited. Since this is only used to report metrics, which are known to
  // already be quite flawed, and the feature is meant to be temporary (either
  // shipped or abandoned), don't handle this corner case (which should only
  // happen for the initial partition, which is tiny anyway).
  if (total_committed_bytes >= total_active_bytes) {
    wasted = total_committed_bytes - total_active_bytes;
  }
  total_committed_bytes_ += total_committed_bytes;
  total_wasted_bytes_ += wasted;

  internal::AddBackendScalars(
      allocator_dump, {.resident_size = memory_stats->total_resident_bytes,
                       .virtual_size = memory_stats->total_mmapped_bytes,
                       .committed_size = total_committed_bytes,
                       .allocated_objects_size = total_active_bytes,
                       .wasted = wasted});
  allocator_dump->AddScalar("allocated_objects_count", "count",
                            memory_stats->total_active_count);
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
    // The cached slots are already in the partition's buckets, so own them to
    // not count them twice. Without bucket dumps there is no double count.
    if (MemoryAllocatorDump* buckets_dump =
            memory_dump_->GetAllocatorDump(dump_name + "/buckets")) {
      memory_dump_->AddOwnershipEdge(all_thread_caches_dump->guid(),
                                     buckets_dump->guid());
    }
  }

  if (memory_stats->has_scheduler_loop_quarantine) {
    MemoryAllocatorDump* quarantine_dump_total =
        memory_dump_->CreateAllocatorDump(dump_name +
                                          "/scheduler_loop_quarantine");
    ReportPartitionAllocSchedulerLoopQuarantineStats(
        quarantine_dump_total,
        memory_stats->scheduler_loop_quarantine_stats_total);
  }

  allocator_dump->AddScalar("intended_leak_size",
                            MemoryAllocatorDump::kUnitsBytes,
                            memory_stats->total_intended_leak_bytes);
}

void MemoryDumpPartitionStatsDumper::PartitionsDumpBucketStats(
    const char* partition_name,
    const partition_alloc::PartitionBucketMemoryStats* memory_stats) {
  DCHECK(memory_stats->is_valid);
  std::string dump_name = GetPartitionDumpName(root_name_, partition_name);
  // Explicit, so that the thread cache dump can own it.
  memory_dump_->GetOrCreateAllocatorDump(dump_name + "/buckets");
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

void MemoryDumpPartitionStatsDumper::DumpIntendedLeak(uint32_t type_id,
                                                      size_t size) {
  std::string dump_name = base::StringPrintf(
      "%s/%s/leaked/LeakedSecurityObject/%08x", root_name_,
      MemoryDumpPartitionStatsDumper::kPartitionsDumpName, type_id);
  MemoryAllocatorDump* dump = memory_dump_->CreateAllocatorDump(dump_name);
  dump->AddScalar(base::trace_event::MemoryAllocatorDump::kNameSize,
                  base::trace_event::MemoryAllocatorDump::kUnitsBytes, size);
}

}  // namespace base::trace_event
