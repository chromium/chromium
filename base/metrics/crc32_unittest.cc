// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/crc32.h"

#include <stdint.h>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Table was generated similarly to sample code for CRC-32 given on:
// http://www.w3.org/TR/PNG/#D-CRCAppendix.
TEST(Crc32Test, TableTest) {
  for (int i = 0; i < 256; ++i) {
    uint32_t checksum = i;
    for (int j = 0; j < 8; ++j) {
      const uint32_t kReversedPolynomial = 0xEDB88320L;
      if (checksum & 1)
        checksum = kReversedPolynomial ^ (checksum >> 1);
      else
        checksum >>= 1;
    }
    EXPECT_EQ(kCrcTable[i], checksum);
  }
}

// A CRC of nothing should always be zero.
TEST(Crc32Test, ZeroTest) {
  EXPECT_EQ(0U, Crc32(0, nullptr, 0));
}

}  // namespace base
