// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Micro-benchmarks for the thread-bound Scheduler-Loop Quarantine (SLQ), which
// delays frees until the end of the current task.
//
// Each scenario is measured with these configurations:
// - `no_quarantine`: SLQ disabled (baseline).
// - `browser_main`: SLQ configured like on the browser main thread on desktop.
// - `browser_main_unlimited`: `browser_main` with an unlimited capacity.
// - `browser_main_purge_headroom_10`: `browser_main` with a 10% purge headroom.
// To reduce the impact of machine noise, configurations run alternately in the
// same process, `kRounds` times each, and the median is reported.
//
// Scenarios:
// - FreeStorm: A task frees many objects (e.g. destroys a large `base::Value`
//   tree). Reports the time per free, including the end-of-task purge.
// - SteadyChurn: A long task repeatedly frees an object and allocates a new
//   one, with a full quarantine (unless its capacity is unlimited). Reports the
//   time per (free + alloc).
// Each scenario has two variants, which bound the work of destructors before
// objects are freed: the default reads only the first cache line of each
// object, the `_all_lines` variant reads all its cache lines. This matters
// because zapping writes the whole object: it misses the CPU cache on lines
// that weren't read, and is cheaper on lines that were.

#include "partition_alloc/scheduler_loop_quarantine.h"

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <initializer_list>
#include <limits>
#include <string>
#include <utility>
#include <vector>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/extended_api.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_alloc_public_constants.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/scheduler_loop_quarantine_support.h"
#include "partition_alloc/thread_cache.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace partition_alloc::internal {

