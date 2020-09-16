// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/ranges/functional.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(RangesTest, EqualTo) {
  ranges::equal_to eq;
  EXPECT_TRUE(eq(0, 0));
  EXPECT_FALSE(eq(0, 1));
  EXPECT_FALSE(eq(1, 0));
}

TEST(RangesTest, Less) {
  ranges::less lt;
  EXPECT_FALSE(lt(0, 0));
  EXPECT_TRUE(lt(0, 1));
  EXPECT_FALSE(lt(1, 0));
}

}  // namespace base
