// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/types/zip.h"

#include <iostream>
#include <iterator>
#include <vector>

#include "base/test/gtest_util.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {
// This is a type that has a different iterator type for its begin/end.
template <typename T>
class VectorWithCustomIterators {
 public:
  class Iterator {
   public:
    using iterator_category = std::forward_iterator_tag;
    using value_type = T;
    using difference_type = std::ptrdiff_t;
    using pointer = T*;
    using reference = T&;

    explicit Iterator(typename std::vector<T>::iterator it) : it_(it) {}
    reference operator*() const { return *it_; }
    Iterator& operator++() {
      ++it_;
      return *this;
    }
    Iterator operator++(int) {
      Iterator temp = *this;
      ++it_;
      return temp;
    }
    bool operator!=(const typename std::vector<T>::iterator& other) const {
      return it_ != other;
    }

    bool operator==(const typename std::vector<T>::iterator& other) const {
      return it_ == other;
    }

   private:
    typename std::vector<T>::iterator it_;
  };
  explicit VectorWithCustomIterators(const std::vector<T>& data)
      : data_(data) {}
  auto begin() { return Iterator(data_.begin()); }
  auto end() { return data_.end(); }
  const auto& data() const { return data_; }

 private:
  std::vector<T> data_;
};

}  // namespace

TEST(ZipTest, Basics) {
  std::vector<int> a = {1, 2, 3};
  std::vector<double> b = {4.5, 5.5, 6.5};
  std::vector<std::string> c = {"x", "y", "z"};

  size_t index = 0;
  for (auto [x, y, z] : zip(a, b, c)) {
    EXPECT_EQ(a[index], x);
    EXPECT_EQ(b[index], y);
    EXPECT_EQ(c[index], z);
    ++index;
  }
}

TEST(ZipTest, DifferentBeginEndIterators) {
  auto a = VectorWithCustomIterators(std::vector<int>({1, 2, 3}));
  std::vector<double> b = {4.5, 5.5, 6.5};
  std::vector<std::string> c = {"x", "y", "z"};

  size_t index = 0;
  for (auto [x, y, z] : zip(a, b, c)) {
    EXPECT_EQ(a.data()[index], x);
    EXPECT_EQ(b[index], y);
    EXPECT_EQ(c[index], z);
    ++index;
  }
}

TEST(ZipTest, WithCommonArrays) {
  const int a[] = {1, 2, 3};
  const double b[] = {4.5, 5.5, 6.5};
  const char* c[] = {"x", "y", "z"};

  size_t index = 0;
  for (auto [x, y, z] : zip(a, b, c)) {
    // SAFETY: Unsafe buffer access to demonstrate that the test is correct in
    // concept.
    EXPECT_EQ(UNSAFE_BUFFERS(a[index]), x);
    EXPECT_EQ(UNSAFE_BUFFERS(b[index]), y);
    EXPECT_EQ(UNSAFE_BUFFERS(c[index]), z);
    ++index;
  }
}

TEST(ZipTest, MutatingThroughZip) {
  std::vector<int> a = {1, 2, 3};
  std::vector<int> b = {4, 5, 6};
  std::vector<int> c = {7, 8, 9};

  for (auto [x, y, z] : zip(a, b, c)) {
    x *= 10;
    y *= 10;
    z *= 10;
  }

  EXPECT_EQ(a, std::vector<int>({10, 20, 30}));
  EXPECT_EQ(b, std::vector<int>({40, 50, 60}));
  EXPECT_EQ(c, std::vector<int>({70, 80, 90}));
}

TEST(ZipTest, BailOutAsOnMinimumSize) {
  std::vector<int> a = {7, 8, 9};
  std::vector<int> b = {4, 5};
  std::vector<int> c = {1, 2, 3};

  for (auto [x, y, z] : zip(a, b, c)) {
    x *= 10;
    y *= 10;
    z *= 10;
  }

  EXPECT_EQ(a, std::vector<int>({70, 80, 9}));
  EXPECT_EQ(b, std::vector<int>({40, 50}));
  EXPECT_EQ(c, std::vector<int>({10, 20, 3}));
}

TEST(ZipTest, EmptyZip) {
  std::vector<int> a = {};
  std::vector<int> b = {};
  std::vector<int> c = {};

  for (auto [x, y, z] : zip(a, b, c)) {
    x *= 10;
    y *= 10;
    z *= 10;
  }

  EXPECT_TRUE(a.empty());
  EXPECT_TRUE(b.empty());
  EXPECT_TRUE(c.empty());

  b.push_back(1);

  for (auto [x, y, z] : zip(a, b, c)) {
    x *= 10;
    y *= 10;
    z *= 10;
  }

  EXPECT_TRUE(a.empty());
  EXPECT_EQ(b, std::vector<int>({1}));
  EXPECT_TRUE(c.empty());
}

TEST(ZipTest, NotCopyableRange) {
  struct NotCopyable {
    explicit NotCopyable(int x) : value(x) {}
    NotCopyable(const NotCopyable&) = delete;
    NotCopyable& operator=(const NotCopyable&) = delete;
    NotCopyable(NotCopyable&& other) = default;
    NotCopyable& operator=(NotCopyable&& other) = default;
    int value;
  };

  auto a = std::to_array<NotCopyable>({NotCopyable{10}, NotCopyable{10}});
  auto b = std::to_array<NotCopyable>({NotCopyable{20}, NotCopyable{20}});
  auto c = std::to_array<NotCopyable>({NotCopyable{30}, NotCopyable{30}});
  for (auto [x, y, z] : zip(a, b, c)) {
    EXPECT_EQ(10, x.value);
    EXPECT_EQ(20, y.value);
    EXPECT_EQ(30, z.value);
  }
}

TEST(ZipTest, NotCopyableOrMovableRange) {
  struct NotCopyableOrMovable {
    explicit NotCopyableOrMovable(int x) : value(x) {}
    NotCopyableOrMovable(const NotCopyableOrMovable&) = delete;
    NotCopyableOrMovable& operator=(const NotCopyableOrMovable&) = delete;
    NotCopyableOrMovable(NotCopyableOrMovable&& other) = default;
    NotCopyableOrMovable& operator=(NotCopyableOrMovable&& other) = delete;
    int value;
  };

  NotCopyableOrMovable a[] = {NotCopyableOrMovable{10},
                              NotCopyableOrMovable{10}};
  NotCopyableOrMovable b[] = {NotCopyableOrMovable{20},
                              NotCopyableOrMovable{20}};
  NotCopyableOrMovable c[] = {NotCopyableOrMovable{30},
                              NotCopyableOrMovable{30}};
  for (auto [x, y, z] : zip(a, b, c)) {
    EXPECT_EQ(10, x.value);
    EXPECT_EQ(20, y.value);
    EXPECT_EQ(30, z.value);
  }
}

TEST(ZipTest, CheckForIterationPastTheEnd) {
  std::vector<int> a = {7, 8, 9};
  std::vector<int> b = {4, 5};

  auto ranges = zip(a, b);
  auto it = ranges.begin();
  std::advance(it, 2);
  EXPECT_DCHECK_DEATH(std::advance(it, 1));
}

}  // namespace base
