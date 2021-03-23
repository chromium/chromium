// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/strcat.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StrCat, 8Bit) {
  EXPECT_EQ("", StrCat({""}));
  EXPECT_EQ("1", StrCat({"1"}));
  EXPECT_EQ("122", StrCat({"1", "22"}));
  EXPECT_EQ("122333", StrCat({"1", "22", "333"}));
  EXPECT_EQ("1223334444", StrCat({"1", "22", "333", "4444"}));
  EXPECT_EQ("122333444455555", StrCat({"1", "22", "333", "4444", "55555"}));
}

TEST(StrCat, 16Bit) {
  std::u16string arg1 = u"1";
  std::u16string arg2 = u"22";
  std::u16string arg3 = u"333";

  EXPECT_EQ(u"", StrCat({std::u16string()}));
  EXPECT_EQ(u"1", StrCat({arg1}));
  EXPECT_EQ(u"122", StrCat({arg1, arg2}));
  EXPECT_EQ(u"122333", StrCat({arg1, arg2, arg3}));
}

TEST(StrAppend, 8Bit) {
  std::string result;

  result = "foo";
  StrAppend(&result, {std::string()});
  EXPECT_EQ("foo", result);

  result = "foo";
  StrAppend(&result, {"1"});
  EXPECT_EQ("foo1", result);

  result = "foo";
  StrAppend(&result, {"1", "22", "333"});
  EXPECT_EQ("foo122333", result);
}

TEST(StrAppend, 16Bit) {
  std::u16string arg1 = u"1";
  std::u16string arg2 = u"22";
  std::u16string arg3 = u"333";

  std::u16string result;

  result = u"foo";
  StrAppend(&result, {std::u16string()});
  EXPECT_EQ(u"foo", result);

  result = u"foo";
  StrAppend(&result, {arg1});
  EXPECT_EQ(u"foo1", result);

  result = u"foo";
  StrAppend(&result, {arg1, arg2, arg3});
  EXPECT_EQ(u"foo122333", result);
}

TEST(StrAppendT, ReserveAdditionalIfNeeded) {
  std::string str = "foo";
  const char* prev_data = str.data();
  size_t prev_capacity = str.capacity();
  // Fully exhaust current capacity.
  StrAppend(&str, {std::string(str.capacity() - str.size(), 'o')});
  // Expect that we hit capacity, but didn't require a re-alloc.
  EXPECT_EQ(str.capacity(), str.size());
  EXPECT_EQ(prev_data, str.data());
  EXPECT_EQ(prev_capacity, str.capacity());

  // Force a re-alloc by appending another character.
  StrAppend(&str, {"o"});

  // Expect at least 2x growth in capacity.
  EXPECT_LE(2 * prev_capacity, str.capacity());
}

}  // namespace base
