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

TEST(BitsTestPA, BitWidth) {
  EXPECT_EQ(0, BitWidth(0));
  EXPECT_EQ(1, BitWidth(1));
  EXPECT_EQ(2, BitWidth(2));
  EXPECT_EQ(2, BitWidth(3));
  EXPECT_EQ(3, BitWidth(4));
  for (int i = 3; i < 31; ++i) {
    unsigned int value = 1U << i;
    EXPECT_EQ(i + 1, BitWidth(value));
    EXPECT_EQ(i + 1, BitWidth(value + 1));
    EXPECT_EQ(i + 1, BitWidth(value + 2));
    EXPECT_EQ(i, BitWidth(value - 1));
    EXPECT_EQ(i, BitWidth(value - 2));
  }
  EXPECT_EQ(32, BitWidth(0xffffffffU));
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

TEST(BitsTestPA, PowerOfTwo) {
  EXPECT_FALSE(HasSingleBit(0u));
  EXPECT_TRUE(HasSingleBit(1u));
  EXPECT_TRUE(HasSingleBit(2u));
  // Unsigned 64 bit cases.
  for (uint32_t i = 2; i < 64; i++) {
    const uint64_t val = uint64_t{1} << i;
    EXPECT_FALSE(HasSingleBit(val - 1));
    EXPECT_TRUE(HasSingleBit(val));
    EXPECT_FALSE(HasSingleBit(val + 1));
  }
}

TEST(BitsTestPA, CountlZero8) {
  EXPECT_EQ(8, CountlZero(uint8_t{0}));
  EXPECT_EQ(7, CountlZero(uint8_t{1}));
  for (int shift = 0; shift <= 7; ++shift) {
    EXPECT_EQ(7 - shift, CountlZero(static_cast<uint8_t>(1 << shift)));
  }
  EXPECT_EQ(4, CountlZero(uint8_t{0x0f}));
}

TEST(BitsTestPA, CountlZero16) {
  EXPECT_EQ(16, CountlZero(uint16_t{0}));
  EXPECT_EQ(15, CountlZero(uint16_t{1}));
  for (int shift = 0; shift <= 15; ++shift) {
    EXPECT_EQ(15 - shift, CountlZero(static_cast<uint16_t>(1 << shift)));
  }
  EXPECT_EQ(4, CountlZero(uint16_t{0x0f0f}));
}

TEST(BitsTestPA, CountlZero32) {
  EXPECT_EQ(32, CountlZero(uint32_t{0}));
  EXPECT_EQ(31, CountlZero(uint32_t{1}));
  for (int shift = 0; shift <= 31; ++shift) {
    EXPECT_EQ(31 - shift, CountlZero(uint32_t{1} << shift));
  }
  EXPECT_EQ(4, CountlZero(uint32_t{0x0f0f0f0f}));
}

TEST(BitsTestPA, CountrZero8) {
  EXPECT_EQ(8, CountrZero(uint8_t{0}));
  EXPECT_EQ(7, CountrZero(uint8_t{128}));
  for (int shift = 0; shift <= 7; ++shift) {
    EXPECT_EQ(shift, CountrZero(static_cast<uint8_t>(1 << shift)));
  }
  EXPECT_EQ(4, CountrZero(uint8_t{0xf0}));
}

TEST(BitsTestPA, CountrZero16) {
  EXPECT_EQ(16, CountrZero(uint16_t{0}));
  EXPECT_EQ(15, CountrZero(uint16_t{32768}));
  for (int shift = 0; shift <= 15; ++shift) {
    EXPECT_EQ(shift, CountrZero(static_cast<uint16_t>(1 << shift)));
  }
  EXPECT_EQ(4, CountrZero(uint16_t{0xf0f0}));
}

TEST(BitsTestPA, CountrZero32) {
  EXPECT_EQ(32, CountrZero(uint32_t{0}));
  EXPECT_EQ(31, CountrZero(uint32_t{1} << 31));
  for (int shift = 0; shift <= 31; ++shift) {
    EXPECT_EQ(shift, CountrZero(uint32_t{1} << shift));
  }
  EXPECT_EQ(4, CountrZero(uint32_t{0xf0f0f0f0}));
}

TEST(BitsTestPA, CountlZero64) {
  EXPECT_EQ(64, CountlZero(uint64_t{0}));
  EXPECT_EQ(63, CountlZero(uint64_t{1}));
  for (int shift = 0; shift <= 63; ++shift) {
    EXPECT_EQ(63 - shift, CountlZero(uint64_t{1} << shift));
  }
  EXPECT_EQ(4, CountlZero(uint64_t{0x0f0f0f0f0f0f0f0f}));
}

TEST(BitsTestPA, CountrZero64) {
  EXPECT_EQ(64, CountrZero(uint64_t{0}));
  EXPECT_EQ(63, CountrZero(uint64_t{1} << 63));
  for (int shift = 0; shift <= 31; ++shift) {
    EXPECT_EQ(shift, CountrZero(uint64_t{1} << shift));
  }
  EXPECT_EQ(4, CountrZero(uint64_t{0xf0f0f0f0f0f0f0f0}));
}

TEST(BitsTestPA, CountlZeroSizeT) {
#if PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
  EXPECT_EQ(64, CountlZero(size_t{0}));
  EXPECT_EQ(63, CountlZero(size_t{1}));
  EXPECT_EQ(32, CountlZero(size_t{1} << 31));
  EXPECT_EQ(1, CountlZero(size_t{1} << 62));
  EXPECT_EQ(0, CountlZero(size_t{1} << 63));
#else
  EXPECT_EQ(32, CountlZero(size_t{0}));
  EXPECT_EQ(31, CountlZero(size_t{1}));
  EXPECT_EQ(1, CountlZero(size_t{1} << 30));
  EXPECT_EQ(0, CountlZero(size_t{1} << 31));
#endif
}

TEST(BitsTestPA, CountrZeroSizeT) {
#if PA_BUILDFLAG(PA_ARCH_CPU_64_BITS)
  EXPECT_EQ(64, CountrZero(size_t{0}));
  EXPECT_EQ(63, CountrZero(size_t{1} << 63));
  EXPECT_EQ(31, CountrZero(size_t{1} << 31));
  EXPECT_EQ(1, CountrZero(size_t{2}));
  EXPECT_EQ(0, CountrZero(size_t{1}));
#else
  EXPECT_EQ(32, CountrZero(size_t{0}));
  EXPECT_EQ(31, CountrZero(size_t{1} << 31));
  EXPECT_EQ(1, CountrZero(size_t{2}));
  EXPECT_EQ(0, CountrZero(size_t{1}));
#endif
}

}  // namespace partition_alloc::internal::base::bits
