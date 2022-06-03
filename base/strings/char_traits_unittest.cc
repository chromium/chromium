// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/char_traits.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(CharTraitsTest, CharCompare) {
  static_assert(CharTraits<char>::compare("abc", "def", 3) == -1, "");
  static_assert(CharTraits<char>::compare("def", "def", 3) == 0, "");
  static_assert(CharTraits<char>::compare("ghi", "def", 3) == 1, "");
}

TEST(CharTraitsTest, CharLength) {
  static_assert(CharTraits<char>::length("") == 0, "");
  static_assert(CharTraits<char>::length("abc") == 3, "");
}

TEST(CharTraitsTest, Char16TCompare) {
  static_assert(CharTraits<char16_t>::compare(u"abc", u"def", 3) == -1, "");
  static_assert(CharTraits<char16_t>::compare(u"def", u"def", 3) == 0, "");
  static_assert(CharTraits<char16_t>::compare(u"ghi", u"def", 3) == 1, "");
}

TEST(CharTraitsTest, Char16TLength) {
  static_assert(CharTraits<char16_t>::length(u"abc") == 3, "");
}

}  // namespace base
