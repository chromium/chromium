// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/internal/immutable_string.h"

#include <string_view>
#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace base::i18n_internal {

TEST(ImmutableStringTest, EmptyStringConstructor) {
  constexpr ImmutableString str;
  EXPECT_EQ(str.AsString(), "");
}

TEST(ImmutableStringTest, DefaultConstructor) {
  ImmutableString str;
  EXPECT_EQ(str.AsString(), "");
}

TEST(ImmutableStringTest, ConstevalConstructorEmpty) {
  constexpr ImmutableString str =
      ImmutableString(ImmutableString::ForceStackString(), {""});
  EXPECT_EQ(str.AsString(), "");
}

TEST(ImmutableStringTest, ConstevalConstructorSmall) {
  constexpr ImmutableString str =
      ImmutableString(ImmutableString::ForceStackString(), {"hello"});
  EXPECT_EQ(str.AsString(), "hello");
}

TEST(ImmutableStringTest, ConstevalConstructorWithOtherConstant) {
  constexpr std::string_view kStr = "hello";
  constexpr ImmutableString str =
      ImmutableString(ImmutableString::ForceStackString(), {kStr});
  EXPECT_EQ(str.AsString(), "hello");
}

TEST(ImmutableStringTest, ConstevalConstructorMaxSmallSize) {
  ImmutableString str({"123456789012"});
  EXPECT_EQ(str.AsString(), "123456789012");
}

TEST(ImmutableStringTest, JoinPartsSmall) {
  std::string_view parts[] = {"hello", " ", "world"};
  ImmutableString str(parts);
  EXPECT_EQ(str.AsString(), "hello world");
}

TEST(ImmutableStringTest, JoinPartsLarge) {
  std::string_view parts[] = {"this",        " is ",    "a ",    "much ",
                              "longer ",     "string ", "that ", "will ",
                              "definitely ", "exceed ", "the ",  "small ",
                              "stack ",      "buffer ", "limit"};
  ImmutableString str(parts);
  EXPECT_EQ(str.AsString(),
            "this is a much longer string that will definitely exceed the "
            "small stack buffer limit");
}

TEST(ImmutableStringTest, CopyAndMove) {
  constexpr ImmutableString str1 =
      ImmutableString(ImmutableString::ForceStackString(), {"test"});
  ImmutableString str2(str1);
  EXPECT_EQ(str2.AsString(), "test");

  ImmutableString str3 = std::move(str1);
  EXPECT_EQ(str3.AsString(), "test");
}

}  // namespace base::i18n_internal
