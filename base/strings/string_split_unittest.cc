// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_split.h"

#include <stddef.h>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "testing/gmock/include/gmock/gmock.h"
#include "testing/gtest/include/gtest/gtest.h"

using ::testing::ElementsAre;

namespace base {

class SplitStringIntoKeyValuePairsTest : public testing::Test {
 protected:
  base::StringPairs kv_pairs;
};

using SplitStringIntoKeyValuePairsUsingSubstrTest =
    SplitStringIntoKeyValuePairsTest;

TEST_F(SplitStringIntoKeyValuePairsUsingSubstrTest, EmptyString) {
  EXPECT_TRUE(
      SplitStringIntoKeyValuePairsUsingSubstr(std::string(),
                                              ':',  // Key-value delimiter
                                              ",",  // Key-value pair delimiter
                                              &kv_pairs));
  EXPECT_TRUE(kv_pairs.empty());
}

TEST_F(SplitStringIntoKeyValuePairsUsingSubstrTest, MissingKeyValueDelimiter) {
  EXPECT_FALSE(
      SplitStringIntoKeyValuePairsUsingSubstr("key1,,key2:value2",
                                              ':',   // Key-value delimiter
                                              ",,",  // Key-value pair delimiter
                                              &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_TRUE(kv_pairs[0].first.empty());
  EXPECT_TRUE(kv_pairs[0].second.empty());
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsUsingSubstrTest,
       MissingKeyValuePairDelimiter) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairsUsingSubstr(
      "key1:value1,,key3:value3",
      ':',    // Key-value delimiter
      ",,,",  // Key-value pair delimiter
      &kv_pairs));
  ASSERT_EQ(1U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1,,key3:value3", kv_pairs[0].second);
}

TEST_F(SplitStringIntoKeyValuePairsUsingSubstrTest, UntrimmedWhitespace) {
  EXPECT_TRUE(
      SplitStringIntoKeyValuePairsUsingSubstr("key1 : value1",
                                              ':',  // Key-value delimiter
                                              ",",  // Key-value pair delimiter
                                              &kv_pairs));
  ASSERT_EQ(1U, kv_pairs.size());
  EXPECT_EQ("key1 ", kv_pairs[0].first);
  EXPECT_EQ(" value1", kv_pairs[0].second);
}

TEST_F(SplitStringIntoKeyValuePairsUsingSubstrTest, OnlySplitAtGivenSeparator) {
  std::string a("a ?!@#$%^&*()_+:/{}\\\t\nb");
  EXPECT_TRUE(
      SplitStringIntoKeyValuePairsUsingSubstr(a + "X" + a + "XY" + a + "YX" + a,
                                              'X',   // Key-value delimiter
                                              "XY",  // Key-value pair delimiter
                                              &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ(a, kv_pairs[0].first);
  EXPECT_EQ(a, kv_pairs[0].second);
  EXPECT_EQ(a + 'Y', kv_pairs[1].first);
  EXPECT_EQ(a, kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptyString) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs(std::string(),
                                           ':',  // Key-value delimiter
                                           ',',  // Key-value pair delimiter
                                           &kv_pairs));
  EXPECT_TRUE(kv_pairs.empty());
}

TEST_F(SplitStringIntoKeyValuePairsTest, MissingKeyValueDelimiter) {
  EXPECT_FALSE(SplitStringIntoKeyValuePairs("key1,key2:value2",
                                            ':',  // Key-value delimiter
                                            ',',  // Key-value pair delimiter
                                            &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_TRUE(kv_pairs[0].first.empty());
  EXPECT_TRUE(kv_pairs[0].second.empty());
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptyKeyWithKeyValueDelimiter) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs(":value1,key2:value2",
                                           ':',  // Key-value delimiter
                                           ',',  // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_TRUE(kv_pairs[0].first.empty());
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, TrailingAndLeadingPairDelimiter) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs(",key1:value1,key2:value2,",
                                           ':',   // Key-value delimiter
                                           ',',   // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptyPair) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:value1,,key3:value3",
                                           ':',   // Key-value delimiter
                                           ',',   // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key3", kv_pairs[1].first);
  EXPECT_EQ("value3", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, EmptyValue) {
  EXPECT_FALSE(SplitStringIntoKeyValuePairs("key1:,key2:value2",
                                            ':',   // Key-value delimiter
                                            ',',   // Key-value pair delimiter
                                            &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, UntrimmedWhitespace) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1 : value1",
                                           ':',  // Key-value delimiter
                                           ',',  // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(1U, kv_pairs.size());
  EXPECT_EQ("key1 ", kv_pairs[0].first);
  EXPECT_EQ(" value1", kv_pairs[0].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, TrimmedWhitespace) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:value1 , key2:value2",
                                           ':',   // Key-value delimiter
                                           ',',   // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, MultipleKeyValueDelimiters) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:::value1,key2:value2",
                                           ':',   // Key-value delimiter
                                           ',',   // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("value1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST_F(SplitStringIntoKeyValuePairsTest, OnlySplitAtGivenSeparator) {
  std::string a("a ?!@#$%^&*()_+:/{}\\\t\nb");
  EXPECT_TRUE(SplitStringIntoKeyValuePairs(a + "X" + a + "Y" + a + "X" + a,
                                           'X',  // Key-value delimiter
                                           'Y',  // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ(a, kv_pairs[0].first);
  EXPECT_EQ(a, kv_pairs[0].second);
  EXPECT_EQ(a, kv_pairs[1].first);
  EXPECT_EQ(a, kv_pairs[1].second);
}


TEST_F(SplitStringIntoKeyValuePairsTest, DelimiterInValue) {
  EXPECT_TRUE(SplitStringIntoKeyValuePairs("key1:va:ue1,key2:value2",
                                           ':',   // Key-value delimiter
                                           ',',   // Key-value pair delimiter
                                           &kv_pairs));
  ASSERT_EQ(2U, kv_pairs.size());
  EXPECT_EQ("key1", kv_pairs[0].first);
  EXPECT_EQ("va:ue1", kv_pairs[0].second);
  EXPECT_EQ("key2", kv_pairs[1].first);
  EXPECT_EQ("value2", kv_pairs[1].second);
}

TEST(SplitStringUsingSubstrTest, EmptyString) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      std::string(), "DELIMITER", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre(""));
}

TEST(SplitStringUsingSubstrTest, EmptyDelimiter) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      "TEST", std::string(), TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("TEST"));
}

TEST(StringUtilTest, SplitString_Basics) {
  std::vector<std::string> r;

  r = SplitString(std::string(), ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  EXPECT_TRUE(r.empty());

  // Empty separator list
  r = SplitString("hello, world", "", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(1u, r.size());
  EXPECT_EQ("hello, world", r[0]);

  // Should split on any of the separators.
  r = SplitString("::,,;;", ",:;", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(7u, r.size());
  for (auto str : r)
    ASSERT_TRUE(str.empty());

  r = SplitString("red, green; blue:", ",:;", TRIM_WHITESPACE,
                  SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(3u, r.size());
  EXPECT_EQ("red", r[0]);
  EXPECT_EQ("green", r[1]);
  EXPECT_EQ("blue", r[2]);

  // Want to split a string along whitespace sequences.
  r = SplitString("  red green   \tblue\n", " \t\n", TRIM_WHITESPACE,
                  SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(3u, r.size());
  EXPECT_EQ("red", r[0]);
  EXPECT_EQ("green", r[1]);
  EXPECT_EQ("blue", r[2]);

  // Weird case of splitting on spaces but not trimming.
  r = SplitString(" red ", " ", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(3u, r.size());
  EXPECT_EQ("", r[0]);  // Before the first space.
  EXPECT_EQ("red", r[1]);
  EXPECT_EQ("", r[2]);  // After the last space.
}

TEST(StringUtilTest, SplitString_WhitespaceAndResultType) {
  std::vector<std::string> r;

  // Empty input handling.
  r = SplitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  EXPECT_TRUE(r.empty());
  r = SplitString(std::string(), ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
  EXPECT_TRUE(r.empty());

  // Input string is space and we're trimming.
  r = SplitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(1u, r.size());
  EXPECT_EQ("", r[0]);
  r = SplitString(" ", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  EXPECT_TRUE(r.empty());

  // Test all 4 combinations of flags on ", ,".
  r = SplitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(3u, r.size());
  EXPECT_EQ("", r[0]);
  EXPECT_EQ(" ", r[1]);
  EXPECT_EQ("", r[2]);
  r = SplitString(", ,", ",", KEEP_WHITESPACE, SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(1u, r.size());
  ASSERT_EQ(" ", r[0]);
  r = SplitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(3u, r.size());
  EXPECT_EQ("", r[0]);
  EXPECT_EQ("", r[1]);
  EXPECT_EQ("", r[2]);
  r = SplitString(", ,", ",", TRIM_WHITESPACE, SPLIT_WANT_NONEMPTY);
  ASSERT_TRUE(r.empty());
}

TEST(SplitStringUsingSubstrTest, StringWithNoDelimiter) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      "alongwordwithnodelimiter", "DELIMITER", TRIM_WHITESPACE,
      SPLIT_WANT_ALL);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(SplitStringUsingSubstrTest, LeadingDelimitersSkipped) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      "DELIMITERDELIMITERDELIMITERoneDELIMITERtwoDELIMITERthree",
      "DELIMITER", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(6u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(SplitStringUsingSubstrTest, ConsecutiveDelimitersSkipped) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      "unoDELIMITERDELIMITERDELIMITERdosDELIMITERtresDELIMITERDELIMITERcuatro",
      "DELIMITER", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(SplitStringUsingSubstrTest, TrailingDelimitersSkipped) {
  std::vector<std::string> results = SplitStringUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERquatreDELIMITERDELIMITERDELIMITER",
      "DELIMITER", TRIM_WHITESPACE, SPLIT_WANT_ALL);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(
      results, ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

TEST(SplitStringPieceUsingSubstrTest, StringWithNoDelimiter) {
  std::vector<base::StringPiece> results =
      SplitStringPieceUsingSubstr("alongwordwithnodelimiter", "DELIMITER",
                                  base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(1u, results.size());
  EXPECT_THAT(results, ElementsAre("alongwordwithnodelimiter"));
}

TEST(SplitStringPieceUsingSubstrTest, LeadingDelimitersSkipped) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "DELIMITERDELIMITERDELIMITERoneDELIMITERtwoDELIMITERthree", "DELIMITER",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(6u, results.size());
  EXPECT_THAT(results, ElementsAre("", "", "", "one", "two", "three"));
}

TEST(SplitStringPieceUsingSubstrTest, ConsecutiveDelimitersSkipped) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "unoDELIMITERDELIMITERDELIMITERdosDELIMITERtresDELIMITERDELIMITERcuatro",
      "DELIMITER", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results, ElementsAre("uno", "", "", "dos", "tres", "", "cuatro"));
}

TEST(SplitStringPieceUsingSubstrTest, TrailingDelimitersSkipped) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERquatreDELIMITERDELIMITERDELIMITER",
      "DELIMITER", base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(7u, results.size());
  EXPECT_THAT(results,
              ElementsAre("un", "deux", "trois", "quatre", "", "", ""));
}

TEST(SplitStringPieceUsingSubstrTest, KeepWhitespace) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "un DELIMITERdeux\tDELIMITERtrois\nDELIMITERquatre", "DELIMITER",
      base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("un ", "deux\t", "trois\n", "quatre"));
}

TEST(SplitStringPieceUsingSubstrTest, TrimWhitespace) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "un DELIMITERdeux\tDELIMITERtrois\nDELIMITERquatre", "DELIMITER",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(4u, results.size());
  EXPECT_THAT(results, ElementsAre("un", "deux", "trois", "quatre"));
}

TEST(SplitStringPieceUsingSubstrTest, SplitWantAll) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERDELIMITER", "DELIMITER",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(5u, results.size());
  EXPECT_THAT(results, ElementsAre("un", "deux", "trois", "", ""));
}

TEST(SplitStringPieceUsingSubstrTest, SplitWantNonEmpty) {
  std::vector<base::StringPiece> results = SplitStringPieceUsingSubstr(
      "unDELIMITERdeuxDELIMITERtroisDELIMITERDELIMITER", "DELIMITER",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);
  ASSERT_EQ(3u, results.size());
  EXPECT_THAT(results, ElementsAre("un", "deux", "trois"));
}

TEST(StringSplitTest, StringSplitKeepWhitespace) {
  std::vector<std::string> r;

  r = SplitString("   ", "*", base::KEEP_WHITESPACE, base::SPLIT_WANT_ALL);
  ASSERT_EQ(1U, r.size());
  EXPECT_EQ(r[0], "   ");

  r = SplitString("\t  \ta\t ", "\t", base::KEEP_WHITESPACE,
                  base::SPLIT_WANT_ALL);
  ASSERT_EQ(4U, r.size());
  EXPECT_EQ(r[0], "");
  EXPECT_EQ(r[1], "  ");
  EXPECT_EQ(r[2], "a");
  EXPECT_EQ(r[3], " ");

  r = SplitString("\ta\t\nb\tcc", "\n", base::KEEP_WHITESPACE,
                  base::SPLIT_WANT_ALL);
  ASSERT_EQ(2U, r.size());
  EXPECT_EQ(r[0], "\ta\t");
  EXPECT_EQ(r[1], "b\tcc");
}

TEST(StringSplitTest, SplitStringAlongWhitespace) {
  struct TestData {
    const char* input;
    const size_t expected_result_count;
    const char* output1;
    const char* output2;
  } data[] = {
    { "a",       1, "a",  ""   },
    { " ",       0, "",   ""   },
    { " a",      1, "a",  ""   },
    { " ab ",    1, "ab", ""   },
    { " ab c",   2, "ab", "c"  },
    { " ab c ",  2, "ab", "c"  },
    { " ab cd",  2, "ab", "cd" },
    { " ab cd ", 2, "ab", "cd" },
    { " \ta\t",  1, "a",  ""   },
    { " b\ta\t", 2, "b",  "a"  },
    { " b\tat",  2, "b",  "at" },
    { "b\tat",   2, "b",  "at" },
    { "b\t at",  2, "b",  "at" },
  };
  for (const auto& i : data) {
    std::vector<std::string> results =
        base::SplitString(i.input, kWhitespaceASCII, base::KEEP_WHITESPACE,
                          base::SPLIT_WANT_NONEMPTY);
    ASSERT_EQ(i.expected_result_count, results.size());
    if (i.expected_result_count > 0)
      ASSERT_EQ(i.output1, results[0]);
    if (i.expected_result_count > 1)
      ASSERT_EQ(i.output2, results[1]);
  }
}

}  // namespace base
