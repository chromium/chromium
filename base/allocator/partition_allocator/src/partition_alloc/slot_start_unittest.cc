// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_alloc_for_testing.h"
#include "partition_alloc/partition_page.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

// These tests are default-disabled when PA passes through to a
// sanitizer, in which case the values returned from `Alloc()` are not
// managed by PartitionAlloc.
#if !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)

namespace partition_alloc::internal {
namespace {

constexpr PartitionOptions kOptions{};

class SlotStartTest : public testing::Test {
 protected:
  SlotStartTest()
      : allocator_(partition_alloc::PartitionAllocatorForTesting(kOptions)) {}

  partition_alloc::PartitionAllocatorForTesting allocator_;
};

TEST_F(SlotStartTest, SlotStartDoesntCrash) {
  void* buffer = allocator_.root()->Alloc(16, "");

  // `buffer` _is_ a slot start, so this must not crash.
  SlotStart::FromObject</*enforce=*/true>(buffer);

  // This is _not_ a slot start, but with enforcement off, this also
  // must not crash.
  SlotStart::FromObject</*enforce=*/false>(static_cast<char*>(buffer) + 1);

  allocator_.root()->Free(buffer);
}

#if PA_USE_DEATH_TESTS()
TEST_F(SlotStartTest, SlotStartCrashes) {
  void* buffer = allocator_.root()->Alloc(16, "");

  // `buffer + 1` is not a slot start, so this must crash.
  EXPECT_DEATH_IF_SUPPORTED(
      SlotStart::FromObject</*enforce=*/true>(static_cast<char*>(buffer) + 1),
      "");

  allocator_.root()->Free(buffer);
}
#endif  // PA_USE_DEATH_TESTS()

}  // namespace
}  // namespace partition_alloc::internal

#endif  // !defined(MEMORY_TOOL_REPLACES_ALLOCATOR)
