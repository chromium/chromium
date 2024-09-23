// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/memory_reclaimer.h"

#include <memory>
#include <utility>

#include "partition_alloc/build_config.h"
#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_base/compiler_specific.h"
#include "partition_alloc/partition_alloc_base/logging.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/shim/allocator_shim_default_dispatch_to_partition_alloc.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    PA_CONFIG(THREAD_CACHE_SUPPORTED)
#include "partition_alloc/extended_api.h"
#include "partition_alloc/thread_cache.h"
#endif

// Otherwise, PartitionAlloc doesn't allocate any memory, and the tests are
// meaningless.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

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
    opts.star_scan_quarantine = PartitionOptions::kAllowed;
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

  void Reclaim() { MemoryReclaimer::Instance()->ReclaimNormal(); }

  void AllocateAndFree() {
    void* data = allocator_->root()->Alloc(1);
    allocator_->root()->Free(data);
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
  internal::ThreadCacheProcessScopeForTesting scope(
      allocator_shim::internal::PartitionAllocMalloc::Allocator());

  for (size_t i = 0; i < ThreadCache::kDefaultSizeThreshold; i++) {
    void* data = malloc(i);
    FreeForTest(data);
  }

  auto* tcache = ThreadCache::Get();
  ASSERT_TRUE(tcache);
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

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
