// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/contains.h"

#include <set>
#include <string>

#include "base/containers/flat_set.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(ContainsTest, GenericContains) {
  const char allowed_chars[] = {'a', 'b', 'c', 'd'};

  EXPECT_TRUE(Contains(allowed_chars, 'a'));
  EXPECT_FALSE(Contains(allowed_chars, 'z'));
  EXPECT_FALSE(Contains(allowed_chars, 0));

  const char allowed_chars_including_nul[] = "abcd";
  EXPECT_TRUE(Contains(allowed_chars_including_nul, 0));
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
