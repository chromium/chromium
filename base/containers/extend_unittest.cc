// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/extend.h"

#include <initializer_list>
#include <type_traits>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using testing::ElementsAre;

struct NonCopyable {
  char c_;
  explicit NonCopyable(char c) : c_(c) {}
  NonCopyable(NonCopyable&&) = default;
  NonCopyable& operator=(NonCopyable&& other) = default;

  NonCopyable(const NonCopyable&) = delete;
  NonCopyable& operator=(const NonCopyable&) = delete;
};

bool operator==(const NonCopyable& a, const NonCopyable& b) {
  return a.c_ == b.c_;
}

static_assert(std::is_move_constructible_v<NonCopyable>, "");
static_assert(!std::is_copy_constructible_v<NonCopyable>, "");

struct CopyableMovable {
  bool copied_;
  char c_;
  explicit CopyableMovable(char c) : copied_(false), c_(c) {}
  CopyableMovable(const CopyableMovable& other) : copied_(true), c_(other.c_) {}

  CopyableMovable& operator=(const CopyableMovable&) = default;
  CopyableMovable(CopyableMovable&&) = default;
  CopyableMovable& operator=(CopyableMovable&& other) = default;
};

bool operator==(const CopyableMovable& a, const CopyableMovable& b) {
  return a.c_ == b.c_;
}

}  // namespace

TEST(ExtendTest, ExtendWithMove) {
  std::vector<NonCopyable> dst;
  for (char c : {'a', 'b', 'c', 'd'})
    dst.emplace_back(c);
  std::vector<NonCopyable> src;
  for (char c : {'e', 'f', 'g'})
    src.emplace_back(c);
  std::vector<NonCopyable> expected;
  for (char c : {'a', 'b', 'c', 'd', 'e', 'f', 'g'})
    expected.emplace_back(c);

  Extend(dst, std::move(src));
  EXPECT_EQ(dst, expected);
  EXPECT_TRUE(src.empty());
}

TEST(ExtendTest, ExtendCopyableWithMove) {
  std::vector<CopyableMovable> dst;
  for (char c : {'a', 'b', 'c', 'd'})
    dst.emplace_back(c);
  std::vector<CopyableMovable> src;
  for (char c : {'e', 'f', 'g'})
    src.emplace_back(c);
  std::vector<CopyableMovable> expected;
  for (char c : {'a', 'b', 'c', 'd', 'e', 'f', 'g'})
    expected.emplace_back(c);

  Extend(dst, std::move(src));
  EXPECT_EQ(dst, expected);
  EXPECT_TRUE(src.empty());
}

TEST(ExtendTest, ExtendWithCopy) {
  std::vector<CopyableMovable> dst;
  for (char c : {'a', 'b', 'c', 'd'})
    dst.emplace_back(c);
  std::vector<CopyableMovable> src;
  for (char c : {'e', 'f', 'g'})
    src.emplace_back(c);
  std::vector<CopyableMovable> expected;
  for (char c : {'a', 'b', 'c', 'd', 'e', 'f', 'g'})
    expected.emplace_back(c);

  EXPECT_FALSE(dst[0].copied_);
  Extend(dst, src);
  EXPECT_EQ(dst, expected);
  EXPECT_FALSE(dst[0].copied_);
  EXPECT_TRUE(dst[dst.size() - 1].copied_);
}

TEST(ExtendTest, ExtendWithSpan) {
  static constexpr uint8_t kRawArray[] = {3, 4, 5};

  static constexpr auto kVectorGenerator = []() -> std::vector<uint8_t> {
    return {9, 10, 11};
  };

  static const std::vector<uint8_t> kConstVector = kVectorGenerator();
  static std::vector<uint8_t> kMutVector = kVectorGenerator();

  std::vector<uint8_t> dst;

  // Selects overload for span<const uint8_t, 3>.
  Extend(dst, span(kRawArray));
  EXPECT_THAT(dst, ElementsAre(3, 4, 5));

  // Selects overload for span<uint8_t, 3>.
  static std::array<uint8_t, 3> kArray = {6, 7, 8};
  Extend(dst, span(kArray));
  EXPECT_THAT(dst, ElementsAre(3, 4, 5, 6, 7, 8));

  // Selects overload for span<const uint8_t, dynamic_extent>.
  Extend(dst, span(kConstVector));
  EXPECT_THAT(dst, ElementsAre(3, 4, 5, 6, 7, 8, 9, 10, 11));

  // Selects overload for span<uint8_t, dynamic_extent>.
  Extend(dst, span(kMutVector));
  EXPECT_THAT(dst, ElementsAre(3, 4, 5, 6, 7, 8, 9, 10, 11, 9, 10, 11));

  // Input is convertible to span.
  Extend(dst, kRawArray);
  EXPECT_THAT(dst,
              ElementsAre(3, 4, 5, 6, 7, 8, 9, 10, 11, 9, 10, 11, 3, 4, 5));
}

}  // namespace base