namespace {

// -----------------------------------------------------------------------------
// Configurations
// -----------------------------------------------------------------------------

constexpr SchedulerLoopQuarantineConfig BrowserMainConfig() {
  SchedulerLoopQuarantineConfig config;
  config.enable_quarantine = true;
  config.enable_zapping = true;
  config.enable_task_controlled_purge = true;
  config.branch_capacity_in_bytes = 512 * 1024;
  return config;
}

// Candidate configuration with an unlimited capacity, which is only purged at
// the end of each task (Finch group "browser_enabled_unlimited").
constexpr SchedulerLoopQuarantineConfig BrowserMainUnlimitedConfig() {
  SchedulerLoopQuarantineConfig config = BrowserMainConfig();
  config.pause_in_between_tasks = true;
  config.branch_capacity_in_bytes = std::numeric_limits<size_t>::max();
  return config;
}

constexpr SchedulerLoopQuarantineConfig BrowserMainPurgeHeadroomConfig() {
  SchedulerLoopQuarantineConfig config = BrowserMainConfig();
  config.purge_headroom_percent = 10;
  return config;
}

struct Configuration {
  const char* name;
  SchedulerLoopQuarantineConfig config;
};

constexpr std::array<Configuration, 4> kConfigurations = {{
    {"no_quarantine", SchedulerLoopQuarantineConfig()},
    {"browser_main", BrowserMainConfig()},
    {"browser_main_unlimited", BrowserMainUnlimitedConfig()},
    {"browser_main_purge_headroom_10", BrowserMainPurgeHeadroomConfig()},
}};

constexpr int kRounds = 9;

// -----------------------------------------------------------------------------
// Allocation sizes
// -----------------------------------------------------------------------------

// Returns a fast pseudo-random number generator. All configurations use the
// same seeds, so that they get the same allocation sizes.
base::InsecureRandomGenerator CreateRandomGenerator(uint64_t seed) {
  base::InsecureRandomGenerator rng =
      base::InsecureRandomGenerator::ConstructForTesting();
  rng.ReseedForTesting(seed);
  return rng;
}

// Slot sizes and their relative frequency.
struct WeightedSlotSize {
  uint16_t slot_size;
  uint16_t weight;
};

// Distribution of malloc() slot sizes on the browser main thread on Windows,
// in percent of allocations. Measured by counting allocations per bucket
// (`ThreadCacheStats::allocs_per_bucket_`) during startup followed by the
// opening of 11 tabs. Up to 128 bytes, each entry is one bucket. Above, each
// entry represents a range of buckets (129-192, 193-256, 257-384, 385-512,
// 513-1024, 1025-4096 and 4097-32768 bytes), with the slot size of a bucket
// near the count-weighted mean of the range (e.g. {160, 6}: 6% of allocations
// have a slot of 129 to 192 bytes). Slots above 32 KiB (0.03% of allocations)
// are ignored.
//
// A slot holds the requested bytes plus per-allocation extras (e.g. 4 bytes of
// BackupRefPtr in-slot metadata on Windows), whose size depends on the platform
// and build. `TestPartition::Alloc()` subtracts the extras of the partition
// under test, so that allocations use the measured buckets on all platforms.
constexpr WeightedSlotSize kSlotSizeDistribution[] = {
    {16, 10}, {32, 9},  {48, 16}, {64, 23},  {80, 7},
    {96, 5},  {112, 5}, {128, 5}, {160, 6},  {224, 3},
    {320, 5}, {448, 2}, {768, 2}, {2048, 1}, {10240, 1},
};

constexpr size_t kTotalWeight = [] {
  size_t total = 0;
  for (const WeightedSlotSize& entry : kSlotSizeDistribution) {
    total += entry.weight;
  }
  return total;
}();

// Table in which each slot size of `kSlotSizeDistribution` appears `weight`
// times, to pick a random slot size in constant time.
constexpr auto kSlotSizeTable = [] {
  std::array<uint16_t, kTotalWeight> table{};
  size_t index = 0;
  for (const WeightedSlotSize& entry : kSlotSizeDistribution) {
    for (size_t i = 0; i < entry.weight; ++i) {
      table[index++] = entry.slot_size;
    }
  }
  return table;
}();

size_t RandomSlotSize(base::InsecureRandomGenerator& rng) {
  return kSlotSizeTable[rng.RandUint64() % kSlotSizeTable.size()];
}

// -----------------------------------------------------------------------------
// Partition under test
// -----------------------------------------------------------------------------

// Which cache lines of an object are read before it is freed, like a destructor
// would.
enum class ObjectReads {
  // Only the first cache line, e.g. for an object whose destructor only reads
  // a few fields at its start, or a buffer of trivially destructible elements.
  kFirstLine,
  // All cache lines, e.g. for an object whose fields have non-trivial
  // destructors, or an array of such objects.
  kAllLines,
};

const char* MetricSuffix(ObjectReads reads) {
  return reads == ObjectReads::kAllLines ? "_all_lines" : "";
}

// A partition with a thread cache, which owns the current thread's quarantine.
class TestPartition {
 public:
  explicit TestPartition(const SchedulerLoopQuarantineConfig& config)
      : thread_cache_scope_(allocator_.root()) {
    // The thread-bound quarantine lives in the thread cache.
    PA_CHECK(allocator_.root()->settings_.with_thread_cache);
    // Like the browser process malloc partition on desktop.
    allocator_.root()->SwitchToDenserBucketDistribution();
    allocator_.root()->ReconfigureSchedulerLoopQuarantineForCurrentThread(
        config);
  }

  ~TestPartition() {
    // Release quarantined and cached objects before destroying the partition.
    allocator_.root()->ReconfigureSchedulerLoopQuarantineForCurrentThread(
        SchedulerLoopQuarantineConfig());
    ::partition_alloc::ThreadCache::PurgeCurrentThread();
  }

  // Allocates an object that fills a slot of `slot_size` bytes together with
  // the per-allocation extras of this partition (e.g. in-slot BackupRefPtr
  // metadata). The object can hold at least a `uintptr_t`.
  void* Alloc(size_t slot_size) {
    const size_t extras_size = allocator_.root()->settings_.extras_size;
    const size_t size =
        std::max(slot_size, extras_size + sizeof(uintptr_t)) - extras_size;
    void* object = allocator_.root()->Alloc<AllocFlags::kNoHooks>(size);
    // Write to the object, like a constructor would. The object's size is
    // stored in its first word, for `Free()`.
    *static_cast<volatile uintptr_t*>(object) = (size << 8) | kObjectTag;
    return object;
  }

  // Reads `object` like a destructor would (see `ObjectReads`), then frees it
  // through the quarantine, if enabled.
  void Free(void* object, ObjectReads reads) {
    const uintptr_t first_word = *static_cast<volatile uintptr_t*>(object);
    PA_CHECK((first_word & 0xff) == kObjectTag);
    if (reads == ObjectReads::kAllLines) {
      const size_t size = first_word >> 8;
      const uintptr_t address = reinterpret_cast<uintptr_t>(object);
      for (size_t offset = kPartitionCachelineSize; offset < size;
           offset += kPartitionCachelineSize) {
        (void)*reinterpret_cast<volatile uint8_t*>(address + offset);
      }
    }
    PartitionRoot::FreeInUnknownRoot<FreeFlags::kNoHooks |
                                     FreeFlags::kSchedulerLoopQuarantine>(
        object);
  }

