// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// Micro-benchmarks for slot provisioning on the allocation slow path: when a
// slot span runs out of provisioned slots, more slots are provisioned (and on
// platforms with lazy commit, e.g. Windows, their memory is committed with a
// system call made under the partition lock).
//
// All variants of a scenario run in an interleaved (round-robin) way within the
// same process to minimize the impact of machine noise. The median is
// reported.

#include <algorithm>
#include <array>
#include <cstddef>
#include <cstdint>
#include <string>
#include <vector>

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/notreached.h"
#include "partition_alloc/partition_alloc_base/rand_util.h"
#include "partition_alloc/partition_alloc_base/time/time.h"
#include "partition_alloc/partition_alloc_check.h"
#include "partition_alloc/partition_alloc_constants.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_root.h"
#include "partition_alloc/partition_stats.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "testing/perf/perf_result_reporter.h"

namespace partition_alloc::internal {

namespace {

constexpr char kMetricPrefix[] = "SlotProvisioning.";
constexpr int kRounds = 9;

struct Variant {
  const char* name;
  size_t min_provisioning_size;
};

constexpr auto kVariants = std::to_array<Variant>({
    {"default", 0},
    {"min_provisioning_16k", 16 * 1024},
    {"min_provisioning_32k", 32 * 1024},
});

// Sets the min provisioning size for the lifetime of the object.
class ScopedMinProvisioningSize {
 public:
  explicit ScopedMinProvisioningSize(size_t size)
      : previous_size_(PartitionRoot::GetMinProvisioningSize()) {
    PartitionRoot::SetMinProvisioningSize(size);
  }
  ~ScopedMinProvisioningSize() {
    PartitionRoot::SetMinProvisioningSize(previous_size_);
  }

 private:
  const size_t previous_size_;
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
size_t RandomSize(base::InsecureRandomGenerator& rng) {
  struct Entry {
    uint32_t size;
    uint32_t weight;
  };
  static constexpr Entry kDistribution[] = {
      {16, 10}, {32, 9},  {48, 16}, {64, 23},  {80, 7},
      {96, 5},  {112, 5}, {128, 5}, {160, 6},  {224, 3},
      {320, 5}, {448, 2}, {768, 2}, {2048, 1}, {10240, 1},
  };
  uint32_t r = rng.RandUint32() % 100;
  for (const Entry& entry : kDistribution) {
    if (r < entry.weight) {
      return entry.size;
    }
    r -= entry.weight;
  }
  PA_NOTREACHED();
}

double Median(std::vector<double> samples) {
  PA_CHECK(!samples.empty());
  std::sort(samples.begin(), samples.end());
  return samples[samples.size() / 2];
}

constexpr PartitionOptions kOptions = [] {
  PartitionOptions options;
  // No thread cache: every allocation goes to the central allocator, which
  // makes the provisioning cost more visible, and the results more stable.
  options.thread_cache = PartitionOptions::kDisabled;
  return options;
}();

struct ScenarioParams {
  const char* name;
  // 0 means "mixed sizes".
  size_t size;
};

// Allocates 16 MiB worth of objects from slot spans that need to be
// provisioned, like a task building a large data structure in a fresh
// partition. Returns ns per allocation.
double RunFreshAllocations(PartitionRoot& root,
                           const std::vector<size_t>& sizes) {
  std::vector<void*> objects(sizes.size());

  const base::TimeTicks start = base::TimeTicks::Now();
  for (size_t i = 0; i < sizes.size(); ++i) {
    objects[i] = root.Alloc<AllocFlags::kNoHooks>(sizes[i]);
    // Initialize the object, like a constructor would. This also accounts for
    // page faults on freshly committed memory, wherever they happen.
    *static_cast<volatile uintptr_t*>(objects[i]) = 42;
  }
  const base::TimeTicks end = base::TimeTicks::Now();

  for (void* object : objects) {
    root.Free<FreeFlags::kNoHooks>(object);
  }
  // Decommit everything, so that the next run has to provision (and commit)
  // slots again.
  root.PurgeMemory(PurgeFlags::kDecommitEmptySlotSpans |
                   PurgeFlags::kDiscardUnusedSystemPages);

  return static_cast<double>((end - start).InNanoseconds()) /
         static_cast<double>(sizes.size());
}

TEST(SlotProvisioningPerfTest, FreshAllocations) {
  constexpr size_t kTotalSize = 16 << 20;
  constexpr ScenarioParams kScenarios[] = {
      {"fresh_alloc_mixed", 0},   {"fresh_alloc_32", 32},
      {"fresh_alloc_128", 128},   {"fresh_alloc_1024", 1024},
      {"fresh_alloc_4096", 4096},
  };

  for (const ScenarioParams& scenario : kScenarios) {
    base::InsecureRandomGenerator rng =
        base::InsecureRandomGenerator::ConstructForTesting();
    rng.ReseedForTesting(42);
    std::vector<size_t> sizes;
    for (size_t total = 0; total < kTotalSize;) {
      sizes.push_back(scenario.size ? scenario.size : RandomSize(rng));
      total += sizes.back();
    }

    PartitionAllocatorForTesting<AllowLeaks> allocator(kOptions);
    PartitionRoot& root = *allocator.root();
    std::vector<std::vector<double>> samples(std::size(kVariants));
    // The first round is a warm-up (e.g. reserves super pages).
    for (int round = 0; round < kRounds + 1; ++round) {
      for (size_t i = 0; i < std::size(kVariants); ++i) {
        ScopedMinProvisioningSize scoped_min_provisioning_size(
            kVariants[i].min_provisioning_size);
        const double sample = RunFreshAllocations(root, sizes);
        if (round > 0) {
          samples[i].push_back(sample);
        }
      }
    }

    for (size_t i = 0; i < std::size(kVariants); ++i) {
      perf_test::PerfResultReporter reporter(kMetricPrefix, kVariants[i].name);
      reporter.RegisterImportantMetric(scenario.name, "ns");
      reporter.AddResult(scenario.name, Median(samples[i]));
    }
  }
}

// Returns the memory committed for slots of `root`, in bytes.
size_t CommittedBytes(PartitionRoot& root) {
  SimplePartitionStatsDumper dumper;
  root.DumpStats("", /*is_light_dump=*/true,
                 /*populate_discardable_bytes=*/false, &dumper);
  return dumper.stats().total_committed_bytes;
}

// Memory overhead of provisioning more slots at once: allocates a single
// object of each small bucket size, and reports the committed memory.
TEST(SlotProvisioningPerfTest, OneObjectPerBucket) {
  for (const Variant& variant : kVariants) {
    ScopedMinProvisioningSize scoped_min_provisioning_size(
        variant.min_provisioning_size);
    PartitionAllocatorForTesting<AllowLeaks> allocator(kOptions);
    PartitionRoot& root = *allocator.root();
    const size_t committed_before = CommittedBytes(root);

    std::vector<void*> objects;
    for (size_t size = kAlignment; size <= 16 * 1024; size += kAlignment) {
      objects.push_back(root.Alloc<AllocFlags::kNoHooks>(size));
    }
    const size_t committed_after = CommittedBytes(root);
    for (void* object : objects) {
      root.Free<FreeFlags::kNoHooks>(object);
    }

    perf_test::PerfResultReporter reporter(kMetricPrefix, variant.name);
    reporter.RegisterImportantMetric("one_object_per_bucket_committed", "KiB");
    reporter.AddResult(
        "one_object_per_bucket_committed",
        static_cast<double>((committed_after - committed_before) / 1024));
  }
}

}  // namespace

}  // namespace partition_alloc::internal
