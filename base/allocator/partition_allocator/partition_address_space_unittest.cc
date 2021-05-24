// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/allocator/partition_allocator/partition_address_space.h"

#include <array>

#include "base/allocator/partition_allocator/partition_alloc_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace internal {

#if defined(PA_HAS_64_BITS_POINTERS)

TEST(PartitionAllocAddressSpaceTest, CalculateGigaCageProperties) {
  GigaCageProperties props;

  props = CalculateGigaCageProperties(std::array<size_t, 1>{1});
  EXPECT_EQ(1u, props.size);
  EXPECT_EQ(1u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 2>{2, 1});
  EXPECT_EQ(3u, props.size);
  EXPECT_EQ(2u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 2>{1, 2});
  EXPECT_EQ(3u, props.size);
  EXPECT_EQ(2u, props.alignment);
  EXPECT_EQ(1u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 4>{8, 4, 2, 1});
  EXPECT_EQ(15u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 4>{1, 2, 4, 8});
  EXPECT_EQ(15u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(1u, props.alignment_offset);

  props =
      CalculateGigaCageProperties(std::array<size_t, 7>{1, 2, 4, 2, 2, 4, 8});
  EXPECT_EQ(23u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(1u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 3>{8, 4, 4});
  EXPECT_EQ(16u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 3>{4, 4, 8});
  EXPECT_EQ(16u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  props = CalculateGigaCageProperties(std::array<size_t, 4>{8, 4, 4, 8});
  EXPECT_EQ(24u, props.size);
  EXPECT_EQ(8u, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);

  size_t GiB = 1024ull * 1024 * 1024;
  props = CalculateGigaCageProperties(
      std::array<size_t, 6>{GiB, GiB, GiB, GiB, GiB, GiB});
  EXPECT_EQ(6 * GiB, props.size);
  EXPECT_EQ(GiB, props.alignment);
  EXPECT_EQ(0u, props.alignment_offset);
}

TEST(PartitionAllocAddressSpaceTest, CalculateGigaCagePropertiesImpossible) {
  EXPECT_DEATH_IF_SUPPORTED(
      CalculateGigaCageProperties(std::array<size_t, 1>{0}), "");
  EXPECT_DEATH_IF_SUPPORTED(
      CalculateGigaCageProperties(std::array<size_t, 1>{3}), "");
  EXPECT_DEATH_IF_SUPPORTED(
      CalculateGigaCageProperties(std::array<size_t, 3>{8, 4, 8}), "");
  EXPECT_DEATH_IF_SUPPORTED(
      CalculateGigaCageProperties(std::array<size_t, 7>{1, 2, 4, 1, 2, 4, 8}),
      "");
}

#endif  // defined(PA_HAS_64_BITS_POINTERS)

}  // namespace internal
}  // namespace base
