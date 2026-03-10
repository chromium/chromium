// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/adapters.h"

#include <array>
#include <forward_list>
#include <ranges>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/containers/to_vector.h"
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

  {
    std::vector<int> v;
    static_assert(std::ranges::bidirectional_range<decltype(v)>);
    static_assert(std::ranges::bidirectional_range<
                  decltype(base::RangeAsRvalues(std::move(v)))>);
  }

  {
    std::forward_list<int> l;
    static_assert(!std::ranges::bidirectional_range<decltype(l)>);
    static_assert(!std::ranges::bidirectional_range<
                  decltype(base::RangeAsRvalues(std::move(l)))>);
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
  std::array<int, 3> v = {3, 2, 1};
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

TEST(AdaptersTest, RangeAsRvalues) {
  std::vector<std::unique_ptr<int>> v;
  v.push_back(std::make_unique<int>(1));
  v.push_back(std::make_unique<int>(2));
  v.push_back(std::make_unique<int>(3));

  auto v2 = base::ToVector(base::RangeAsRvalues(std::move(v)));
  EXPECT_EQ(1, *v2[0]);
  EXPECT_EQ(2, *v2[1]);
  EXPECT_EQ(3, *v2[2]);

  // The old vector should be consumed. The standard guarantees that a
  // moved-from std::unique_ptr will be null.
  EXPECT_EQ(nullptr, v[0]);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(nullptr, v[1]);  // NOLINT(bugprone-use-after-move)
  EXPECT_EQ(nullptr, v[2]);  // NOLINT(bugprone-use-after-move)
}

TEST(AdaptersTest, RangeAsRvaluesIsReversible) {
  struct Collection {
    struct Iterator {
      using value_type = std::unique_ptr<int>;
      using pointer = value_type*;
      using reference = value_type&;
      using difference_type = std::ptrdiff_t;

      reference operator*() const { return it.operator*(); }
      pointer operator->() const { return it.operator->(); }

      Iterator& operator++() {
        ++it;
        return *this;
      }
      Iterator operator++(int) {
        Iterator that(*this);
        ++*this;
        return that;
      }

      constexpr bool operator==(const Iterator&) const = default;

      std::vector<std::unique_ptr<int>>::iterator it;
    };

    struct ReverseIterator {
      using value_type = std::unique_ptr<int>;
      using pointer = value_type*;
      using reference = value_type&;
      using difference_type = std::ptrdiff_t;

      reference operator*() const { return it.operator*(); }
      pointer operator->() const { return it.operator->(); }

      ReverseIterator& operator++() {
        ++it;
        return *this;
      }
      ReverseIterator operator++(int) {
        ReverseIterator that(*this);
        ++*this;
        return that;
      }

      constexpr bool operator==(const ReverseIterator&) const = default;

      std::vector<std::unique_ptr<int>>::reverse_iterator it;
    };

    Iterator begin() { return Iterator{.it = underlying.begin()}; }
    Iterator end() { return Iterator{.it = underlying.end()}; }
    ReverseIterator rbegin() {
      return ReverseIterator{.it = underlying.rbegin()};
    }
    ReverseIterator rend() { return ReverseIterator{.it = underlying.rend()}; }

    std::vector<std::unique_ptr<int>> underlying;
  };

  // Even though `Collection` is not a bidirectional range, `base::Reversed(c)`
  // works, so `base::Reversed(base::RangeAsRvalues(std::move(c)))` should also
  // work.
  static_assert(!std::ranges::bidirectional_range<Collection>);
  Collection c;
  c.underlying.push_back(std::make_unique<int>(1));
  c.underlying.push_back(std::make_unique<int>(2));
  c.underlying.push_back(std::make_unique<int>(3));

  auto v2 = base::ToVector(base::Reversed(base::RangeAsRvalues(std::move(c))));
  EXPECT_EQ(3, *v2[0]);
  EXPECT_EQ(2, *v2[1]);
  EXPECT_EQ(1, *v2[2]);
}

}  // namespace
