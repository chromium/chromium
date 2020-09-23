// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/memory_reclaimer.h"

#include <memory>
#include <utility>

#include "base/allocator/partition_allocator/partition_alloc.h"
#include "base/logging.h"
#include "base/test/task_environment.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

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
    allocator_->init();
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

  size_t committed_initially =
      root->total_size_of_committed_pages_for_testing();
  AllocateAndFree();
  size_t committed_before = root->total_size_of_committed_pages_for_testing();

  EXPECT_GT(committed_before, committed_initially);

  StartReclaimer();
  task_environment_.FastForwardBy(
      task_environment_.NextMainThreadPendingTaskDelay());
  size_t committed_after = root->total_size_of_committed_pages_for_testing();
  EXPECT_LT(committed_after, committed_before);
  EXPECT_LE(committed_initially, committed_after);
}

TEST_F(PartitionAllocMemoryReclaimerTest, Reclaim) {
  PartitionRoot<internal::ThreadSafe>* root = allocator_->root();
  size_t committed_initially =
      root->total_size_of_committed_pages_for_testing();

  {
    AllocateAndFree();

    size_t committed_before = root->total_size_of_committed_pages_for_testing();
    EXPECT_GT(committed_before, committed_initially);
    PartitionAllocMemoryReclaimer::Instance()->Reclaim();
    size_t committed_after = root->total_size_of_committed_pages_for_testing();

    EXPECT_LT(committed_after, committed_before);
    EXPECT_LE(committed_initially, committed_after);
  }
}

}  // namespace base
#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
