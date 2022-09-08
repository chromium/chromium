// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/auto_reset.h"

#include <utility>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(AutoReset, Move) {
  int value = 10;
  {
    AutoReset<int> resetter1{&value, 20};
    EXPECT_EQ(20, value);
    {
      value = 15;
      AutoReset<int> resetter2 = std::move(resetter1);
      // Moving to a new resetter does not change the value;
      EXPECT_EQ(15, value);
    }
    // Moved-to `resetter2` is out of scoped, and resets to the original value
    // that was in moved-from `resetter1`.
    EXPECT_EQ(10, value);
    value = 105;
  }
  // Moved-from `resetter1` does not reset to anything.
  EXPECT_EQ(105, value);
}

}  // namespace base
