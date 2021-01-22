// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string16.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Ensure that STRING16_LITERAL can be used to instantiate constants of type
// char16 and char16[], respectively.
TEST(String16Test, String16Literal) {
  static constexpr char16 kHelloChars[] = {
      STRING16_LITERAL('H'), STRING16_LITERAL('e'), STRING16_LITERAL('l'),
      STRING16_LITERAL('l'), STRING16_LITERAL('o'), STRING16_LITERAL('\0'),
  };

  static constexpr char16 kHelloStr[] = STRING16_LITERAL("Hello");
  EXPECT_EQ(std::char_traits<char16>::compare(kHelloChars, kHelloStr, 6), 0);
}

}  // namespace base
