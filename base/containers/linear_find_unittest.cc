// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/linear_find.h"

#include <array>
#include <concepts>
#include <list>
#include <map>
#include <memory>
#include <ranges>
#include <set>
#include <string>
#include <string_view>
#include <utility>
#include <vector>

#include "base/compiler_specific.h"
#include "base/containers/span.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

using ::testing::Pointee;

struct Entry {
  int id = 0;
  std::string name;

  constexpr int GetId() const { return id; }
  friend bool operator==(const Entry&, const Entry&) = default;
};

TEST(LinearFindTest, RangeReturnsElement) {
  std::vector<int> vec = {10, 20, 30};
  const auto& const_vec = vec;
  static_assert(std::same_as<decltype(LinearFind(vec, 20)), int*>);
  static_assert(std::same_as<decltype(LinearFind(const_vec, 20)), const int*>);

  int* found = LinearFind(vec, 20);
  ASSERT_NE(found, nullptr);
  *found = 25;
  EXPECT_EQ(LinearFind(const_vec, 25), &vec[1]);
  EXPECT_EQ(LinearFind(const_vec, 20), nullptr);

  std::array<int, 3> arr = {1, 2, 3};
  EXPECT_EQ(LinearFind(arr, 2), &arr[1]);

  std::list<std::string> list = {"a", "b"};
  EXPECT_THAT(LinearFind(list, "b"), Pointee(std::string("b")));
  EXPECT_EQ(LinearFind(list, "c"), nullptr);

  // Borrowed views, including temporaries.
  EXPECT_EQ(LinearFind(span(vec), 30), &vec[2]);
  EXPECT_EQ(LinearFind(span(const_vec), 99), nullptr);
  EXPECT_EQ(LinearFind(std::ranges::subrange(vec), 30), &vec[2]);

  std::string_view sv = "hello";
  static_assert(std::same_as<decltype(LinearFind(sv, 'e')), const char*>);
  EXPECT_EQ(LinearFind(sv, 'e'), &sv[1]);
  EXPECT_EQ(LinearFind(sv, 'z'), nullptr);
}

TEST(LinearFindTest, ProjectionReturnsProjectedMatch) {
  const std::vector<Entry> entries = {{.id = 1, .name = "one"},
                                      {.id = 2, .name = "two"},
                                      {.id = 3, .name = "three"}};

  EXPECT_EQ(LinearFind(entries, 2, &Entry::id), &entries[1]);
  EXPECT_EQ(LinearFind(entries, 3, &Entry::GetId), &entries[2]);
  EXPECT_EQ(
      LinearFind(entries, 5u, [](const Entry& e) { return e.name.size(); }),
      &entries[2]);
  EXPECT_EQ(LinearFind(entries, 99, &Entry::id), nullptr);

  // Braced-init-lists are the projected type.
  EXPECT_EQ(LinearFind(entries, {.id = 1, .name = "one"}), &entries[0]);
  EXPECT_EQ(LinearFind(entries, {.id = 1, .name = "wrong"}), nullptr);
  std::vector<std::pair<std::pair<int, int>, int>> pairs = {{{1, 2}, 3}};
  EXPECT_EQ(
      LinearFind(pairs, {1, 2}, &std::pair<std::pair<int, int>, int>::first),
      &pairs[0]);

  // Projections see through smart pointers.
  std::vector<std::unique_ptr<Entry>> unique_ptrs;
  unique_ptrs.push_back(std::make_unique<Entry>(10, "ten"));
  EXPECT_EQ(LinearFind(unique_ptrs, 10, &Entry::id), &unique_ptrs[0]);
  EXPECT_EQ(LinearFind(unique_ptrs, 20, &Entry::id), nullptr);
}

TEST(LinearFindTest, MapReturnsKeyValuePair) {
  std::map<int, std::string> map = {{1, "one"}, {2, "two"}};
  using Pair = std::map<int, std::string>::value_type;
  static_assert(
      std::same_as<decltype(LinearFind(map, "two", &Pair::second)), Pair*>);
  EXPECT_EQ(LinearFind(map, "two", &Pair::second), &*map.find(2));
  EXPECT_EQ(LinearFind(map, "three", &Pair::second), nullptr);
}

TEST(LinearFindTest, FindIfReturnsMatchingElement) {
  std::vector<Entry> entries = {{.id = 1, .name = "alpha"},
                                {.id = 2, .name = "beta"}};
  const auto& const_entries = entries;

  Entry* found =
      LinearFindIf(entries, [](const Entry& e) { return e.id == 2; });
  ASSERT_NE(found, nullptr);
  found->name = "BETA";
  EXPECT_EQ(entries[1].name, "BETA");
  static_assert(
      std::same_as<decltype(LinearFindIf(const_entries,
                                         [](const Entry&) { return true; })),
                   const Entry*>);
  EXPECT_EQ(LinearFindIf(
                const_entries, [](int id) { return id > 1; }, &Entry::id),
            &entries[1]);
  EXPECT_EQ(LinearFindIf(span(entries), [](const Entry&) { return false; }),
            nullptr);

  std::set<int> set = {10, 20, 30};
  EXPECT_THAT(LinearFindIf(set, [](int x) { return x > 15; }), Pointee(20));
}

TEST(LinearFindTest, ConstexprEvaluatesAtCompileTime) {
  static constexpr std::array<int, 3> kNumbers = {10, 20, 30};
  static_assert(LinearFind(kNumbers, 20) == &kNumbers[1]);
  static_assert(LinearFind(kNumbers, 99) == nullptr);
  static_assert(LinearFind(kNumbers, 60, [](int x) { return x * 2; }) ==
                &kNumbers[2]);
  static_assert(LinearFindIf(kNumbers, [](int x) { return x > 15; }) ==
                &kNumbers[1]);
}

// These compile without `-Wdangling` or `-Wreturn-stack-address` false
// positives because views are taken by value. See the nocompile test for the
// cases that do dangle.
int* FindInSpan(span<int> s, int value) {
  return LinearFind(s, value);
}

const char* FindIfInStringView(std::string_view sv, char c) {
  return LinearFindIf(sv, [c](char x) { return x == c; });
}

struct SpanHolder {
  std::vector<int> values = {1, 2, 3};
  span<const int> GetSpan() const LIFETIME_BOUND { return values; }
};

TEST(LinearFindTest, BorrowedViewsDoNotDangle) {
  std::vector<int> values = {10, 20, 30};
  EXPECT_EQ(FindInSpan(values, 20), &values[1]);
  EXPECT_EQ(FindInSpan(values, 99), nullptr);

  std::string text = "hello";
  EXPECT_EQ(FindIfInStringView(text, 'l'), &text[2]);

  SpanHolder holder;
  // Keep `found` as a local: storing the result past the temporary span is
  // the case that a reference parameter would flag with `-Wdangling`.
  const int* found = LinearFind(holder.GetSpan(), 2);
  EXPECT_EQ(found, &holder.values[1]);
}

}  // namespace

}  // namespace base