 private:
  static constexpr uintptr_t kObjectTag = 42;

  static constexpr PartitionOptions kOptions = [] {
    PartitionOptions options;
    // With PartitionAlloc-Everywhere, the malloc partition owns the thread
    // cache: `thread_cache_scope_` moves it to this partition instead.
#if !PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC)
    options.thread_cache = PartitionOptions::kEnabled;
#endif
#if PA_BUILDFLAG(ENABLE_BACKUP_REF_PTR_SUPPORT)
    options.backup_ref_ptr = PartitionOptions::kEnabled;
#endif
    return options;
  }();

  PartitionAllocatorForTesting<AllowLeaks> allocator_{kOptions};
  ThreadCacheProcessScopeForTesting thread_cache_scope_;
};

// -----------------------------------------------------------------------------
// Measurement helpers
// -----------------------------------------------------------------------------

// Evicts CPU caches by touching a buffer larger than the last level cache.
class CacheFlusher {
 public:
  CacheFlusher() : buffer_(64 << 20, 1) {}

  void Flush() {
    for (size_t i = 0; i < buffer_.size(); i += kPartitionCachelineSize) {
      // Read-modify-write, to also evict lines held in shared state.
      buffer_[i] = static_cast<uint8_t>(buffer_[i] + 1);
    }
  }

