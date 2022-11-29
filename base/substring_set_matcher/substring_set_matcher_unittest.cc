// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/substring_set_matcher/substring_set_matcher.h"

#include <stddef.h>

#include <set>
#include <string>
#include <vector>

#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {

namespace {

void TestOnePattern(const std::string& test_string,
                    const std::string& pattern,
                    bool is_match) {
  std::string test = "TestOnePattern(" + test_string + ", " + pattern + ", " +
                     (is_match ? "1" : "0") + ")";
  std::vector<MatcherStringPattern> patterns;
  patterns.emplace_back(pattern, 1);
  SubstringSetMatcher matcher;
  ASSERT_TRUE(matcher.Build(patterns));
  std::set<MatcherStringPattern::ID> matches;
  matcher.Match(test_string, &matches);

  size_t expected_matches = (is_match ? 1 : 0);
  EXPECT_EQ(expected_matches, matches.size()) << test;
  EXPECT_EQ(is_match, matches.find(1) != matches.end()) << test;
}

void TestTwoPatterns(const std::string& test_string,
                     const std::string& pattern_1,
                     const std::string& pattern_2,
                     bool is_match_1,
                     bool is_match_2) {
  std::string test = "TestTwoPatterns(" + test_string + ", " + pattern_1 +
                     ", " + pattern_2 + ", " + (is_match_1 ? "1" : "0") + ", " +
                     (is_match_2 ? "1" : "0") + ")";
  ASSERT_NE(pattern_1, pattern_2);
  MatcherStringPattern substring_pattern_1(pattern_1, 1);
  MatcherStringPattern substring_pattern_2(pattern_2, 2);
  // In order to make sure that the order in which patterns are registered
  // does not make any difference we try both permutations.
  for (int permutation = 0; permutation < 2; ++permutation) {
    std::vector<const MatcherStringPattern*> patterns;
    if (permutation == 0) {
      patterns.push_back(&substring_pattern_1);
      patterns.push_back(&substring_pattern_2);
    } else {
      patterns.push_back(&substring_pattern_2);
      patterns.push_back(&substring_pattern_1);
    }
    SubstringSetMatcher matcher;
    ASSERT_TRUE(matcher.Build(patterns));
    std::set<MatcherStringPattern::ID> matches;
    matcher.Match(test_string, &matches);

    size_t expected_matches = (is_match_1 ? 1 : 0) + (is_match_2 ? 1 : 0);
    EXPECT_EQ(expected_matches, matches.size()) << test;
    EXPECT_EQ(is_match_1, matches.find(1) != matches.end()) << test;
    EXPECT_EQ(is_match_2, matches.find(2) != matches.end()) << test;
  }
}

}  // namespace

TEST(SubstringSetMatcherTest, TestMatcher) {
  // Test overlapping patterns
  // String    abcde
  // Pattern 1  bc
  // Pattern 2   cd
  TestTwoPatterns("abcde", "bc", "cd", true, true);
  if (HasFatalFailure())
    return;

  // Test subpatterns - part 1
  // String    abcde
  // Pattern 1  bc
  // Pattern 2  b
  TestTwoPatterns("abcde", "bc", "b", true, true);
  if (HasFatalFailure())
    return;

  // Test subpatterns - part 2
  // String    abcde
  // Pattern 1  bc
  // Pattern 2   c
  TestTwoPatterns("abcde", "bc", "c", true, true);
  if (HasFatalFailure())
    return;

  // Test identical matches
  // String    abcde
  // Pattern 1 abcde
  TestOnePattern("abcde", "abcde", true);
  if (HasFatalFailure())
    return;

  // Test multiple matches
  // String    aaaaa
  // Pattern 1 a
  TestOnePattern("abcde", "a", true);
  if (HasFatalFailure())
    return;

  // Test matches at beginning and end
  // String    abcde
  // Pattern 1 ab
  // Pattern 2    de
  TestTwoPatterns("abcde", "ab", "de", true, true);
  if (HasFatalFailure())
    return;

  // Test non-match
  // String    abcde
  // Pattern 1        fg
  TestOnePattern("abcde", "fg", false);
  if (HasFatalFailure())
    return;

  // Test empty pattern and too long pattern
  // String    abcde
  // Pattern 1
  // Pattern 2 abcdef
  TestTwoPatterns("abcde", std::string(), "abcdef", true, false);
  if (HasFatalFailure())
    return;
}

TEST(SubstringSetMatcherTest, TestMatcher2) {
  MatcherStringPattern pattern_1("a", 1);
  MatcherStringPattern pattern_2("b", 2);
  MatcherStringPattern pattern_3("c", 3);

  std::vector<const MatcherStringPattern*> patterns = {&pattern_1, &pattern_2,
                                                       &pattern_3};
  auto matcher = std::make_unique<SubstringSetMatcher>();
  ASSERT_TRUE(matcher->Build(patterns));

  std::set<MatcherStringPattern::ID> matches;
  matcher->Match("abd", &matches);
  EXPECT_EQ(2u, matches.size());
  EXPECT_TRUE(matches.end() != matches.find(1));
  EXPECT_TRUE(matches.end() != matches.find(2));

  patterns = {&pattern_1, &pattern_3};
  matcher = std::make_unique<SubstringSetMatcher>();
  ASSERT_TRUE(matcher->Build(patterns));

  matches.clear();
  matcher->Match("abd", &matches);
  EXPECT_EQ(1u, matches.size());
  EXPECT_TRUE(matches.end() != matches.find(1));
  EXPECT_TRUE(matches.end() == matches.find(2));

  matcher = std::make_unique<SubstringSetMatcher>();
  ASSERT_TRUE(matcher->Build(std::vector<const MatcherStringPattern*>()));
  EXPECT_TRUE(matcher->IsEmpty());
}

TEST(SubstringSetMatcherTest, TestMatcher3) {
  std::string text = "abcde";

  std::vector<MatcherStringPattern> patterns;
  int id = 0;
  // Add all substrings of this string, including empty string.
  patterns.emplace_back("", id++);
  for (size_t i = 0; i < text.length(); i++) {
    for (size_t j = i; j < text.length(); j++) {
      patterns.emplace_back(text.substr(i, j - i + 1), id++);
    }
  }

  SubstringSetMatcher matcher;
  matcher.Build(patterns);
  std::set<MatcherStringPattern::ID> matches;
  matcher.Match(text, &matches);
  EXPECT_EQ(patterns.size(), matches.size());
  for (const MatcherStringPattern& pattern : patterns) {
    EXPECT_TRUE(matches.find(pattern.id()) != matches.end())
        << pattern.pattern();
  }
}

TEST(SubstringSetMatcherTest, TestEmptyMatcher) {
  std::vector<MatcherStringPattern> patterns;
  SubstringSetMatcher matcher;
  matcher.Build(patterns);
  std::set<MatcherStringPattern::ID> matches;
  matcher.Match("abd", &matches);
  EXPECT_TRUE(matches.empty());
  EXPECT_TRUE(matcher.IsEmpty());
}

// Test a case where we have more than 256 edges from one node
// (the “a” node gets one for each possible ASCII bytes, and then
// one for the output link).
TEST(SubstringSetMatcherTest, LotsOfEdges) {
  std::vector<MatcherStringPattern> patterns;
  for (int i = 0; i < 256; ++i) {
    std::string str;
    str.push_back('a');
    str.push_back(static_cast<char>(i));
    patterns.emplace_back(str, i);
  }

  {
    std::string str;
    str.push_back('a');
    patterns.emplace_back(str, 256);
  }

  SubstringSetMatcher matcher;
  matcher.Build(patterns);
}

}  // namespace base
