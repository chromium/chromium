// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/span_reader.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace {

TEST(SpanReaderTest, Construct) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  EXPECT_EQ(r.remaining(), 5u);
  EXPECT_EQ(r.remaining_span().data(), &kArray[0u]);
  EXPECT_EQ(r.remaining_span().size(), 5u);
}

TEST(SpanReaderTest, Read) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read(5u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    EXPECT_FALSE(o.has_value());
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read(1u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    auto o = r.Read(2u);
    static_assert(std::same_as<decltype(*o), span<const int>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadFixed) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read<5u>();
    static_assert(std::same_as<decltype(*o), span<const int, 5u>&>);
    EXPECT_FALSE(o.has_value());
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    auto o = r.Read<1u>();
    static_assert(std::same_as<decltype(*o), span<const int, 1u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    auto o = r.Read<2u>();
    static_assert(std::same_as<decltype(*o), span<const int, 2u>&>);
    ASSERT_TRUE(o.has_value());
    EXPECT_TRUE(*o == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

TEST(SpanReaderTest, ReadInto) {
  std::array<const int, 5u> kArray = {1, 2, 3, 4, 5};

  auto r = SpanReader(base::span(kArray));
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(0u, 2u));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    base::span<const int> s;
    EXPECT_FALSE(r.ReadInto(5u, s));
    EXPECT_EQ(r.remaining(), 3u);
  }
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(1u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(2u, 1u));
    EXPECT_EQ(r.remaining(), 2u);
  }
  {
    base::span<const int> s;
    EXPECT_TRUE(r.ReadInto(2u, s));
    EXPECT_TRUE(s == base::span(kArray).subspan(3u, 2u));
    EXPECT_EQ(r.remaining(), 0u);
  }
}

}  // namespace
}  // namespace base
