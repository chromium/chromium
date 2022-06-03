// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#import <Foundation/Foundation.h>

#include <string>

#include "base/strings/sys_string_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SysStrings, ConversionsFromNSString) {
  EXPECT_STREQ("Hello, world!", SysNSStringToUTF8(@"Hello, world!").c_str());

  // Conversions should be able to handle a NULL value without crashing.
  EXPECT_STREQ("", SysNSStringToUTF8(nil).c_str());
  EXPECT_EQ(std::u16string(), SysNSStringToUTF16(nil));
}

std::vector<std::string> GetRoundTripStrings() {
  return {
      "Hello, World!",  // ASCII / ISO8859 string (also valid UTF-8)
      "a\0b",           // UTF-8 with embedded NUL byte
      "λf",             // lowercase lambda + 'f'
      "χρώμιο",         // "chromium" in greek
      "כרום",           // "chromium" in hebrew
      "クロム",         // "chromium" in japanese

      // Tarot card symbol "the morning", which does not fit in one UTF-16
      // character.
      "🃦",
  };
}

TEST(SysStrings, RoundTripsFromUTF8) {
  for (const auto& string8 : GetRoundTripStrings()) {
    NSString* nsstring8 = SysUTF8ToNSString(string8);
    std::string back8 = SysNSStringToUTF8(nsstring8);
    EXPECT_EQ(string8, back8);
  }
}

TEST(SysStrings, RoundTripsFromUTF16) {
  for (const auto& string8 : GetRoundTripStrings()) {
    std::u16string string16 = base::UTF8ToUTF16(string8);
    NSString* nsstring16 = SysUTF16ToNSString(string16);
    std::u16string back16 = SysNSStringToUTF16(nsstring16);
    EXPECT_EQ(string16, back16);
  }
}

}  // namespace base
