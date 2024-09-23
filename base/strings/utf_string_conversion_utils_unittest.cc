// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversion_utils.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(UtfStringConversionUtilsTest, CountUnicodeCharacters) {
  const struct TestCase {
    std::string value;
    size_t limit;
    std::optional<size_t> count;
  } test_cases[] = {
      {"", 0, 0},
      {"abc", 1, 1},
      {"abc", 3, 3},
      {"abc", 0, 0},
      {"abc", 4, 3},
      // The casts and u8 string literals are needed here so that we don't
      // trigger linter errors about invalid ascii values.
      {reinterpret_cast<const char*>(u8"abc\U0001F4A9"), 4, 4},
      {reinterpret_cast<const char*>(u8"\U0001F4A9"), 1, 1},
      {{1, static_cast<char>(-1)}, 5, std::nullopt},
  };
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(CountUnicodeCharacters(test_case.value, test_case.limit),
              test_case.count);
  }
}

}  // namespace base
