// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/utf_string_conversion_utils.h"

#include <string>

#include "base/strings/string_piece.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(UtfStringConversionUtilsTest, CountUnicodeCharacters) {
  struct TestCase {
    std::u16string value;
    size_t limit;
    absl::optional<size_t> count;
  } test_cases[] = {
      {u"", 0, 0},           {u"abc", 1, 1},
      {u"abc", 3, 3},        {u"abc", 0, 0},
      {u"abc", 4, 3},        {u"abc\U0001F4A9", 4, 4},
      {u"\U0001F4A9", 1, 1}, {{1, 0xD801u}, 5, absl::nullopt},
  };
  for (const auto& test_case : test_cases) {
    EXPECT_EQ(CountUnicodeCharacters(test_case.value.data(),
                                     test_case.value.length(), test_case.limit),
              test_case.count);
  }
}

}  // namespace base
