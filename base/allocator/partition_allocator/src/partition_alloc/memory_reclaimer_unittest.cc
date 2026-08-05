// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/memory_reclaimer.h"

#include <array>
#include <limits>
#include <memory>
#include <utility>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/partition_root_internal.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_base/test/gtest_util.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
#include "partition_alloc/extended_api.h"
#include "partition_alloc/internal/thread_cache_internal.h"
#endif

// Otherwise, PartitionAlloc doesn't allocate any memory, and the tests are
// meaningless.
#if !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc {

namespace {

void HandleOOM(size_t unused_size) {
  PA_LOG(FATAL) << "Out of memory";
}

}  // namespace

class MemoryReclaimerTest : public ::testing::Test {
 public:
  MemoryReclaimerTest() {
    // Since MemoryReclaimer::ResetForTesting() clears partitions_,
    // we need to make PartitionAllocator after this ResetForTesting().
    // Otherwise, we will see no PartitionAllocator is registered.
    MemoryReclaimer::Instance()->ResetForTesting();

    PartitionOptions opts;
    allocator_ = std::make_unique<PartitionAllocatorForTesting>(opts);
    allocator_->root()->UncapEmptySlotSpanMemoryForTesting();
    PartitionAllocGlobalInit(HandleOOM);
  }

  ~MemoryReclaimerTest() override {
    // Since MemoryReclaimer::UnregisterPartition() checks whether
    // the given partition is managed by MemoryReclaimer, need to
    // destruct |allocator_| before ResetForTesting().
    allocator_ = nullptr;
    PartitionAllocGlobalUninitForTesting();
  }

  void Reclaim() { MemoryReclaimer::Instance()->ReclaimForTesting(); }

  void AllocateAndFree() {
    void* data = allocator_->root()->Alloc(1);
    allocator_->root()->Free(data);
  }

  // The back-off policy internals are private. This fixture is a friend of
  // MemoryReclaimer, but the TEST_F bodies below run in a derived class that is
  // not, so expose what they need here.
  using Config = MemoryReclaimer::AdaptiveIntervalConfig;

  static internal::base::TimeDelta ComputeNextInterval(
      const Config& config,
      internal::base::TimeDelta current,
      size_t total_decommitted_bytes) {
    return MemoryReclaimer::ComputeNextReclaimInterval(config, current,
                                                       total_decommitted_bytes);
  }

  // The properties the embedder relies on have to hold for any usable
  // configuration, so the tests below check them against the defaults and
  // against a deliberately different set of bounds.
  static std::array<Config, 2> TestConfigs() {
    Config defaults;
    defaults.enabled = true;

    Config custom;
    custom.enabled = true;
    custom.min_interval = internal::base::Seconds(1);
    custom.max_interval = internal::base::Seconds(10);
    custom.default_interval = internal::base::Seconds(3);
    custom.min_decommittable_bytes = 1024;

    return {defaults, custom};
  }

  std::unique_ptr<PartitionAllocatorForTesting> allocator_;
};

TEST_F(MemoryReclaimerTest, FreesMemory) {
  PartitionRoot* root = allocator_->root();

  size_t committed_initially = root->get_total_size_of_committed_pages();
  AllocateAndFree();
  size_t committed_before = root->get_total_size_of_committed_pages();

  EXPECT_GT(committed_before, committed_initially);

  Reclaim();
  size_t committed_after = root->get_total_size_of_committed_pages();
  EXPECT_LT(committed_after, committed_before);
  EXPECT_LE(committed_initially, committed_after);
}

TEST_F(MemoryReclaimerTest, Reclaim) {
  PartitionRoot* root = allocator_->root();
  size_t committed_initially = root->get_total_size_of_committed_pages();

  {
    AllocateAndFree();

    size_t committed_before = root->get_total_size_of_committed_pages();
    EXPECT_GT(committed_before, committed_initially);
    MemoryReclaimer::Instance()->ReclaimAll();
    size_t committed_after = root->get_total_size_of_committed_pages();

    EXPECT_LT(committed_after, committed_before);
    EXPECT_LE(committed_initially, committed_after);
  }
}

// Tests for the adaptive reclaim-interval policy. The interval shrinks toward
// config.min_interval when there is a lot of decommittable memory and grows
// toward config.max_interval when there is little, with a stable hysteresis
// band in between. Every result is clamped to [min_interval, max_interval].
//
// These check the properties the embedder relies on rather than the exact
// arithmetic, so that the policy can be retuned without rewriting them.

