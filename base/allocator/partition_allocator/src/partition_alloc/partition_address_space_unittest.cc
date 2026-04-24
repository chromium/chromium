// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/partition_address_space.h"

#include "partition_alloc/buildflags.h"
#include "partition_alloc/partition_alloc_config.h"
#include "partition_alloc/use_death_tests.h"
#include "testing/gtest/include/gtest/gtest.h"

#if PA_BUILDFLAG(HAS_64_BIT_POINTERS)

namespace partition_alloc::internal {

TEST(PartitionAddressSpaceTest, ZeroSegment) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
  const size_t expected_min_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;
  // We assume that we are always able to acquire the configured size when
  // executing a unit test.
  EXPECT_GE(size, expected_min_size);
#else
  EXPECT_EQ(size, 0u);
#endif
}

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_USE_DEATH_TESTS()
TEST(PartitionAddressSpaceTest, ZeroSegmentInaccessibleBegin) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }
  EXPECT_DEATH(
      {
        volatile char* ptr =
            reinterpret_cast<volatile char*>(static_cast<uintptr_t>(0));
        char val = *ptr;
        (void)val;
      },
      "");
}

TEST(PartitionAddressSpaceTest, ZeroSegmentInaccessibleEnd) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }
  EXPECT_DEATH(
      {
        volatile char* ptr =
            reinterpret_cast<volatile char*>(static_cast<uintptr_t>(size - 1));
        char val = *ptr;
        (void)val;
      },
      "");
}
#endif

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
