// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/memory/discardable_memory_allocator.h"

#include <memory>

#include "base/byte_size.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/discardable_memory.h"
#include "base/test/bind.h"
#include "base/test/test_discardable_memory_allocator.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

// Fails the first `failures` allocations, then allocates normally. Counts calls
// to `AllocateLockedDiscardableMemory()` and `ReleaseFreeMemory()`.
class FailingDiscardableMemoryAllocator
    : public TestDiscardableMemoryAllocator {
 public:
  explicit FailingDiscardableMemoryAllocator(int failures)
      : failures_remaining_(failures) {}

  std::unique_ptr<DiscardableMemory> AllocateLockedDiscardableMemory(
      ByteSize size) override {
    ++allocation_count_;
    if (failures_remaining_ > 0) {
      --failures_remaining_;
      return nullptr;
    }
    return TestDiscardableMemoryAllocator::AllocateLockedDiscardableMemory(
        size);
  }

  void ReleaseFreeMemory() override { ++release_free_memory_count_; }

  int allocation_count() const { return allocation_count_; }
  int release_free_memory_count() const { return release_free_memory_count_; }

 private:
  int failures_remaining_;
  int allocation_count_ = 0;
  int release_free_memory_count_ = 0;
};

TEST(DiscardableMemoryAllocatorTest, RetryOrDieSucceedsWithoutRetry) {
  FailingDiscardableMemoryAllocator allocator(/*failures=*/0);
  bool on_no_memory_called = false;

  std::unique_ptr<DiscardableMemory> memory =
      allocator.AllocateLockedDiscardableMemoryWithRetryOrDie(
          KiB(4), BindLambdaForTesting([&] { on_no_memory_called = true; }));

  EXPECT_TRUE(memory);
  EXPECT_FALSE(on_no_memory_called);
  EXPECT_EQ(allocator.allocation_count(), 1);
  EXPECT_EQ(allocator.release_free_memory_count(), 0);
}

// The allocation and the retry must both use the allocator the method is
// called on, not the global instance.
TEST(DiscardableMemoryAllocatorTest, RetryOrDieUsesThisAllocator) {
  FailingDiscardableMemoryAllocator global_allocator(/*failures=*/0);
  DiscardableMemoryAllocator::SetInstance(&global_allocator);
  ScopedClosureRunner reset_global_allocator(
      BindOnce([] { DiscardableMemoryAllocator::SetInstance(nullptr); }));

  FailingDiscardableMemoryAllocator allocator(/*failures=*/1);
  bool on_no_memory_called = false;

  std::unique_ptr<DiscardableMemory> memory =
      allocator.AllocateLockedDiscardableMemoryWithRetryOrDie(
          KiB(4), BindLambdaForTesting([&] { on_no_memory_called = true; }));

  EXPECT_TRUE(memory);
  EXPECT_TRUE(on_no_memory_called);
  EXPECT_EQ(allocator.allocation_count(), 2);
  EXPECT_EQ(allocator.release_free_memory_count(), 1);
  EXPECT_EQ(global_allocator.allocation_count(), 0);
  EXPECT_EQ(global_allocator.release_free_memory_count(), 0);
}

}  // namespace
}  // namespace base
