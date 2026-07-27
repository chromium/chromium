// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the unit tests for the bit utilities.

#include <cstddef>
#include <limits>

#include "partition_alloc/build_config.h"
#include "partition_alloc/partition_alloc_base/bits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base::bits {

TEST(BitsTestPA, Log2Ceiling) {
  EXPECT_EQ(-1, Log2Ceiling(0));
  EXPECT_EQ(0, Log2Ceiling(1));
  EXPECT_EQ(1, Log2Ceiling(2));
  EXPECT_EQ(2, Log2Ceiling(3));
  EXPECT_EQ(2, Log2Ceiling(4));
  for (int i = 3; i < 31; ++i) {
    unsigned int value = 1U << i;
    EXPECT_EQ(i, Log2Ceiling(value));
    EXPECT_EQ(i + 1, Log2Ceiling(value + 1));
    EXPECT_EQ(i + 1, Log2Ceiling(value + 2));
    EXPECT_EQ(i, Log2Ceiling(value - 1));
    EXPECT_EQ(i, Log2Ceiling(value - 2));
  }
  EXPECT_EQ(32, Log2Ceiling(0xffffffffU));
}

TEST(BitsTestPA, AlignUp) {
  static constexpr size_t kSizeTMax = std::numeric_limits<size_t>::max();
  EXPECT_EQ(0u, AlignUp(0u, 4u));
  EXPECT_EQ(4u, AlignUp(1u, 4u));
  EXPECT_EQ(4096u, AlignUp(1u, 4096u));
  EXPECT_EQ(4096u, AlignUp(4096u, 4096u));
  EXPECT_EQ(4096u, AlignUp(4095u, 4096u));
  EXPECT_EQ(8192u, AlignUp(4097u, 4096u));
  EXPECT_EQ(kSizeTMax - 31, AlignUp(kSizeTMax - 62, size_t{32}));
  EXPECT_EQ(kSizeTMax / 2 + 1, AlignUp(size_t{1}, kSizeTMax / 2 + 1));
}

TEST(BitsTestPA, AlignUpPointer) {
  static constexpr uintptr_t kUintPtrTMax =
      std::numeric_limits<uintptr_t>::max();
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignUp(reinterpret_cast<uint8_t*>(0), 4));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4),
            AlignUp(reinterpret_cast<uint8_t*>(1), 4));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4096),
            AlignUp(reinterpret_cast<uint8_t*>(1), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4096),
            AlignUp(reinterpret_cast<uint8_t*>(4096), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4096),
            AlignUp(reinterpret_cast<uint8_t*>(4095), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(8192),
            AlignUp(reinterpret_cast<uint8_t*>(4097), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(kUintPtrTMax - 31),
            AlignUp(reinterpret_cast<uint8_t*>(kUintPtrTMax - 62), 32));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(kUintPtrTMax / 2 + 1),
            AlignUp(reinterpret_cast<uint8_t*>(1), kUintPtrTMax / 2 + 1));
}

TEST(BitsTestPA, AlignDown) {
  static constexpr size_t kSizeTMax = std::numeric_limits<size_t>::max();
  EXPECT_EQ(0u, AlignDown(0u, 4u));
  EXPECT_EQ(0u, AlignDown(1u, 4u));
  EXPECT_EQ(0u, AlignDown(1u, 4096u));
  EXPECT_EQ(4096u, AlignDown(4096u, 4096u));
  EXPECT_EQ(0u, AlignDown(4095u, 4096u));
  EXPECT_EQ(4096u, AlignDown(4097u, 4096u));
  EXPECT_EQ(kSizeTMax - 63, AlignDown(kSizeTMax - 62, size_t{32}));
  EXPECT_EQ(kSizeTMax - 31, AlignDown(kSizeTMax, size_t{32}));
  EXPECT_EQ(0ul, AlignDown(size_t{1}, kSizeTMax / 2 + 1));
}

TEST(BitsTestPA, AlignDownPointer) {
  static constexpr uintptr_t kUintPtrTMax =
      std::numeric_limits<uintptr_t>::max();
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignDown(reinterpret_cast<uint8_t*>(0), 4));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignDown(reinterpret_cast<uint8_t*>(1), 4));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignDown(reinterpret_cast<uint8_t*>(1), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4096),
            AlignDown(reinterpret_cast<uint8_t*>(4096), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignDown(reinterpret_cast<uint8_t*>(4095), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(4096),
            AlignDown(reinterpret_cast<uint8_t*>(4097), 4096));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(kUintPtrTMax - 63),
            AlignDown(reinterpret_cast<uint8_t*>(kUintPtrTMax - 62), 32));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(kUintPtrTMax - 31),
            AlignDown(reinterpret_cast<uint8_t*>(kUintPtrTMax), 32));
  EXPECT_EQ(reinterpret_cast<uint8_t*>(0),
            AlignDown(reinterpret_cast<uint8_t*>(1), kUintPtrTMax / 2 + 1));
}

TEST(BitsTestPA, RotR32) {
  EXPECT_EQ(RotR<uint32_t>(3696969696, 0), 3696969696u);
  EXPECT_EQ(RotR<uint32_t>(3696969696, 1), 1848484848u);
  EXPECT_EQ(RotR<uint32_t>(3696969696, 10), 4164359889u);
  EXPECT_EQ(RotR<uint32_t>(3696969696, 31), 3098972097u);
  EXPECT_EQ(RotR<uint32_t>(3696969696, 32), 3696969696u);
  EXPECT_EQ(RotR<uint32_t>(3696969696, 40), 3772537671u);
}

TEST(BitsTestPA, RotR64) {
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 0), 36969696969696ull);
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 1), 18484848484848ull);
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 10), 17870283357509347824ull);
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 63), 73939393939392ull);
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 64), 36969696969696ull);
  EXPECT_EQ(RotR<uint64_t>(36969696969696, 80), 14114281232743247271ull);
}

}  // namespace partition_alloc::internal::base::bits
