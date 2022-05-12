// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/substring_set_matcher/string_pattern.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(StringPatternTest, StringPattern) {
  StringPattern r1("Test", 2);
  EXPECT_EQ("Test", r1.pattern());
  EXPECT_EQ(2, r1.id());

  EXPECT_FALSE(r1 < r1);
  StringPattern r2("Test", 3);
  EXPECT_TRUE(r1 < r2);
  StringPattern r3("ZZZZ", 2);
  EXPECT_TRUE(r1 < r3);
}

}  // namespace base
