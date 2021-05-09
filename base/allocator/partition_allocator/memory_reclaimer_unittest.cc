// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/memory_reclaimer.h"

#include <memory>
#include <utility>

#include "base/allocator/buildflags.h"
#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED)
#include "base/allocator/partition_allocator/thread_cache.h"
#endif

// Otherwise, PartitionAlloc doesn't allocate any memory, and the tests are
// meaningless.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace base {

namespace {

void HandleOOM(size_t unused_size) {
  LOG(FATAL) << "Out of memory";
}

}  // namespace

class PartitionAllocMemoryReclaimerTest : public ::testing::Test {
 public:
  PartitionAllocMemoryReclaimerTest()
      : ::testing::Test(),
        task_environment_(test::TaskEnvironment::TimeSource::MOCK_TIME),
        allocator_() {}

 protected:
  void SetUp() override {
    PartitionAllocGlobalInit(HandleOOM);
    PartitionAllocMemoryReclaimer::Instance()->ResetForTesting();
    allocator_ = std::make_unique<PartitionAllocator>();
    allocator_->init({PartitionOptions::AlignedAlloc::kDisallowed,
                      PartitionOptions::ThreadCache::kDisabled,
                      PartitionOptions::Quarantine::kAllowed,
                      PartitionOptions::Cookies::kAllowed,
                      PartitionOptions::RefCount::kDisallowed});
  }

  void TearDown() override {
    allocator_ = nullptr;
    PartitionAllocMemoryReclaimer::Instance()->ResetForTesting();
    task_environment_.FastForwardUntilNoTasksRemain();
    PartitionAllocGlobalUninitForTesting();
  }

  void StartReclaimer() {
    auto* memory_reclaimer = PartitionAllocMemoryReclaimer::Instance();
    memory_reclaimer->Start(task_environment_.GetMainThreadTaskRunner());
  }

  void AllocateAndFree() {
    void* data = allocator_->root()->Alloc(1, "");
    allocator_->root()->Free(data);
  }

  test::TaskEnvironment task_environment_;
  std::unique_ptr<PartitionAllocator> allocator_;
};

TEST_F(PartitionAllocMemoryReclaimerTest, Simple) {
  StartReclaimer();

  EXPECT_EQ(1u, task_environment_.GetPendingMainThreadTaskCount());
  EXPECT_TRUE(task_environment_.NextTaskIsDelayed());
}

TEST_F(PartitionAllocMemoryReclaimerTest, FreesMemory) {
  PartitionRoot<internal::ThreadSafe>* root = allocator_->root();

  size_t committed_initially = root->get_total_size_of_committed_pages();
  AllocateAndFree();
  size_t committed_before = root->get_total_size_of_committed_pages();

  EXPECT_GT(committed_before, committed_initially);

  StartReclaimer();
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  size_t committed_after = root->get_total_size_of_committed_pages();
  EXPECT_LT(committed_after, committed_before);
  EXPECT_LE(committed_initially, committed_after);
}

TEST_F(PartitionAllocMemoryReclaimerTest, Reclaim) {
  PartitionRoot<internal::ThreadSafe>* root = allocator_->root();
  size_t committed_initially = root->get_total_size_of_committed_pages();

  {
    AllocateAndFree();

    size_t committed_before = root->get_total_size_of_committed_pages();
    EXPECT_GT(committed_before, committed_initially);
    PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
    size_t committed_after = root->get_total_size_of_committed_pages();

    EXPECT_LT(committed_after, committed_before);
    EXPECT_LE(committed_initially, committed_after);
  }
}

// ThreadCache tests disabled  when ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL is
// enabled, because the "original" PartitionRoot has ThreadCache disabled.
#if BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
    defined(PA_THREAD_CACHE_SUPPORTED) &&       \
    !BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)

namespace {
// malloc() / free() pairs can be removed by the compiler, this is enough (for
// now) to prevent that.
NOINLINE void FreeForTest(void* data) {
  free(data);
}
}  // namespace

TEST_F(PartitionAllocMemoryReclaimerTest, DoNotAlwaysPurgeThreadCache) {
  for (size_t i = 0; i < internal::ThreadCache::kDefaultSizeThreshold; i++) {
    void* data = malloc(i);
    FreeForTest(data);
  }

  auto* tcache = internal::ThreadCache::Get();
  ASSERT_TRUE(tcache);
  size_t cached_size = tcache->CachedMemory();

  StartReclaimer();
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());

  // No thread cache purging during periodic purge, but with ReclaimAll().
  //
  // Cannot assert on the exact size of the thread cache, since it can shrink
  // when a buffer is overfull, and this may happen through other malloc()
  // allocations in the test harness.
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  PartitionAllocMemoryReclaimer::Instance()->ReclaimPeriodically();
  EXPECT_GT(tcache->CachedMemory(), cached_size / 2);

  PartitionAllocMemoryReclaimer::Instance()->ReclaimAll();
  EXPECT_LT(tcache->CachedMemory(), cached_size / 2);
}

#endif  // BUILDFLAG(USE_PARTITION_ALLOC_AS_MALLOC) && \
        // defined(PA_THREAD_CACHE_SUPPORTED) && \
        // !BUILDFLAG(ENABLE_RUNTIME_BACKUP_REF_PTR_CONTROL)

}  // namespace base
#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
