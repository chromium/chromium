// Copyright (c) 2011 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/break_iterator.h"

#include <stddef.h>
#include <vector>

#include "base/stl_util.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/stringprintf.h"
#include "base/strings/utf_string_conversions.h"
#include "build/build_config.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base {
namespace i18n {

TEST(BreakIteratorTest, BreakWordEmpty) {
  string16 empty;
  BreakIterator iter(empty, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWord) {
  string16 space(UTF8ToUTF16(" "));
  string16 str(UTF8ToUTF16(" foo bar! \npouet boom"));
  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("foo"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("bar"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("!"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("pouet"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("boom"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordWide16) {
  // Two greek words separated by space.
  const string16 str(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2"));
  const string16 word1(str.substr(0, 10));
  const string16 word2(str.substr(11, 5));
  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(word1, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(" "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(word2, iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordWide32) {
  // U+1D49C MATHEMATICAL SCRIPT CAPITAL A
  const char very_wide_char[] = "\xF0\x9D\x92\x9C";
  const string16 str(
      UTF8ToUTF16(base::StringPrintf("%s a", very_wide_char)));
  const string16 very_wide_word(str.substr(0, 2));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(very_wide_word, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(" "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("a"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordThai) {
  // Terms in Thai, without spaces in between.
  const char term1[] = "พิมพ์";
  const char term2[] = "น้อย";
  const char term3[] = "ลง";
  const string16 str(UTF8ToUTF16(base::JoinString({term1, term2, term3}, "")));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term1), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term2), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term3), iter.GetString());
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
  const char term1[] = "瀏覽";
  const char term2[] = "速度";
  const char term3[] = "飛快";
  const string16 str(UTF8ToUTF16(base::JoinString({term1, term2, term3}, "")));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term1), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term2), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term3), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordJapanese) {
  // Terms in Japanese, without spaces in between.
  const char term1[] = "モバイル";
  const char term2[] = "でも";
  const string16 str(UTF8ToUTF16(base::JoinString({term1, term2}, "")));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term1), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(term2), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakWordChineseEnglish) {
  // Terms in Simplified Chinese mixed with English and wide punctuations.
  string16 space(UTF8ToUTF16(" "));
  const char token1[] = "下载";
  const char token2[] = "Chrome";
  const char token3[] = "（";
  const char token4[] = "Mac";
  const char token5[] = "版";
  const char token6[] = "）";
  const string16 str(UTF8ToUTF16(base::JoinString(
      {token1, " ", token2, token3, token4, " ", token5, token6}, "")));

  BreakIterator iter(str, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token1), iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token2), iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token3), iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token4), iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(space, iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token5), iter.GetString());

  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(token6), iter.GetString());

  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
}

#endif  // !(defined(OS_IOS) || defined(OS_ANDROID))

TEST(BreakIteratorTest, BreakSpaceEmpty) {
  string16 empty;
  BreakIterator iter(empty, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpace) {
  string16 str(UTF8ToUTF16(" foo bar! \npouet boom"));
  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(" "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("foo "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("bar! \n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("pouet "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("boom"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpaceSP) {
  string16 str(UTF8ToUTF16(" foo bar! \npouet boom "));
  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16(" "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("foo "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("bar! \n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("pouet "), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("boom "), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSpacekWide16) {
  // Two Greek words.
  const string16 str(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      L"\x03bf\x03c2\x0020\x0399\x03c3\x03c4\x03cc\x03c2"));
  const string16 word1(str.substr(0, 11));
  const string16 word2(str.substr(11, 5));
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
  // U+1D49C MATHEMATICAL SCRIPT CAPITAL A
  const char very_wide_char[] = "\xF0\x9D\x92\x9C";
  const string16 str(
      UTF8ToUTF16(base::StringPrintf("%s a", very_wide_char)));
  const string16 very_wide_word(str.substr(0, 3));

  BreakIterator iter(str, BreakIterator::BREAK_SPACE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(very_wide_word, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("a"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLineEmpty) {
  string16 empty;
  BreakIterator iter(empty, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLine) {
  string16 nl(UTF8ToUTF16("\n"));
  string16 str(UTF8ToUTF16("\nfoo bar!\n\npouet boom"));
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("foo bar!\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("pouet boom"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakSentence) {
  string16 nl(UTF8ToUTF16("\n"));
  string16 str(UTF8ToUTF16(
      "\nFoo bar!\nOne sentence.\n\n\tAnother sentence?One more thing"));
  BreakIterator iter(str, BreakIterator::BREAK_SENTENCE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("Foo bar!\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("One sentence.\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("\tAnother sentence?"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("One more thing"), iter.GetString());
  EXPECT_FALSE(iter.Advance());  // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, IsSentenceBoundary) {
  string16 str(UTF8ToUTF16(
      "Foo bar!\nOne sentence.\n\n\tAnother sentence?One more thing"));
  BreakIterator iter(str, BreakIterator::BREAK_SENTENCE);
  ASSERT_TRUE(iter.Init());

  std::vector<size_t> sentence_breaks;
  sentence_breaks.push_back(0);
  sentence_breaks.push_back(9);
  sentence_breaks.push_back(23);
  sentence_breaks.push_back(24);
  sentence_breaks.push_back(42);
  for (size_t i = 0; i < str.size(); i++) {
    if (std::find(sentence_breaks.begin(), sentence_breaks.end(), i) !=
        sentence_breaks.end()) {
      EXPECT_TRUE(iter.IsSentenceBoundary(i)) << " at index=" << i;
    } else {
      EXPECT_FALSE(iter.IsSentenceBoundary(i)) << " at index=" << i;
    }
  }
}

TEST(BreakIteratorTest, BreakLineNL) {
  string16 nl(UTF8ToUTF16("\n"));
  string16 str(UTF8ToUTF16("\nfoo bar!\n\npouet boom\n"));
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("foo bar!\n"), iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(nl, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("pouet boom\n"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakLineWide16) {
  // Two Greek words separated by newline.
  const string16 str(WideToUTF16(
      L"\x03a0\x03b1\x03b3\x03ba\x03cc\x03c3\x03bc\x03b9"
      L"\x03bf\x03c2\x000a\x0399\x03c3\x03c4\x03cc\x03c2"));
  const string16 line1(str.substr(0, 11));
  const string16 line2(str.substr(11, 5));
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
  // U+1D49C MATHEMATICAL SCRIPT CAPITAL A
  const char very_wide_char[] = "\xF0\x9D\x92\x9C";
  const string16 str(
      UTF8ToUTF16(base::StringPrintf("%s\na", very_wide_char)));
  const string16 very_wide_line(str.substr(0, 3));
  BreakIterator iter(str, BreakIterator::BREAK_NEWLINE);
  ASSERT_TRUE(iter.Init());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(very_wide_line, iter.GetString());
  EXPECT_TRUE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_EQ(UTF8ToUTF16("a"), iter.GetString());
  EXPECT_FALSE(iter.Advance());
  EXPECT_FALSE(iter.IsWord());
  EXPECT_FALSE(iter.Advance());   // Test unexpected advance after end.
  EXPECT_FALSE(iter.IsWord());
}

TEST(BreakIteratorTest, BreakCharacter) {
  static const char* kCharacters[] = {
      // An English word consisting of four ASCII characters.
      "w",
      "o",
      "r",
      "d",
      " ",
      // A Hindi word (which means "Hindi") consisting of two Devanagari
      // grapheme clusters.
      "\u0939\u093F",
      "\u0928\u094D\u0926\u0940",
      " ",
      // A Thai word (which means "feel") consisting of three Thai grapheme
      // clusters.
      "\u0E23\u0E39\u0E49",
      "\u0E2A\u0E36",
      "\u0E01",
      " ",
  };
  std::vector<string16> characters;
  string16 text;
  for (auto*& i : kCharacters) {
    characters.push_back(base::UTF8ToUTF16(i));
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
  const string16 initial_string(ASCIIToUTF16("str"));
  BreakIterator iter(initial_string, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  const string16 long_string(ASCIIToUTF16("another,string"));
  EXPECT_TRUE(iter.SetText(long_string.c_str(), long_string.size()));
  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.Advance());  // Advance to ',' in |long_string|

  // Check that the current position is out of bounds of the |initial_string|.
  EXPECT_LT(initial_string.size(), iter.pos());

  // Check that we can get a valid substring of |long_string|.
  EXPECT_EQ(ASCIIToUTF16(","), iter.GetString());
}

TEST(BreakIteratorTest, GetStringPiece) {
  const string16 initial_string(ASCIIToUTF16("some string"));
  BreakIterator iter(initial_string, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(iter.GetString(), iter.GetStringPiece().as_string());
  EXPECT_EQ(StringPiece16(ASCIIToUTF16("some")), iter.GetStringPiece());

  EXPECT_TRUE(iter.Advance());
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(iter.GetString(), iter.GetStringPiece().as_string());
  EXPECT_EQ(StringPiece16(ASCIIToUTF16("string")), iter.GetStringPiece());
}

// Make sure that when not in RULE_BASED or BREAK_WORD mode we're getting
// IS_LINE_OR_CHAR_BREAK.
TEST(BreakIteratorTest, GetWordBreakStatusBreakLine) {
  // A string containing the English word "foo", followed by two Khmer
  // characters, the English word "Can", and then two Russian characters and
  // punctuation.
  base::string16 text(
      base::WideToUTF16(L"foo \x1791\x17C1 \nCan \x041C\x0438..."));
  BreakIterator iter(text, BreakIterator::BREAK_LINE);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo" and the space.
  EXPECT_EQ(base::UTF8ToUTF16("foo "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the Khmer characters, the next space, and the newline.
  EXPECT_EQ(base::WideToUTF16(L"\x1791\x17C1 \n"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds "Can" and the space.
  EXPECT_EQ(base::UTF8ToUTF16("Can "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_LINE_OR_CHAR_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the Russian characters and periods.
  EXPECT_EQ(base::WideToUTF16(L"\x041C\x0438..."), iter.GetString());
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
  base::string16 text(
      base::WideToUTF16(L"foo \x1791\x17C1 \nCan \x041C\x0438..."));
  BreakIterator iter(text, BreakIterator::BREAK_WORD);
  ASSERT_TRUE(iter.Init());

  EXPECT_TRUE(iter.Advance());
  // Finds "foo".
  EXPECT_EQ(base::UTF8ToUTF16("foo"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space, and the Khmer characters.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x1791\x17C1"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and the newline.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("\n"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  // Finds "Can".
  EXPECT_EQ(base::UTF8ToUTF16("Can"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the space and the Russian characters.
  EXPECT_EQ(base::UTF8ToUTF16(" "), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::WideToUTF16(L"\x041C\x0438"), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_WORD_BREAK);
  EXPECT_TRUE(iter.Advance());
  // Finds the trailing periods.
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_TRUE(iter.Advance());
  EXPECT_EQ(base::UTF8ToUTF16("."), iter.GetString());
  EXPECT_EQ(iter.GetWordBreakStatus(), BreakIterator::IS_SKIPPABLE_WORD);
  EXPECT_FALSE(iter.Advance());
}

}  // namespace i18n
}  // namespace base
