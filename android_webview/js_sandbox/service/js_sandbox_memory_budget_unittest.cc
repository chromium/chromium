// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "android_webview/js_sandbox/service/js_sandbox_memory_budget.h"

#include <memory>
#include <vector>

#include "base/barrier_closure.h"
#include "base/run_loop.h"
#include "base/task/thread_pool.h"
#include "base/test/bind.h"
#include "base/test/gtest_util.h"
#include "base/test/task_environment.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace android_webview {

class JsSandboxMemoryBudgetTest : public ::testing::Test {
 protected:
  void SetUp() override {
    memory_budget_ =
        std::make_unique<JsSandboxMemoryBudget>(kBudget, kPageSize);
  }

  void TearDown() override { memory_budget_.reset(); }

  std::unique_ptr<JsSandboxMemoryBudget> memory_budget_;
  // We're not actually allocating memory for real, so 4KiB is fine regardless
  // of the actual test environment's page size.
  constexpr static size_t kPageSize = 4096;
  constexpr static size_t kBudget = 1024 * 1024;
};

TEST_F(JsSandboxMemoryBudgetTest, TestUsageInitiallyZero) {
  EXPECT_EQ(memory_budget_->GetUsage(), 0u);
}

TEST_F(JsSandboxMemoryBudgetTest, TestAllocateWholeBudgetInSingleAllocation) {
  EXPECT_TRUE(memory_budget_->Allocate(kBudget));
  EXPECT_EQ(memory_budget_->GetUsage(), kBudget);
  memory_budget_->Free(kBudget);
}

TEST_F(JsSandboxMemoryBudgetTest, TestAllocateWithNoBudget) {
  EXPECT_TRUE(memory_budget_->Allocate(kBudget));
  EXPECT_FALSE(memory_budget_->Allocate(1));
  memory_budget_->Free(kBudget);
}

TEST_F(JsSandboxMemoryBudgetTest, TestAllocateWithInsuffientBudget) {
  EXPECT_TRUE(memory_budget_->Allocate(1));
  EXPECT_FALSE(memory_budget_->Allocate(kBudget));
  memory_budget_->Free(1);
}

TEST_F(JsSandboxMemoryBudgetTest, TestAllocatePageSize) {
  EXPECT_TRUE(memory_budget_->Allocate(1));
  EXPECT_EQ(memory_budget_->GetUsage(), kPageSize);
  memory_budget_->Free(1);
}

// Verifies that allocation size that rounded up to page size will fail because
// of exceeding the memory budget.
TEST_F(JsSandboxMemoryBudgetTest,
       testAllocateRoundedUpToPageThatExceedsBudget) {
  memory_budget_ =
      std::make_unique<JsSandboxMemoryBudget>(kBudget - 1, kPageSize);
  EXPECT_FALSE(memory_budget_->Allocate(kBudget - 1));
}

// Verifies that allocating 0 bytes succeeds without allocating any memory.
TEST_F(JsSandboxMemoryBudgetTest, TestAllocateZeroBytes) {
  EXPECT_TRUE(memory_budget_->Allocate(0));
  EXPECT_EQ(memory_budget_->GetUsage(), 0u);
  // It is technically correct to request a corresponding free of 0, even though
  // it may effectively be a no-op.
  memory_budget_->Free(0);
}

// Verifies that allocating 0 bytes succeeds without allocating any memory, even
// if there is no budget remaining.
TEST_F(JsSandboxMemoryBudgetTest, TestAllocateZeroBytesWhenFull) {
  EXPECT_TRUE(memory_budget_->Allocate(kBudget));
  EXPECT_TRUE(memory_budget_->Allocate(0));
  EXPECT_EQ(memory_budget_->GetUsage(), kBudget);
  // It is technically correct to request a corresponding free of 0, even though
  // it may effectively be a no-op.
  memory_budget_->Free(0);
  memory_budget_->Free(kBudget);
}

// Verifies that you can allocate memory after freeing it.
TEST_F(JsSandboxMemoryBudgetTest, TestAllocateAfterFree) {
  EXPECT_TRUE(memory_budget_->Allocate(kBudget));
  EXPECT_EQ(memory_budget_->GetUsage(), kBudget);

  memory_budget_->Free(kBudget);
  EXPECT_EQ(memory_budget_->GetUsage(), 0u);

  EXPECT_TRUE(memory_budget_->Allocate(kBudget));
  EXPECT_EQ(memory_budget_->GetUsage(), kBudget);

  memory_budget_->Free(kBudget);
}

// Verifies thread safety for concurrent allocations from multiple threads.
TEST_F(JsSandboxMemoryBudgetTest, TestAllocateConcurrent) {
  base::test::TaskEnvironment task_environment;

  // Each thread attempt to allocate and then deallocate some memory.
  // Different threads have different characteristics:
  //   T1-4: Should always succeed, allocating a page at a time.
  //   T5-8: Sometimes fail, conflicting with each other to allocate 6 pages.
  const size_t budget = kPageSize * 16;
  const int num_allocations_per_thread = 10000;

  memory_budget_ = std::make_unique<JsSandboxMemoryBudget>(budget, kPageSize);

  base::RunLoop run_loop;
  base::RepeatingClosure barrier_closure =
      base::BarrierClosure(8, run_loop.QuitClosure());

  auto small_task = [&]() {
    for (int i = 0; i < num_allocations_per_thread; i++) {
      EXPECT_TRUE(memory_budget_->Allocate(kPageSize));
      memory_budget_->Free(kPageSize);
    }
    barrier_closure.Run();
  };
  auto large_task = [&]() {
    for (int i = 0; i < num_allocations_per_thread; i++) {
      if (memory_budget_->Allocate(6 * kPageSize)) {
        memory_budget_->Free(6 * kPageSize);
      }
    }
    barrier_closure.Run();
  };

  for (int i = 0; i < 4; ++i) {
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindLambdaForTesting(small_task));
    base::ThreadPool::PostTask(FROM_HERE,
                               base::BindLambdaForTesting(large_task));
  }

  run_loop.Run();

  EXPECT_EQ(memory_budget_->GetUsage(), 0u);
}

// Verifies that freeing more memory than is allocated will crash the process.
TEST_F(JsSandboxMemoryBudgetTest, TestFreeMemoryUnallocated) {
  EXPECT_CHECK_DEATH(memory_budget_->Free(1));
}

}  // namespace android_webview
