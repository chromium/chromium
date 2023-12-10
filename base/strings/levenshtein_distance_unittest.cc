// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/levenshtein_distance.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

TEST(LevenshteinDistanceTest, WithoutMaxDistance) {
  EXPECT_EQ(0u, LevenshteinDistance("banana", "banana"));

  EXPECT_EQ(2u, LevenshteinDistance("ab", "ba"));
  EXPECT_EQ(2u, LevenshteinDistance("ba", "ab"));

  EXPECT_EQ(2u, LevenshteinDistance("ananas", "banana"));
  EXPECT_EQ(2u, LevenshteinDistance("banana", "ananas"));

  EXPECT_EQ(2u, LevenshteinDistance("unclear", "nuclear"));
  EXPECT_EQ(2u, LevenshteinDistance("nuclear", "unclear"));

  EXPECT_EQ(3u, LevenshteinDistance("chrome", "chromium"));
  EXPECT_EQ(3u, LevenshteinDistance("chromium", "chrome"));

  EXPECT_EQ(4u, LevenshteinDistance("", "abcd"));
  EXPECT_EQ(4u, LevenshteinDistance("abcd", ""));

  // `std::u16string_view` version.
  EXPECT_EQ(4u, LevenshteinDistance(u"xxx", u"xxxxxxx"));
  EXPECT_EQ(4u, LevenshteinDistance(u"xxxxxxx", u"xxx"));

  EXPECT_EQ(7u, LevenshteinDistance(u"yyy", u"xxxxxxx"));
  EXPECT_EQ(7u, LevenshteinDistance(u"xxxxxxx", u"yyy"));
}

TEST(LevenshteinDistanceTest, WithMaxDistance) {
  EXPECT_EQ(LevenshteinDistance("aa", "aa", 0), 0u);

  EXPECT_EQ(LevenshteinDistance("a", "aa", 1), 1u);
  EXPECT_EQ(LevenshteinDistance("aa", "a", 1), 1u);

  // If k is less than `LevenshteinDistance()`, the function should return k+1.
  EXPECT_EQ(LevenshteinDistance("", "12", 1), 2u);
  EXPECT_EQ(LevenshteinDistance("12", "", 1), 2u);

  EXPECT_EQ(LevenshteinDistance("street", "str.", 1), 2u);
  EXPECT_EQ(LevenshteinDistance("str.", "street", 1), 2u);

  EXPECT_EQ(LevenshteinDistance("asdf", "fdsa", 2), 3u);
  EXPECT_EQ(LevenshteinDistance("fdsa", "asdf", 2), 3u);

  EXPECT_EQ(LevenshteinDistance(std::u16string(100, 'a'),
                                std::u16string(200, 'a'), 50),
            51u);
  EXPECT_EQ(LevenshteinDistance(std::u16string(200, 'a'),
                                std::u16string(100, 'a'), 50),
            51u);
}

}  // namespace

}  // namespace base
