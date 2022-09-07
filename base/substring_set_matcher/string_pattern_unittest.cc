// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/substring_set_matcher/matcher_string_pattern.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(MatcherStringPatternTest, MatcherStringPattern) {
  MatcherStringPattern r1("Test", 2);
  EXPECT_EQ("Test", r1.pattern());
  EXPECT_EQ(2u, r1.id());

  EXPECT_FALSE(r1 < r1);
  MatcherStringPattern r2("Test", 3);
  EXPECT_TRUE(r1 < r2);
  MatcherStringPattern r3("ZZZZ", 2);
  EXPECT_TRUE(r1 < r3);
}

}  // namespace base