 private:
  std::vector<uint8_t> buffer_;
};

double Median(std::vector<double> samples) {
  PA_CHECK(!samples.empty());
  auto middle = samples.begin() + samples.size() / 2;
  std::nth_element(samples.begin(), middle, samples.end());
  return *middle;
}

double NanosecondsPerOperation(base::TimeDelta elapsed, size_t operations) {
  return static_cast<double>(elapsed.InNanoseconds()) /
         static_cast<double>(operations);
}

// Runs `scenario(config, seed)` `kRounds` times for each configuration and
// reports the median result of each configuration, in ns, as `metric`.
template <typename Scenario>
void MeasureAndReport(const std::string& metric, Scenario scenario) {
  std::array<std::vector<double>, kConfigurations.size()> results;
  for (int round = 0; round < kRounds; ++round) {
    // All configurations use the same seed, so they get the same sizes.
    const uint64_t seed = static_cast<uint64_t>(round) + 1;
    for (size_t i = 0; i < kConfigurations.size(); ++i) {
      results[i].push_back(scenario(kConfigurations[i].config, seed));
    }
  }
  for (size_t i = 0; i < kConfigurations.size(); ++i) {
    perf_test::PerfResultReporter reporter("SchedulerLoopQuarantine.",
                                           kConfigurations[i].name);
    reporter.RegisterImportantMetric(metric, "ns");
    reporter.AddResult(metric, Median(std::move(results[i])));
  }
}

// -----------------------------------------------------------------------------
// Scenarios
// -----------------------------------------------------------------------------

struct FreeStormParams {
  const char* name;
  size_t num_objects;
  // Whether CPU caches are flushed before each storm, i.e. whether the freed
  // objects are cold.
  bool cold;
  // Whether objects are freed in a random order (vs. allocation order).
  bool random_order;
  int storms_per_round;
};

// Simulates tasks that free `params.num_objects` objects. Returns the median
// time per free, including the end-of-task purge, in ns.
double RunFreeStorms(const FreeStormParams& params,
                     ObjectReads reads,
                     CacheFlusher& cache_flusher,
                     const SchedulerLoopQuarantineConfig& config,
                     uint64_t seed) {
  TestPartition partition(config);
  base::InsecureRandomGenerator rng = CreateRandomGenerator(seed);
  std::vector<void*> objects(params.num_objects);

  // The first storm provisions slot spans and grows the quarantine's storage.
  // It isn't measured, so that all measured storms start from the state left
  // by a previous storm.
  constexpr int kWarmupStorms = 1;
  std::vector<double> results;
  for (int storm = 0; storm < kWarmupStorms + params.storms_per_round;
       ++storm) {
    for (void*& object : objects) {
      object = partition.Alloc(RandomSlotSize(rng));
    }
    if (params.random_order) {
      for (size_t i = objects.size() - 1; i > 0; --i) {
        std::swap(objects[i], objects[rng.RandUint64() % (i + 1)]);
      }
    }
    if (params.cold) {
      cache_flusher.Flush();
    }

    const base::TimeTicks start = base::TimeTicks::Now();
    {
      ScopedSchedulerLoopQuarantineTaskScope task;
      for (void* object : objects) {
        partition.Free(object, reads);
      }
    }  // The quarantine is purged at the end of the task.
    const base::TimeDelta elapsed = base::TimeTicks::Now() - start;

    if (storm >= kWarmupStorms) {
      results.push_back(NanosecondsPerOperation(elapsed, objects.size()));
    }
  }
  return Median(std::move(results));
}

// Simulates a long task with a ring of live objects, in which each iteration
// frees the oldest object and allocates a new one. Returns the time per
// (free + alloc), in ns.
double RunSteadyChurn(ObjectReads reads,
                      const SchedulerLoopQuarantineConfig& config,
                      uint64_t seed) {
  // Power of 2, for a fast modulo.
  constexpr size_t kLiveObjects = 4096;
  // Enough to fill a 512 KiB quarantine before measuring.
  constexpr size_t kWarmupIterations = 1 << 18;
  constexpr size_t kIterations = 1 << 20;

  TestPartition partition(config);
  base::InsecureRandomGenerator rng = CreateRandomGenerator(seed);
  std::vector<void*> ring(kLiveObjects);
  for (void*& object : ring) {
    object = partition.Alloc(RandomSlotSize(rng));
  }

  auto iterate = [&](size_t i) {
    void*& object = ring[i % kLiveObjects];
    partition.Free(object, reads);
    object = partition.Alloc(RandomSlotSize(rng));
  };

  base::TimeDelta elapsed;
  {
    ScopedSchedulerLoopQuarantineTaskScope task;
    for (size_t i = 0; i < kWarmupIterations; ++i) {
      iterate(i);
    }
    const base::TimeTicks start = base::TimeTicks::Now();
    for (size_t i = 0; i < kIterations; ++i) {
      iterate(i);
    }
    elapsed = base::TimeTicks::Now() - start;
  }

  for (void* object : ring) {
    partition.Free(object, reads);
  }
  return NanosecondsPerOperation(elapsed, kIterations);
}

// -----------------------------------------------------------------------------
// Tests
// -----------------------------------------------------------------------------

TEST(SchedulerLoopQuarantinePerfTest, FreeStorm) {
  constexpr FreeStormParams kStorms[] = {
      {"storm_1k_cold", 1000, /*cold=*/true, /*random_order=*/false, 30},
      {"storm_8k_cold", 8000, /*cold=*/true, /*random_order=*/false, 20},
      {"storm_8k_cold_random_order", 8000, /*cold=*/true,
       /*random_order=*/true, 20},
      {"storm_50k", 50000, /*cold=*/false, /*random_order=*/false, 10},
      {"storm_50k_random_order", 50000, /*cold=*/false, /*random_order=*/true,
       10},
      {"storm_300k", 300000, /*cold=*/false, /*random_order=*/false, 4},
  };
  CacheFlusher cache_flusher;
  for (const FreeStormParams& params : kStorms) {
    for (ObjectReads reads :
         {ObjectReads::kFirstLine, ObjectReads::kAllLines}) {
      MeasureAndReport(
          std::string(params.name) + MetricSuffix(reads),
          [&](const SchedulerLoopQuarantineConfig& config, uint64_t seed) {
            return RunFreeStorms(params, reads, cache_flusher, config, seed);
          });
    }
  }
}

TEST(SchedulerLoopQuarantinePerfTest, SteadyChurn) {
  for (ObjectReads reads : {ObjectReads::kFirstLine, ObjectReads::kAllLines}) {
    MeasureAndReport(
        std::string("steady_churn") + MetricSuffix(reads),
        [&](const SchedulerLoopQuarantineConfig& config, uint64_t seed) {
          return RunSteadyChurn(reads, config, seed);
        });
  }
}

}  // namespace

}  // namespace partition_alloc::internal
