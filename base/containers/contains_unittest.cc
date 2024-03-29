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

TEST(ContainsTest, GenericSetContainsWithProjection) {
  constexpr std::string_view kFoo = "foo";
  std::set<std::string> set = {"foo", "bar", "baz"};

  // Opt into a linear search by explicitly providing a projection:
  EXPECT_TRUE(Contains(set, kFoo, std::identity{}));
}

TEST(ContainsTest, ContainsWithFindAndNpos) {
  std::string str = "abcd";

  EXPECT_TRUE(Contains(str, 'a'));
  EXPECT_FALSE(Contains(str, 'z'));
  EXPECT_FALSE(Contains(str, 0));
}

TEST(ContainsTest, ContainsWithFindAndEnd) {
  std::set<int> set = {1, 2, 3, 4};

  EXPECT_TRUE(Contains(set, 1));
  EXPECT_FALSE(Contains(set, 5));
  EXPECT_FALSE(Contains(set, 0));
}

TEST(ContainsTest, ContainsWithContains) {
  flat_set<int> set = {1, 2, 3, 4};

  EXPECT_TRUE(Contains(set, 1));
  EXPECT_FALSE(Contains(set, 5));
  EXPECT_FALSE(Contains(set, 0));
}

}  // namespace base
