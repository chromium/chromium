// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/break_iterator.h"

#include <stddef.h>
#include <vector>

#include "base/cxx17_backports.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace i18n {

TEST(BreakIteratorTest, BreakWordEmpty) {
  std::u16string empty;
  BreakIterator iter(empty, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWord) {
  std::u16string space(u" ");
  std::u16string str(u" foo bar! \npouet boom");
  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"foo", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"bar", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"!", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"pouet", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"boom", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordWide16) {
  // Two greek words separated by space.
  const std::u16string str(
      u"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      u"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2");
  const std::u16string word1(str.substr(0, 10));
  const std::u16string word2(str.substr(11, 5));
  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(word1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(word2, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordWide32) {
  const std::u16string str = u"\U0001d49c a";
  const std::u16string very_wide_word(str.substr(0, 2));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(very_wide_word, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(u"a", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordThai) {
  // Terms in Thai, without spaces in between.
  const char16_t term1[] = u"พิมพ์";
  const char16_t term2[] = u"น้อย";
  const char16_t term3[] = u"ลง";
  const std::u16string str(base::JoinString({term1, term2, term3}, u""));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term2, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term3, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

// In some languages, the words are not broken by spaces. ICU provides a huge
// dictionary to detect word boundaries in Thai, Chinese, Japanese, Burmese,
// and Khmer. Due to the size of such a table, the part for Chinese and
// Japanese is not shipped on mobile.
#if !(defined(OS_IOS) || defined(OS_ANDROID))

TEST(BreakIteratorTest, BreakWordChinese) {
  // Terms in Traditional Chinese, without spaces in between.
  const char16_t term1[] = u"瀏覽";
  const char16_t term2[] = u"速度";
  const char16_t term3[] = u"飛快";
  const std::u16string str(base::JoinString({term1, term2, term3}, u""));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term2, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term3, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordJapanese) {
  // Terms in Japanese, without spaces in between.
  const char16_t term1[] = u"モバイル";
  const char16_t term2[] = u"でも";
  const std::u16string str(base::JoinString({term1, term2}, u""));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(term2, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordChineseEnglish) {
  // Terms in Simplified Chinese mixed with English and wide punctuations.
  std::u16string space(u" ");
  const char16_t token1[] = u"下载";
  const char16_t token2[] = u"Chrome";
  const char16_t token3[] = u"（";
  const char16_t token4[] = u"Mac";
  const char16_t token5[] = u"版";
  const char16_t token6[] = u"）";
  const std::u16string str(base::JoinString(
      {token1, u" ", token2, token3, token4, u" ", token5, token6}, u""));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(token1, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(token2, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(token3, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(token4, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(token5, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(token6, iter.GetString());

  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

#endif  // !(defined(OS_IOS) || defined(OS_ANDROID))

TEST(BreakIteratorTest, BreakSpaceEmpty) {
  std::u16string empty;
  BreakIterator iter(empty, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpace) {
  std::u16string str(u" foo bar! \npouet boom");
  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"foo ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"bar! \n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"pouet ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"boom", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpaceSP) {
  std::u16string str(u" foo bar! \npouet boom ");
  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"foo ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"bar! \n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"pouet ", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"boom ", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpacekWide16) {
  // Two Greek words.
  const std::u16string str(
      u"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      u"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2");
  const std::u16string word1(str.substr(0, 11));
  const std::u16string word2(str.substr(11, 5));
  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(word1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(word2, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpaceWide32) {
  const std::u16string str = u"\U0001d49c a";
  const std::u16string very_wide_word(str.substr(0, 3));

  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(very_wide_word, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"a", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLineEmpty) {
  std::u16string empty;
  BreakIterator iter(empty, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLine) {
  std::u16string nl(u"\n");
  std::u16string str(u"\nfoo bar!\n\npouet boom");
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"foo bar!\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"pouet boom", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSentence) {
  std::u16string nl(u"\n");
  std::u16string str(
      u"\nFoo bar!\nOne sentence.\n\n\tAnother sentence?One more thing");
  BreakIterator iter(str, BreakIterator::BREAK_SENTENCE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"Foo bar!\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"One sentence.\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"\tAnother sentence?", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"One more thing", iter.GetString());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, IsSentenceBoundary) {
  std::u16string str(
      u"Foo bar!\nOne sentence.\n\n\tAnother sentence?One more thing");
  BreakIterator iter(str, BreakIterator::BREAK_SENTENCE);
  ASSERT_TRUE(iter.Init());

  std::vector<size_t> sentence_breaks;
  sentence_breaks.push_back(0);
  sentence_breaks.push_back(9);
  sentence_breaks.push_back(23);
  sentence_breaks.push_back(24);
  sentence_breaks.push_back(42);
  for (size_t i = 0; i < str.size(); i++) {
    if (ranges::find(sentence_breaks, i) != sentence_breaks.end()) {
      EXPECT_TRUE(iter.IsSentenceBoundary(i)) << " at index=" << i;
    } else {
      EXPECT_FALSE(iter.IsSentenceBoundary(i)) << " at index=" << i;
    }
  }
}

TEST(BreakIteratorTest, BreakLineNL) {
  std::u16string nl(u"\n");
  std::u16string str(u"\nfoo bar!\n\npouet boom\n");
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"foo bar!\n", iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"pouet boom\n", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLineWide16) {
  // Two Greek words separated by newline.
  const std::u16string str(
      u"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      u"\x03bf\x03c2\x000a\x0399\x03c3\x03c4\x03cc\x03c2");
  const std::u16string line1(str.substr(0, 11));
  const std::u16string line2(str.substr(11, 5));
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(line1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(line2, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLineWide32) {
  const std::u16string str = u"\U0001d49c\na";
  const std::u16string very_wide_line(str.substr(0, 3));
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(very_wide_line, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(u"a", iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakCharacter) {
  static const char16_t* const kCharacters[] = {
      // An English word consisting of four ASCII characters.
      u"w",
      u"o",
      u"r",
      u"d",
      u" ",
      // A Hindi word (which means "Hindi") consisting of two Devanagari
      // grapheme clusters.
      u"हि",
      u"न्दी",
      u" ",
      // A Thai word (which means "feel") consisting of three Thai grapheme
      // clusters.
      u"รู้",
      u"สึ",
      u"ก",
      u" ",
  };
  std::vector<std::u16string> characters;
  std::u16string text;
  for (const auto* i : kCharacters) {
    characters.push_back(i);
    text.append(characters.back());
  }
  BreakIterator iter(text, BreakIterator::BREAK_CHARACTER);
  ASSERT_TRUE(iter.Init());
  for (size_t i = 0; i < base::size(kCharacters); ++i) {
    EXPECT_TRUE(iter.Advance());
    EXPECT_EQ(characters[i], iter.GetString());
  }
}

// Test for https://code.google.com/p/chromium/issues/detail?id=411213
// We should be able to get valid substrings with GetString() function
// after setting new content by calling SetText().
TEST(BreakIteratorTest, GetStringAfterSetText) {
  const std::u16string initial_string(u"str");
  BreakIterator iter(initial_string, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  const std::u16string long_string(u"another,string");
  EXPECT_TRUE(iter.SetText(long_string.c_str(), long_string.size()));
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.Advance());  // Advance to ',' in |long_string|

  // Check that the current position is out of bounds of the |initial_string|.
  EXPECT_LT(initial_string.size(), iter.pos());

  // Check that we can get a valid substring of |long_string|.
  EXPECT_EQ(u",", iter.GetString());
}

TEST(BreakIteratorTest, GetStringPiece) {
  const std::u16string initial_string(u"some string");
  BreakIterator iter(initial_string, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(iter.GetString(), iter.GetStringPiece());
  EXPECT_EQ(StringPiece16(u"some"), iter.GetStringPiece());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(iter.GetString(), iter.GetStringPiece());
  EXPECT_EQ(StringPiece16(u"string"), iter.GetStringPiece());
}

// Make sure that when not in RULE_BASED or BREAK_WORD mode we're getting
// IS_LINE_OR_CHAR_BREAK.
TEST(BreakIteratorTest, GetWordBreakStatusBreakLine) {
  // A string containing the English word "foo", followed by two Khmer
  // characters, the English word "Can", and then two Russian characters and
  // punctuation.
  std::u16string text(u"foo \x1791\x17C1 \nCan \x041C\x0438...");
  BreakIterator iter(text, BreakIterator::BREAK_LINE);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo" and the space.
  EXPECT_EQ(u"foo ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the Khmer characters, the next space, and the newline.
  EXPECT_EQ(u"\x1791\x17C1 \n", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds "Can" and the space.
  EXPECT_EQ(u"Can ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the Russian characters and periods.
  EXPECT_EQ(u"\x041C\x0438...", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_FALSE(iter.Advance());
}

// Make sure that in BREAK_WORD mode we're getting IS_WORD_BREAK and
// IS_SKIPPABLE_WORD when we should be. IS_WORD_BREAK should be returned when we
// finish going over non-punctuation characters while IS_SKIPPABLE_WORD should
// be returned on punctuation and spaces.
TEST(BreakIteratorTest, GetWordBreakStatusBreakWord) {
  // A string containing the English word "foo", followed by two Khmer
  // characters, the English word "Can", and then two Russian characters and
  // punctuation.
  std::u16string text(u"foo \x1791\x17C1 \nCan \x041C\x0438...");
  BreakIterator iter(text, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo".
  EXPECT_EQ(u"foo", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space, and the Khmer characters.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x1791\x17C1", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and the newline.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\n", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds "Can".
  EXPECT_EQ(u"Can", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and the Russian characters.
  EXPECT_EQ(u" ", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u"\x041C\x0438", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the trailing periods.
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(u".", iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

}  // namespace i18n
}  // namespace base
