// Copyright 2016 The Chromium Authors
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
