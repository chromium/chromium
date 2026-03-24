// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_signal_utils.h"

#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(GetMatchingWordsCountTest, QueryOrCandidateEmpty) {
  EXPECT_EQ(GetMatchingWordsCount("", "Page Title"), 0);
  EXPECT_EQ(GetMatchingWordsCount("Query", ""), 0);
  EXPECT_EQ(GetMatchingWordsCount("", ""), 0);
}

TEST(GetMatchingWordsCountTest, QueryOrCandidateHavePunctuations) {
  EXPECT_EQ(GetMatchingWordsCount("?! ,", "  ...  "), 0);
  EXPECT_EQ(GetMatchingWordsCount("Query", "  ...  "), 0);
  EXPECT_EQ(GetMatchingWordsCount("?! ,", "Title"), 0);
  EXPECT_EQ(GetMatchingWordsCount("test, query!", "This is a test query..."),
            2);
  EXPECT_EQ(GetMatchingWordsCount("test-query", "This is a test query"), 2);
}

TEST(GetMatchingWordsCountTest, QueryAndCandidateHaveNoMatch) {
  EXPECT_EQ(GetMatchingWordsCount("no match", "This is a test query"), 0);
}

TEST(GetMatchingWordsCountTest, QueryHasOnlyStopWords) {
  EXPECT_EQ(GetMatchingWordsCount("this is a", "This is a test query"), 0);
}

TEST(GetMatchingWordsCountTest, QueryAndCandidatesHaveMatches) {
  EXPECT_EQ(GetMatchingWordsCount("what's the weather", "Weather forecast"), 1);
  EXPECT_EQ(GetMatchingWordsCount("test query", "This is a test query"), 2);
  EXPECT_EQ(GetMatchingWordsCount("this is a test query", "Test query"), 2);
  EXPECT_EQ(GetMatchingWordsCount("test query example", "This is a test query"),
            2);
}

TEST(GetMatchingWordsCountTest, QueryHasDuplicateMatchingWords) {
  EXPECT_EQ(GetMatchingWordsCount("test test query", "This is a test query"),
            2);
}

TEST(GetMatchingWordsCountTest, NormalizedTextConsidered) {
  EXPECT_EQ(GetMatchingWordsCount("TEST QuErY", "tHis IS a TeSt qUeRy"), 2);
  EXPECT_EQ(
      GetMatchingWordsCount("  test   query  ", "  This is a test query  "), 2);
}

TEST(GetWordCountTest, EmptyText) {
  EXPECT_EQ(GetWordCount(""), 0);
  EXPECT_EQ(GetWordCount("   "), 0);
}

TEST(GetWordCountTest, TextWithOnlyPunctuations) {
  EXPECT_EQ(GetWordCount("!?, ."), 0);
}

TEST(GetWordCountTest, NormalText) {
  EXPECT_EQ(GetWordCount("This is a test query"), 5);
}

TEST(GetWordCountTest, TextWithPunctuations) {
  EXPECT_EQ(GetWordCount("Hello, world!"), 2);
  EXPECT_EQ(GetWordCount("test-query"), 2);
  EXPECT_EQ(GetWordCount("test.query"), 2);
}

TEST(GetWordCountTest, TextWithExtraSpaces) {
  EXPECT_EQ(GetWordCount("  This   is  a test   query  "), 5);
}

TEST(GetWordCountTest, TextWithDuplicates) {
  EXPECT_EQ(GetWordCount("test test query"), 3);
}

}  // namespace contextual_tasks
