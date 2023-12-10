// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/cxx20_erase.h"
#include "base/containers/queue.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace {

template <typename Container>
size_t GetSize(const Container& c) {
  return c.size();
}

template <typename Container>
void RunEraseTest() {
  const std::pair<Container, Container> test_data[] = {
      {Container(), Container()}, {{1, 2, 3}, {1, 3}}, {{1, 2, 3, 2}, {1, 3}}};

  for (auto test_case : test_data) {
    size_t expected_erased =
        GetSize(test_case.first) - GetSize(test_case.second);
    EXPECT_EQ(expected_erased, base::Erase(test_case.first, 2));
    EXPECT_EQ(test_case.second, test_case.first);
  }
}

// This test is written for containers of std::pair<int, int> to support maps.
template <typename Container>
void RunEraseIfTest() {
  struct {
    Container input;
    Container erase_even;
    Container erase_odd;
  } test_data[] = {
      {Container(), Container(), Container()},
      {{{1, 1}, {2, 2}, {3, 3}}, {{1, 1}, {3, 3}}, {{2, 2}}},
      {{{1, 1}, {2, 2}, {3, 3}, {4, 4}}, {{1, 1}, {3, 3}}, {{2, 2}, {4, 4}}},
  };

  for (auto test_case : test_data) {
    size_t expected_erased =
        GetSize(test_case.input) - GetSize(test_case.erase_even);
    EXPECT_EQ(expected_erased,
              base::EraseIf(test_case.input, [](const auto& elem) {
                return !(elem.first & 1);
              }));
    EXPECT_EQ(test_case.erase_even, test_case.input);
  }

  for (auto test_case : test_data) {
    size_t expected_erased =
        GetSize(test_case.input) - GetSize(test_case.erase_odd);
    EXPECT_EQ(expected_erased,
              base::EraseIf(test_case.input,
                            [](const auto& elem) { return elem.first & 1; }));
    EXPECT_EQ(test_case.erase_odd, test_case.input);
  }
}

struct CustomIntHash {
  size_t operator()(int elem) const { return std::hash<int>()(elem) + 1; }
};

struct HashByFirst {
  size_t operator()(const std::pair<int, int>& elem) const {
    return std::hash<int>()(elem.first);
  }
};

}  // namespace

namespace base {
namespace {

TEST(Erase, Vector) {
  RunEraseTest<std::vector<int>>();
  RunEraseIfTest<std::vector<std::pair<int, int>>>();
}

TEST(Erase, Map) {
  RunEraseIfTest<std::map<int, int>>();
  RunEraseIfTest<std::map<int, int, std::greater<>>>();
}

}  // namespace
}  // namespace base
