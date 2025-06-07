// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_view_util.h"

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

// Tests that MakeStringViewWithNulChars preserves internal NUL characters.
TEST(StringViewUtilTest, MakeStringViewWithNulChars) {
  {
    const char kTestString[] = "abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_EQ(base::span(s), base::span_from_cstring(kTestString));
  }
  {
    const wchar_t kTestString[] = L"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    ASSERT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
  {
    const char16_t kTestString[] = u"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
  {
    const char32_t kTestString[] = U"abd\0def";
    auto s = MakeStringViewWithNulChars(kTestString);
    EXPECT_EQ(s.size(), 7u);
    EXPECT_TRUE(base::span(s) == base::span_from_cstring(kTestString));
  }
}

}  // namespace base
