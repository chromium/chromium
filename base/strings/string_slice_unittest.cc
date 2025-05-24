// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_slice.h"

#include <stdint.h>

#include <concepts>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using subtle::StringSlice;

TEST(StringSliceTest, IndexType) {
  {
    static char kData[255] = {};
    using Slice = StringSlice<sizeof(kData), kData>;
    static_assert(std::same_as<Slice::IndexType, uint8_t>);
  }

  {
    static char kData[256] = {};
    using Slice = StringSlice<sizeof(kData), kData>;
    static_assert(std::same_as<Slice::IndexType, uint16_t>);
  }

  {
    static char kData[65535] = {};
    using Slice = StringSlice<sizeof(kData), kData>;
    static_assert(std::same_as<Slice::IndexType, uint16_t>);
  }

  {
    static char kData[65536] = {};
    using Slice = StringSlice<sizeof(kData), kData>;
    static_assert(std::same_as<Slice::IndexType, uint32_t>);
  }
}

}  // namespace

}  // namespace base
