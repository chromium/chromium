// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_STATS_H_
#define BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_STATS_H_

#include <cstddef>
#include <cstdint>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/allocator/partition_allocator/partition_alloc_constants.h"
#include "base/base_export.h"

namespace base {

// Most of these are not populated if PA_ENABLE_THREAD_CACHE_STATISTICS is not
// defined.
struct ThreadCacheStats {
  uint64_t alloc_count;   // Total allocation requests.
  uint64_t alloc_hits;    // Thread cache hits.
  uint64_t alloc_misses;  // Thread cache misses.

  // Allocation failure details:
  uint64_t alloc_miss_empty;
  uint64_t alloc_miss_too_large;

  // Cache fill details:
  uint64_t cache_fill_count;
  uint64_t cache_fill_hits;
  uint64_t cache_fill_misses;  // Object too large.

  uint64_t batch_fill_count;  // Number of central allocator requests.

  // Memory cost:
  uint64_t bucket_total_memory;
  uint64_t metadata_overhead;

#if defined(PA_THREAD_CACHE_ALLOC_STATS)
  uint64_t allocs_per_bucket_[kNumBuckets + 1];
#endif  // defined(PA_THREAD_CACHE_ALLOC_STATS)
};

// Struct used to retrieve total memory usage of a partition. Used by
// PartitionStatsDumper implementation.
struct PartitionMemoryStats {
  size_t total_mmapped_bytes;    // Total bytes mmap()-ed from the system.
  size_t total_committed_bytes;  // Total size of committed pages.
  size_t max_committed_bytes;    // Max size of committed pages.
  size_t total_allocated_bytes;  // Total size of allcoations.
  size_t max_allocated_bytes;    // Max size of allocations.
  size_t total_resident_bytes;   // Total bytes provisioned by the partition.
  size_t total_active_bytes;     // Total active bytes in the partition.
  size_t total_decommittable_bytes;  // Total bytes that could be decommitted.
  size_t total_discardable_bytes;    // Total bytes that could be discarded.

  bool has_thread_cache;
  ThreadCacheStats current_thread_cache_stats;
  ThreadCacheStats all_thread_caches_stats;
};

// Struct used to retrieve memory statistics about a partition bucket. Used by
// PartitionStatsDumper implementation.
struct PartitionBucketMemoryStats {
  bool is_valid;       // Used to check if the stats is valid.
  bool is_direct_map;  // True if this is a direct mapping; size will not be
                       // unique.
  uint32_t bucket_slot_size;          // The size of the slot in bytes.
  uint32_t allocated_slot_span_size;  // Total size the slot span allocated
                                      // from the system (committed pages).
  uint32_t active_bytes;              // Total active bytes used in the bucket.
  uint32_t resident_bytes;            // Total bytes provisioned in the bucket.
  uint32_t decommittable_bytes;       // Total bytes that could be decommitted.
  uint32_t discardable_bytes;         // Total bytes that could be discarded.
  uint32_t num_full_slot_spans;       // Number of slot spans with all slots
                                      // allocated.
  uint32_t num_active_slot_spans;     // Number of slot spans that have at least
                                      // one provisioned slot.
  uint32_t num_empty_slot_spans;      // Number of slot spans that are empty
                                      // but not decommitted.
  uint32_t num_decommitted_slot_spans;  // Number of slot spans that are empty
                                        // and decommitted.
};

// Interface that is passed to PartitionDumpStats and
// PartitionDumpStats for using the memory statistics.
class BASE_EXPORT PartitionStatsDumper {
 public:
  // Called to dump total memory used by partition, once per partition.
  virtual void PartitionDumpTotals(const char* partition_name,
                                   const PartitionMemoryStats*) = 0;

  // Called to dump stats about buckets, for each bucket.
  virtual void PartitionsDumpBucketStats(const char* partition_name,
                                         const PartitionBucketMemoryStats*) = 0;
};

// Simple version of PartitionStatsDumper, storing the returned stats in stats_.
// Does not handle per-bucket stats.
class BASE_EXPORT SimplePartitionStatsDumper : public PartitionStatsDumper {
 public:
  SimplePartitionStatsDumper();

  void PartitionDumpTotals(const char* partition_name,
                           const PartitionMemoryStats* memory_stats) override;

  void PartitionsDumpBucketStats(const char* partition_name,
                                 const PartitionBucketMemoryStats*) override {}

  const PartitionMemoryStats& stats() const { return stats_; }

 private:
  PartitionMemoryStats stats_;
};

}  // namespace base

#endif  // BASE_ALLOCATOR_PARTITION_ALLOCATOR_PARTITION_STATS_H_
