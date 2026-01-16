// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"

#include <functional>
#include <set>
#include <string>
#include <string_view>

#include "base/containers/flat_set.h"
#include "base/strings/string_util.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ContainsTest, GenericContains) {
  constexpr char allowed_chars[] = {'a', 'b', 'c', 'd'};

  static_assert(Contains(allowed_chars, 'a'), "");
  static_assert(!Contains(allowed_chars, 'z'), "");
  static_assert(!Contains(allowed_chars, 0), "");

  constexpr char allowed_chars_including_nul[] = "abcd";
  static_assert(Contains(allowed_chars_including_nul, 0), "");
}

TEST(ContainsTest, GenericContainsWithProjection) {
  const char allowed_chars[] = {'A', 'B', 'C', 'D'};

  EXPECT_TRUE(Contains(allowed_chars, 'a', &ToLowerASCII<char>));
  EXPECT_FALSE(Contains(allowed_chars, 'z', &ToLowerASCII<char>));
  EXPECT_FALSE(Contains(allowed_chars, 0, &ToLowerASCII<char>));
}

}  // namespace base
