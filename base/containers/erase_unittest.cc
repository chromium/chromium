// Copyright 2021 The Chromium Authors. All rights reserved.
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

template <typename T>
size_t GetSize(const std::forward_list<T>& l) {
  return std::distance(l.begin(), l.end());
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

TEST(Erase, String) {
  const std::pair<std::string, std::string> test_data[] = {
      {"", ""},
      {"abc", "bc"},
      {"abca", "bc"},
  };

  for (auto test_case : test_data) {
    Erase(test_case.first, 'a');
    EXPECT_EQ(test_case.second, test_case.first);
  }

  for (auto test_case : test_data) {
    EraseIf(test_case.first, [](char elem) { return elem < 'b'; });
    EXPECT_EQ(test_case.second, test_case.first);
  }
}

TEST(Erase, String16) {
  std::pair<std::u16string, std::u16string> test_data[] = {
      {std::u16string(), std::u16string()},
      {u"abc", u"bc"},
      {u"abca", u"bc"},
  };

  const std::u16string letters = u"ab";
  for (auto test_case : test_data) {
    Erase(test_case.first, letters[0]);
    EXPECT_EQ(test_case.second, test_case.first);
  }

  for (auto test_case : test_data) {
    EraseIf(test_case.first, [&](short elem) { return elem < letters[1]; });
    EXPECT_EQ(test_case.second, test_case.first);
  }
}

TEST(Erase, Deque) {
  RunEraseTest<std::deque<int>>();
  RunEraseIfTest<std::deque<std::pair<int, int>>>();
}

TEST(Erase, Vector) {
  RunEraseTest<std::vector<int>>();
  RunEraseIfTest<std::vector<std::pair<int, int>>>();
}

TEST(Erase, ForwardList) {
  RunEraseTest<std::forward_list<int>>();
  RunEraseIfTest<std::forward_list<std::pair<int, int>>>();
}

TEST(Erase, List) {
  RunEraseTest<std::list<int>>();
  RunEraseIfTest<std::list<std::pair<int, int>>>();
}

TEST(Erase, Map) {
  RunEraseIfTest<std::map<int, int>>();
  RunEraseIfTest<std::map<int, int, std::greater<>>>();
}

TEST(Erase, Multimap) {
  RunEraseIfTest<std::multimap<int, int>>();
  RunEraseIfTest<std::multimap<int, int, std::greater<>>>();
}

TEST(Erase, Set) {
  RunEraseIfTest<std::set<std::pair<int, int>>>();
  RunEraseIfTest<std::set<std::pair<int, int>, std::greater<>>>();
}

TEST(Erase, Multiset) {
  RunEraseIfTest<std::multiset<std::pair<int, int>>>();
  RunEraseIfTest<std::multiset<std::pair<int, int>, std::greater<>>>();
}

TEST(Erase, UnorderedMap) {
  RunEraseIfTest<std::unordered_map<int, int>>();
  RunEraseIfTest<std::unordered_map<int, int, CustomIntHash>>();
}

TEST(Erase, UnorderedMultimap) {
  RunEraseIfTest<std::unordered_multimap<int, int>>();
  RunEraseIfTest<std::unordered_multimap<int, int, CustomIntHash>>();
}

TEST(Erase, UnorderedSet) {
  RunEraseIfTest<std::unordered_set<std::pair<int, int>, HashByFirst>>();
}

TEST(Erase, UnorderedMultiset) {
  RunEraseIfTest<std::unordered_multiset<std::pair<int, int>, HashByFirst>>();
}

}  // namespace
}  // namespace base
