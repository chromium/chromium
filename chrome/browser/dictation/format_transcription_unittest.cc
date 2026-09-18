// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/dictation/format_transcription.h"

#include <string>

#include "testing/gtest/include/gtest/gtest.h"

namespace dictation {

namespace {

TEST(FormatTranscriptionTest, WhitespaceNeeded) {
  // Empty inputs.
  EXPECT_FALSE(WhitespaceNeeded(u"", u"Hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u""));
  EXPECT_FALSE(WhitespaceNeeded(u"", u""));

  // Preceding text ends with whitespace.
  EXPECT_FALSE(WhitespaceNeeded(u"Hello ", u"world"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello\n", u"world"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello\t", u"world"));

  // New text starts with whitespace.
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u" world"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"\nworld"));

  // Standard Latin text append (space is needed).
  EXPECT_TRUE(WhitespaceNeeded(u"Hello", u"world"));
  EXPECT_TRUE(WhitespaceNeeded(u"One,", u"two"));
  EXPECT_TRUE(WhitespaceNeeded(u"Why?", u"Because"));
  EXPECT_TRUE(WhitespaceNeeded(u"Wow!", u"Amazing"));
  EXPECT_TRUE(WhitespaceNeeded(u"This is a test.", u"This is another test."));

  // Preceding prefix symbols, mentions, hyphens, dashes, slashes, and
  // backslashes (no space after).
  EXPECT_FALSE(WhitespaceNeeded(u"$", u"100"));
  EXPECT_FALSE(WhitespaceNeeded(u"@", u"john"));
  EXPECT_FALSE(WhitespaceNeeded(u"#", u"trending"));
  EXPECT_FALSE(WhitespaceNeeded(u"multi-", u"threaded"));
  EXPECT_FALSE(WhitespaceNeeded(u"—", u"text"));
  EXPECT_FALSE(WhitespaceNeeded(u"–", u"text"));
  EXPECT_FALSE(WhitespaceNeeded(u"https://", u"google.com"));
  EXPECT_FALSE(WhitespaceNeeded(u"C:\\", u"Users"));

  // Preceding opening brackets and quotation marks (no space after).
  EXPECT_FALSE(WhitespaceNeeded(u"(", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"[", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"{", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"\"", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"'", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"`", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"“", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"‘", u"hello"));
  EXPECT_FALSE(WhitespaceNeeded(u"«", u"hello"));

  // New text starts with closing or attaching punctuation (no space before).
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"."));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u","));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"!"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"?"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u":"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u";"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u")"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"]"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"}"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"\""));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"'"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"”"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello", u"’"));
  EXPECT_FALSE(WhitespaceNeeded(u"100", u"%"));

  // Scripts without spaces (e.g. CJK) and full-width punctuation.
  EXPECT_FALSE(WhitespaceNeeded(u"你好", u"世界"));
  EXPECT_FALSE(WhitespaceNeeded(u"你好！", u"今天天气很好"));
  EXPECT_FALSE(WhitespaceNeeded(u"Hello，", u"world"));
}

TEST(FormatTranscriptionTest, TrimTrailingPeriodIfSingleSentence) {
  // Empty and whitespace-only strings.
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u""), u"");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"   "), u"   ");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Jane Doe"), u"Jane Doe");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"How are you?"),
            u"How are you?");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Great!"), u"Great!");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Jane Doe."), u"Jane Doe");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Jane Doe. "), u"Jane Doe ");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Pi is 3.14"), u"Pi is 3.14");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"3.14."), u"3.14");
  EXPECT_EQ(TrimTrailingPeriodIfSingleSentence(u"Thinking..."), u"Thinking...");

  // Multiple sentences should preserve all periods.
  EXPECT_EQ(
      TrimTrailingPeriodIfSingleSentence(u"First sentence. Second sentence."),
      u"First sentence. Second sentence.");
  EXPECT_EQ(
      TrimTrailingPeriodIfSingleSentence(u"Hello! How are you? All good."),
      u"Hello! How are you? All good.");
}

TEST(FormatTranscriptionTest, FormatTranscription) {
  // Empty inputs.
  EXPECT_EQ(FormatTranscription(u"", u"Hello"), u"");
  EXPECT_EQ(FormatTranscription(u"Hello", u""), u"Hello");
  EXPECT_EQ(FormatTranscription(u"", u""), u"");

  // Prepends whitespace when needed and trims trailing period for single
  // sentences.
  EXPECT_EQ(FormatTranscription(u"Jane Doe.", u"Hello"), u" Jane Doe");
  EXPECT_EQ(FormatTranscription(u"Jane Doe.", u"Hello "), u"Jane Doe");
  EXPECT_EQ(FormatTranscription(u"world", u"Hello"), u" world");
  EXPECT_EQ(FormatTranscription(u"world", u"Hello "), u"world");
  EXPECT_EQ(FormatTranscription(u"First sentence. Second sentence.", u"Hello"),
            u" First sentence. Second sentence.");
  EXPECT_EQ(FormatTranscription(u"...", u"Hello"), u"...");
}

}  // namespace

}  // namespace dictation