TEST_F(MemoryReclaimerTest, AdaptiveIntervalIsInertWhenDisabled) {
  const int64_t fixed_interval =
      MemoryReclaimer::Instance()
          ->GetRecommendedReclaimIntervalInMicroseconds();
  EXPECT_GT(fixed_interval, 0);

  // A configuration that is not enabled must leave the embedder's cadence
  // alone, however different its own bounds are.
  Config config;
  config.default_interval = internal::base::Seconds(30);
  MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config);

  EXPECT_EQ(MemoryReclaimer::Instance()
                ->GetRecommendedReclaimIntervalInMicroseconds(),
            fixed_interval);
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalStartsAtDefault) {
  Config config;
  config.enabled = true;
  config.default_interval = internal::base::Seconds(30);
  MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config);

  EXPECT_EQ(MemoryReclaimer::Instance()
                ->GetRecommendedReclaimIntervalInMicroseconds(),
            config.default_interval.InMicroseconds());
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalConfigIsAppliedAsGiven) {
  Config config;
  config.enabled = true;
  config.min_interval = internal::base::Seconds(2);
  config.max_interval = internal::base::Minutes(5);
  config.default_interval = internal::base::Seconds(45);
  config.min_decommittable_bytes = 512 * 1024;
  MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config);

  const Config applied = MemoryReclaimer::Instance()
                             ->GetSanitizedAdaptiveIntervalConfigForTesting();
  EXPECT_TRUE(applied.enabled);
  EXPECT_EQ(applied.min_interval, config.min_interval);
  EXPECT_EQ(applied.max_interval, config.max_interval);
  EXPECT_EQ(applied.default_interval, config.default_interval);
  EXPECT_EQ(applied.min_decommittable_bytes, config.min_decommittable_bytes);
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalConfigRejectsInvalidValues) {
  // A configuration the state machine cannot run on is a bug in whatever
  // produced it. Release builds repair it, but development builds must not
  // paper over it.
  {
    Config config;
    config.min_interval = internal::base::Seconds(-1);
    PA_EXPECT_DCHECK_DEATH(
        MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config));
  }
  {
    Config config;
    config.max_interval = config.min_interval / 2;
    PA_EXPECT_DCHECK_DEATH(
        MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config));
  }
  {
    Config config;
    config.default_interval = config.max_interval * 2;
    PA_EXPECT_DCHECK_DEATH(
        MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config));
  }
  {
    Config config;
    config.min_decommittable_bytes = 0;
    PA_EXPECT_DCHECK_DEATH(
        MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config));
  }
}

// Reclaim() itself must feed back what it actually decommitted, not just carry
// the configured default around.

TEST_F(MemoryReclaimerTest, ReclaimShortensIntervalWhenPartitionsHoldMemory) {
  Config config;
  config.enabled = true;
  config.min_interval = internal::base::Seconds(1);
  config.max_interval = internal::base::Seconds(64);
  config.default_interval = internal::base::Seconds(8);
  // Make whatever the test partition decommits count as "a lot to reclaim".
  config.min_decommittable_bytes = 1;
  MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config);

  AllocateAndFree();
  Reclaim();

  const int64_t interval = MemoryReclaimer::Instance()
                               ->GetRecommendedReclaimIntervalInMicroseconds();
  EXPECT_LT(interval, config.default_interval.InMicroseconds());
  EXPECT_GE(interval, config.min_interval.InMicroseconds());
}

