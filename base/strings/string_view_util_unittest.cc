// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_view_util.h"

#include <string_view>
#include <type_traits>

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

TEST(SpanTest, AsStringView) {
  {
    constexpr uint8_t kArray[] = {'h', 'e', 'l', 'l', 'o'};
    // Fixed size span.
    auto s = as_string_view(kArray);
    static_assert(std::is_same_v<decltype(s), std::string_view>);
    EXPECT_EQ(s.data(), reinterpret_cast<const char*>(&kArray[0u]));
    EXPECT_EQ(s.size(), std::size(kArray));

    // Dynamic size span.
    auto s2 = as_string_view(span<const uint8_t>(kArray));
    static_assert(std::is_same_v<decltype(s2), std::string_view>);
    EXPECT_EQ(s2.data(), reinterpret_cast<const char*>(&kArray[0u]));
    EXPECT_EQ(s2.size(), std::size(kArray));
  }
  {
    constexpr char kArray[] = {'h', 'e', 'l', 'l', 'o'};
    // Fixed size span.
    auto s = as_string_view(kArray);
    static_assert(std::is_same_v<decltype(s), std::string_view>);
    EXPECT_EQ(s.data(), &kArray[0u]);
    EXPECT_EQ(s.size(), std::size(kArray));

    // Dynamic size span.
    auto s2 = as_string_view(span<const char>(kArray));
    static_assert(std::is_same_v<decltype(s2), std::string_view>);
    EXPECT_EQ(s2.data(), &kArray[0u]);
    EXPECT_EQ(s2.size(), std::size(kArray));
  }
}

}  // namespace base
