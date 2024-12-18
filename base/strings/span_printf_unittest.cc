// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/span_printf.h"

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

TEST(SpanPrintf, Fits) {
  char buf[6];
  EXPECT_EQ(5, SpanPrintf(buf, "x=%d\n", 42));
  EXPECT_THAT(buf, testing::ElementsAre('x', '=', '4', '2', '\n', '\0'));
}

TEST(SpanPrintf, DoesNotFit) {
  char buf[2];
  EXPECT_EQ(5, SpanPrintf(buf, "x=%d\n", 42));
  EXPECT_THAT(buf, testing::ElementsAre('x', '\0'));
}

}  // namespace base
