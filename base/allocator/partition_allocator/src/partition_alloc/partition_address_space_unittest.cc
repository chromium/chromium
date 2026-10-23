// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "partition_alloc/buildflags.h"
#include "partition_alloc/internal/page_allocator_internal.h"
#include "partition_alloc/internal/partition_address_space_internal.h"
#include "partition_alloc/page_allocator_constants.h"
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
  // executing a unit test. On Android, this test runs in a raw native
  // executable (`base_unittests_android_death_tests`) without ART, so it works
  // even prior to Android 17 (which introduced the Native Zygote).
  EXPECT_GE(size, expected_min_size);
#else
  EXPECT_EQ(size, 0u);
#endif
}

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)
TEST(PartitionAddressSpaceTest, WellKnownReadOnlyRegions) {
  const size_t min_zero_segment_size =
      static_cast<size_t>(PA_CONFIG(USER_SPACE_ZERO_SEGMENT_SIZE_MB)) * 1024 *
      1024;
  const WellKnownReadOnlyRegions well_known =
      GetWellKnownReadOnlyRegions(min_zero_segment_size);
  ASSERT_GE(well_known.count, 1u);
  ASSERT_LE(well_known.count, WellKnownReadOnlyRegions::kMaxRegions);
  EXPECT_EQ(well_known.regions[0].address, 0u);
  for (size_t i = 0; i < well_known.count; ++i) {
    EXPECT_LT(well_known.regions[i].address, min_zero_segment_size);
    EXPECT_GT(well_known.regions[i].size, 0u);
    EXPECT_EQ(well_known.regions[i].address % PageAllocationGranularity(), 0u);
    EXPECT_EQ(well_known.regions[i].size % PageAllocationGranularity(), 0u);
    if (i > 0) {
      EXPECT_GT(
          well_known.regions[i].address,
          well_known.regions[i - 1].address + well_known.regions[i - 1].size);
    }
  }
}
#endif  // PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT)

#if PA_CONFIG(ENABLE_USER_SPACE_ZERO_SEGMENT) && PA_USE_DEATH_TESTS()
TEST(PartitionAddressSpaceTest, ZeroSegmentInaccessibleBegin) {
  const size_t size = PartitionAddressSpace::GetZeroSegmentSize();
  if (size == 0) {
    return;
  }
  EXPECT_DEATH(
      {
        volatile uintptr_t zero = 0;
        volatile char* ptr = reinterpret_cast<volatile char*>(zero);
        *ptr = 0;
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
        *ptr = 0;
      },
      "");
}
#endif

}  // namespace partition_alloc::internal

#endif  // PA_BUILDFLAG(HAS_64_BIT_POINTERS)
