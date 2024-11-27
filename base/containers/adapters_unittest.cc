// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/adapters.h"

#include <ranges>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

class UnsizedVector {
 public:
  UnsizedVector() = default;
  UnsizedVector(std::initializer_list<int> il) : v_(il) {}

  // Sentinel iterator type that wraps `end()` to ensure this type doesn't
  // satisfy the `sized_range` concept.
  template <typename Iterator>
  class Sentinel {
   public:
    Sentinel() = default;
    explicit Sentinel(Iterator it) : wrapped_it_(it) {}

    bool operator==(Iterator it) const { return wrapped_it_ == it; }

   private:
    Iterator wrapped_it_;
  };

  auto begin() const { return v_.begin(); }
  auto end() const { return Sentinel(v_.end()); }
  auto rbegin() const { return v_.rbegin(); }
  auto rend() const { return Sentinel(v_.rend()); }
  auto begin() { return v_.begin(); }
  auto end() { return Sentinel(v_.end()); }
  auto rbegin() { return v_.rbegin(); }
  auto rend() { return Sentinel(v_.rend()); }

  auto operator[](size_t pos) const { return v_[pos]; }
  auto operator[](size_t pos) { return v_[pos]; }

 private:
  std::vector<int> v_;
};

[[maybe_unused]] void StaticAsserts() {
  {
    // Named local variables are more readable than std::declval<T>().
    std::vector<int> v;
    static_assert(std::ranges::range<decltype(base::Reversed(v))>);
    static_assert(std::ranges::sized_range<decltype(base::Reversed(v))>);
    // `base::Reversed()` takes a const ref to the vector, which is, by
    // definition, a borrowed range.
    static_assert(std::ranges::borrowed_range<decltype(base::Reversed(v))>);

    auto make_vector = [] { return std::vector<int>(); };
    static_assert(std::ranges::range<decltype(base::Reversed(make_vector()))>);
    static_assert(
        std::ranges::sized_range<decltype(base::Reversed(make_vector()))>);
    static_assert(
        !std::ranges::borrowed_range<decltype(base::Reversed(make_vector()))>);
  }

  {
    base::span<int> s;
    static_assert(std::ranges::range<decltype(base::Reversed(s))>);
    static_assert(std::ranges::sized_range<decltype(base::Reversed(s))>);
    static_assert(std::ranges::borrowed_range<decltype(base::Reversed(s))>);

    auto rvalue_span = [] { return base::span<int>(); };
    static_assert(std::ranges::range<decltype(base::Reversed(rvalue_span()))>);
    static_assert(
        std::ranges::sized_range<decltype(base::Reversed(rvalue_span()))>);
    static_assert(
        std::ranges::borrowed_range<decltype(base::Reversed(rvalue_span()))>);
  }

  {
    // A named local variable is more readable than std::declval<T>().
    UnsizedVector v;
    static_assert(std::ranges::range<decltype(v)>);
    static_assert(!std::ranges::sized_range<decltype(v)>);

    static_assert(std::ranges::range<decltype(base::Reversed(v))>);
    static_assert(!std::ranges::sized_range<decltype(base::Reversed(v))>);
    // `base::Reversed()` takes a const ref to the vector, which is, by
    // definition, a borrowed range.
    static_assert(std::ranges::borrowed_range<decltype(base::Reversed(v))>);

    auto make_vector = [] { return UnsizedVector(); };
    static_assert(std::ranges::range<decltype(base::Reversed(make_vector()))>);
    static_assert(
        !std::ranges::sized_range<decltype(base::Reversed(make_vector()))>);
    static_assert(!std::ranges::borrowed_range<decltype(make_vector())>);
  }
}

TEST(AdaptersTest, Reversed) {
  std::vector<int> v = {3, 2, 1};
  int j = 0;
  for (int& i : base::Reversed(v)) {
    EXPECT_EQ(++j, i);
    i += 100;
  }
  EXPECT_EQ(103, v[0]);
  EXPECT_EQ(102, v[1]);
  EXPECT_EQ(101, v[2]);
}

TEST(AdaptersTest, ReversedUnsized) {
  UnsizedVector v = {3, 2, 1};
  int j = 0;
  for (int& i : base::Reversed(v)) {
    EXPECT_EQ(++j, i);
    i += 100;
  }
  EXPECT_EQ(103, v[0]);
  EXPECT_EQ(102, v[1]);
  EXPECT_EQ(101, v[2]);
}

TEST(AdaptersTest, ReversedArray) {
  int v[3] = {3, 2, 1};
  int j = 0;
  for (int& i : base::Reversed(v)) {
    EXPECT_EQ(++j, i);
    i += 100;
  }
  EXPECT_EQ(103, v[0]);
  EXPECT_EQ(102, v[1]);
  EXPECT_EQ(101, v[2]);
}

TEST(AdaptersTest, ReversedConst) {
  std::vector<int> v = {3, 2, 1};
  const std::vector<int>& cv = v;
  int j = 0;
  for (int i : base::Reversed(cv)) {
    EXPECT_EQ(++j, i);
  }
}

}  // namespace