TEST_F(MemoryReclaimerTest, ReclaimLengthensIntervalWhenLittleToReclaim) {
  Config config;
  config.enabled = true;
  config.min_interval = internal::base::Seconds(1);
  config.max_interval = internal::base::Seconds(64);
  config.default_interval = internal::base::Seconds(8);
  // A watermark no partition can reach, so there is never "a lot to reclaim".
  config.min_decommittable_bytes = std::numeric_limits<size_t>::max() / 16;
  MemoryReclaimer::Instance()->SetAdaptiveIntervalConfig(config);

  AllocateAndFree();
  Reclaim();
  const int64_t interval = MemoryReclaimer::Instance()
                               ->GetRecommendedReclaimIntervalInMicroseconds();
  EXPECT_GT(interval, config.default_interval.InMicroseconds());
  EXPECT_LE(interval, config.max_interval.InMicroseconds());
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalGrowsToMaxWhenLittleToReclaim) {
  for (const Config& config : TestConfigs()) {
    const size_t little_to_reclaim = config.min_decommittable_bytes - 1;
    internal::base::TimeDelta interval = config.default_interval;
    internal::base::TimeDelta previous;
    do {
      previous = interval;
      interval = ComputeNextInterval(config, interval, little_to_reclaim);
      EXPECT_GE(interval, previous);
      EXPECT_LE(interval, config.max_interval);
    } while (interval != previous);
    EXPECT_EQ(interval, config.max_interval);
  }
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalShrinksToMinWhenMuchToReclaim) {
  for (const Config& config : TestConfigs()) {
    const size_t much_to_reclaim = 10 * config.min_decommittable_bytes + 1;
    internal::base::TimeDelta interval = config.max_interval;
    internal::base::TimeDelta previous;
    do {
      previous = interval;
      interval = ComputeNextInterval(config, interval, much_to_reclaim);
      EXPECT_LE(interval, previous);
      EXPECT_GE(interval, config.min_interval);
    } while (interval != previous);
    EXPECT_EQ(interval, config.min_interval);
  }
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalIsStableAroundTheWatermark) {
  // Around the watermark the interval is left alone, so that a workload
  // hovering there doesn't make the cadence oscillate.
  for (const Config& config : TestConfigs()) {
    for (size_t bytes :
         {config.min_decommittable_bytes, 2 * config.min_decommittable_bytes}) {
      for (internal::base::TimeDelta interval :
           {config.min_interval, config.default_interval,
            config.max_interval}) {
        EXPECT_EQ(ComputeNextInterval(config, interval, bytes), interval);
      }
    }
  }
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalNeverGrowsWithMoreToReclaim) {
  // The more there is to reclaim, the sooner the next reclaim must happen.
  for (const Config& config : TestConfigs()) {
    for (internal::base::TimeDelta interval :
         {config.min_interval, config.default_interval, config.max_interval}) {
      internal::base::TimeDelta previous =
          ComputeNextInterval(config, interval, /*total_decommitted_bytes=*/0);
      for (size_t multiplier = 1; multiplier <= 20; ++multiplier) {
        const internal::base::TimeDelta next = ComputeNextInterval(
            config, interval, multiplier * config.min_decommittable_bytes);
        EXPECT_LE(next, previous);
        previous = next;
      }
    }
  }
}

TEST_F(MemoryReclaimerTest, AdaptiveIntervalAlwaysStaysWithinBounds) {
  // Whatever it is fed, including an out-of-range current interval, the policy
  // hands the embedder a delay it is willing to wake up on.
  for (const Config& config : TestConfigs()) {
    for (size_t multiplier = 0; multiplier <= 20; ++multiplier) {
      for (internal::base::TimeDelta interval :
           {config.min_interval / 2, config.min_interval,
            config.default_interval, config.max_interval,
            config.max_interval * 2}) {
        const internal::base::TimeDelta next = ComputeNextInterval(
            config, interval, multiplier * config.min_decommittable_bytes);
        EXPECT_GE(next, config.min_interval);
        EXPECT_LE(next, config.max_interval);
      }
    }
  }
}

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)

namespace {
// malloc() / free() pairs can be removed by the compiler, this is enough (for
// now) to prevent that.
PA_NOINLINE void FreeForTest(void* data) {
  free(data);
}
}  // namespace

TEST_F(MemoryReclaimerTest, DoNotAlwaysPurgeThreadCache) {
  // Make sure the thread cache is enabled in the main partition.
  auto* root = allocator_shim::internal::PartitionAllocMalloc::Allocator();
  internal::ThreadCacheProcessScopeForTesting scope(root);

  for (size_t i = 0; i < internal::ThreadCache::kDefaultSizeThreshold; i++) {
    void* data = malloc(i);
    FreeForTest(data);
  }

  auto* tcache = root->thread_cache_for_testing();
  ASSERT_TRUE(tcache);
  // ThreadCache must not be tomestone. If so, tcache->CacheMemory() will
  // cause memory access violation.
  ASSERT_TRUE(!internal::ThreadCache::IsTombstone());
  size_t cached_size = tcache->CachedMemory();

  Reclaim();

  // No thread cache purging during periodic purge, but with ReclaimAll().
  //
  // Cannot assert on the exact size of the thread cache, since it can shrink
  // when a buffer is overfull, and this may happen through other malloc()
  // allocations in the test harness.
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  Reclaim();
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  MemoryReclaimer::Instance()->ReclaimAll();
  EXPECT_LT(tcache->CachedMemory(), cached_size / 2);
}

#endif  // PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
        // PA_CONFIG(THREAD_CACHE_SUPPORTED)

}  // namespace partition_alloc

#endif  // !PA_BUILDFLAG(MEMORY_TOOL_REPLACES_ALLOCATOR)
