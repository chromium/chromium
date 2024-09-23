// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/metrics/crc32.h"

#include <stdint.h>

#include <array>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(Crc32Test, ZeroTest) {
  span<const uint8_t> empty_data;
  EXPECT_EQ(0U, Crc32(0, empty_data));
}

TEST(Crc32Test, EmptyNonzeroTest) {
  span<const uint8_t> empty_data;
  EXPECT_EQ(99U, Crc32(99, empty_data));
}

TEST(Crc32Test, NonemptyTest) {
  std::array<uint8_t, 5> arr = {1, 2, 3, 4, 5};
  EXPECT_EQ(0x81296ee9U, Crc32(0, arr));
}

}  // namespace base
