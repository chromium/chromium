// Copyright 2009 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

// This file contains the unit tests for the bit utilities.

#include <cstddef>
#include <limits>

#include "partition_alloc/partition_alloc_base/bits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace partition_alloc::internal::base::bits {

TEST(BitsTestPA, Log2Floor) {
  EXPECT_EQ(-1, Log2Floor(0));
  EXPECT_EQ(0, Log2Floor(1));
  EXPECT_EQ(1, Log2Floor(2));
  EXPECT_EQ(1, Log2Floor(3));
  EXPECT_EQ(2, Log2Floor(4));
  for (int i = 3; i < 31; ++i) {
    unsigned int value = 1U << i;
    EXPECT_EQ(i, Log2Floor(value));
    EXPECT_EQ(i, Log2Floor(value + 1));
    EXPECT_EQ(i, Log2Floor(value + 2));
    EXPECT_EQ(i - 1, Log2Floor(value - 1));
    EXPECT_EQ(i - 1, Log2Floor(value - 2));
  }
  EXPECT_EQ(31, Log2Floor(0xffffffffU));
}

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

TEST(BitsTestPA, LeftMostBit) {
  // Construction of a signed type from an unsigned one of the same width
  // preserves all bits. Explicitly confirming this behavior here to illustrate
  // correctness of reusing unsigned literals to test behavior of signed types.
  // Using signed literals does not work with EXPECT_EQ.
  static_assert(
      static_cast<int64_t>(0xFFFFFFFFFFFFFFFFu) == 0xFFFFFFFFFFFFFFFFl,
      "Comparing signed with unsigned literals compares bits.");
  static_assert((0xFFFFFFFFFFFFFFFFu ^ 0xFFFFFFFFFFFFFFFFl) == 0,
                "Signed and unsigned literals have the same bits set");

  uint64_t unsigned_long_long_value = 0x8000000000000000u;
  EXPECT_EQ(LeftmostBit<uint64_t>(), unsigned_long_long_value);
  EXPECT_EQ(LeftmostBit<int64_t>(), int64_t(unsigned_long_long_value));

  uint32_t unsigned_long_value = 0x80000000u;
  EXPECT_EQ(LeftmostBit<uint32_t>(), unsigned_long_value);
  EXPECT_EQ(LeftmostBit<int32_t>(), int32_t(unsigned_long_value));

  uint16_t unsigned_short_value = 0x8000u;
  EXPECT_EQ(LeftmostBit<uint16_t>(), unsigned_short_value);
  EXPECT_EQ(LeftmostBit<int16_t>(), int16_t(unsigned_short_value));

  uint8_t unsigned_byte_value = 0x80u;
  EXPECT_EQ(LeftmostBit<uint8_t>(), unsigned_byte_value);
  EXPECT_EQ(LeftmostBit<int8_t>(), int8_t(unsigned_byte_value));
}

}  // namespace partition_alloc::internal::base::bits
