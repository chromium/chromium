// Copyright (c) 2016 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/sys_byteorder.h"

#include <stdint.h>

#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

const uint16_t k16BitTestData = 0xaabb;
const uint16_t k16BitSwappedTestData = 0xbbaa;
const uint32_t k32BitTestData = 0xaabbccdd;
const uint32_t k32BitSwappedTestData = 0xddccbbaa;
const uint64_t k64BitTestData = 0xaabbccdd44332211;
const uint64_t k64BitSwappedTestData = 0x11223344ddccbbaa;

}  // namespace

TEST(ByteOrderTest, ByteSwap16) {
  uint16_t swapped = base::ByteSwap(k16BitTestData);
  EXPECT_EQ(k16BitSwappedTestData, swapped);
  uint16_t reswapped = base::ByteSwap(swapped);
  EXPECT_EQ(k16BitTestData, reswapped);
}

TEST(ByteOrderTest, ByteSwap32) {
  uint32_t swapped = base::ByteSwap(k32BitTestData);
  EXPECT_EQ(k32BitSwappedTestData, swapped);
  uint32_t reswapped = base::ByteSwap(swapped);
  EXPECT_EQ(k32BitTestData, reswapped);
}

TEST(ByteOrderTest, ByteSwap64) {
  uint64_t swapped = base::ByteSwap(k64BitTestData);
  EXPECT_EQ(k64BitSwappedTestData, swapped);
  uint64_t reswapped = base::ByteSwap(swapped);
  EXPECT_EQ(k64BitTestData, reswapped);
}

TEST(ByteOrderTest, ByteSwapUintPtrT) {
#if defined(ARCH_CPU_64_BITS)
  const uintptr_t test_data = static_cast<uintptr_t>(k64BitTestData);
  const uintptr_t swapped_test_data =
      static_cast<uintptr_t>(k64BitSwappedTestData);
#elif defined(ARCH_CPU_32_BITS)
  const uintptr_t test_data = static_cast<uintptr_t>(k32BitTestData);
  const uintptr_t swapped_test_data =
      static_cast<uintptr_t>(k32BitSwappedTestData);
#else
#error architecture not supported
#endif

  uintptr_t swapped = base::ByteSwapUintPtrT(test_data);
  EXPECT_EQ(swapped_test_data, swapped);
  uintptr_t reswapped = base::ByteSwapUintPtrT(swapped);
  EXPECT_EQ(test_data, reswapped);
}

TEST(ByteOrderTest, ByteSwapToLE16) {
  uint16_t le = base::ByteSwapToLE16(k16BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k16BitTestData, le);
#else
  EXPECT_EQ(k16BitSwappedTestData, le);
#endif
}

TEST(ByteOrderTest, ByteSwapToLE32) {
  uint32_t le = base::ByteSwapToLE32(k32BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k32BitTestData, le);
#else
  EXPECT_EQ(k32BitSwappedTestData, le);
#endif
}

TEST(ByteOrderTest, ByteSwapToLE64) {
  uint64_t le = base::ByteSwapToLE64(k64BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k64BitTestData, le);
#else
  EXPECT_EQ(k64BitSwappedTestData, le);
#endif
}

TEST(ByteOrderTest, NetToHost16) {
  uint16_t h = base::NetToHost16(k16BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k16BitSwappedTestData, h);
#else
  EXPECT_EQ(k16BitTestData, h);
#endif
}

TEST(ByteOrderTest, NetToHost32) {
  uint32_t h = base::NetToHost32(k32BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k32BitSwappedTestData, h);
#else
  EXPECT_EQ(k32BitTestData, h);
#endif
}

TEST(ByteOrderTest, NetToHost64) {
  uint64_t h = base::NetToHost64(k64BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k64BitSwappedTestData, h);
#else
  EXPECT_EQ(k64BitTestData, h);
#endif
}

TEST(ByteOrderTest, HostToNet16) {
  uint16_t n = base::HostToNet16(k16BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k16BitSwappedTestData, n);
#else
  EXPECT_EQ(k16BitTestData, n);
#endif
}

TEST(ByteOrderTest, HostToNet32) {
  uint32_t n = base::HostToNet32(k32BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k32BitSwappedTestData, n);
#else
  EXPECT_EQ(k32BitTestData, n);
#endif
}

TEST(ByteOrderTest, HostToNet64) {
  uint64_t n = base::HostToNet64(k64BitTestData);
#if defined(ARCH_CPU_LITTLE_ENDIAN)
  EXPECT_EQ(k64BitSwappedTestData, n);
#else
  EXPECT_EQ(k64BitTestData, n);
#endif
}
