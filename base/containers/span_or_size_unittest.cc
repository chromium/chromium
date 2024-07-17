// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_or_size.h"

#include <vector>

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(SpanOrSizeTest, Size) {
  SpanOrSize<int> s(123u);

  EXPECT_FALSE(s.span().has_value());
  EXPECT_EQ(s.size(), 123u);
  EXPECT_EQ(s.ptr_or_null_if_no_data(), nullptr);
}

TEST(SpanOrSizeTest, Span) {
  std::vector<int> v{1, 2, 3};
  SpanOrSize<int> s{base::span<int>(v)};

  EXPECT_TRUE(s.span().has_value());
  EXPECT_EQ(s.span()->data(), v.data());
  EXPECT_EQ(s.span()->size(), 3u);

  EXPECT_EQ(s.size(), 3u);
  EXPECT_EQ(s.ptr_or_null_if_no_data(), v.data());
}

TEST(SpanOrSizeTest, SpanDeductionGuide) {
  std::vector<int> v{1, 42, 3};

  // MAIN TEST: No need to spell out `SpanOrSize<int>` below
  // (IIUC thanks to an implicit deduction guide inferred by the compiler).
  auto s = SpanOrSize(base::span(v));

  EXPECT_EQ(s.span().value()[1], 42);
}

}  // namespace
}  // namespace base
