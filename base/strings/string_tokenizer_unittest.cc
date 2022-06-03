// Copyright (c) 2006-2008 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/strings/string_tokenizer.h"

#include "testing/gtest/include/gtest/gtest.h"

using std::string;

namespace base {

namespace {

TEST(StringTokenizerTest, Simple) {
  string input = "this is a test";
  StringTokenizer t(input, " ");
  // The start of string, before returning any tokens, is considered a
  // delimiter.
  EXPECT_TRUE(t.token_is_delim());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("this", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("is", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("a", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("test", t.token());

  EXPECT_FALSE(t.GetNext());
  // The end of string, after the last token tokens, is considered a delimiter.
  EXPECT_TRUE(t.token_is_delim());
}

TEST(StringTokenizerTest, Reset) {
  string input = "this is a test";
  StringTokenizer t(input, " ");

  for (int i = 0; i < 2; ++i) {
    EXPECT_TRUE(t.token_is_delim());

    EXPECT_TRUE(t.GetNext());
    EXPECT_FALSE(t.token_is_delim());
    EXPECT_EQ("this", t.token());

    EXPECT_TRUE(t.GetNext());
    EXPECT_FALSE(t.token_is_delim());
    EXPECT_EQ("is", t.token());

    EXPECT_TRUE(t.GetNext());
    EXPECT_FALSE(t.token_is_delim());
    EXPECT_EQ("a", t.token());

    EXPECT_TRUE(t.GetNext());
    EXPECT_FALSE(t.token_is_delim());
    EXPECT_EQ("test", t.token());

    EXPECT_FALSE(t.GetNext());
    EXPECT_TRUE(t.token_is_delim());

    t.Reset();
  }
}

TEST(StringTokenizerTest, RetDelims) {
  string input = "this is a test";
  StringTokenizer t(input, " ");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  EXPECT_TRUE(t.token_is_delim());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("this", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("is", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("a", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("test", t.token());

  EXPECT_FALSE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
}

TEST(StringTokenizerTest, RetEmptyTokens) {
  string input = "foo='a, b',,bar,,baz,quux";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_EMPTY_TOKENS);
  t.set_quote_chars("'");

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("foo='a, b'", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("baz", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("quux", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, RetEmptyTokens_AtStart) {
  string input = ",bar";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_EMPTY_TOKENS);
  t.set_quote_chars("'");

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, RetEmptyTokens_AtEnd) {
  string input = "bar,";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_EMPTY_TOKENS);
  t.set_quote_chars("'");

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, RetEmptyTokens_Both) {
  string input = ",";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_EMPTY_TOKENS);
  t.set_quote_chars("'");

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, RetEmptyTokens_Empty) {
  string input = "";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_EMPTY_TOKENS);

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, RetDelimsAndEmptyTokens) {
  string input = "foo='a, b',,bar,,baz,quux";
  StringTokenizer t(input, ",");
  t.set_options(StringTokenizer::RETURN_DELIMS |
                StringTokenizer::RETURN_EMPTY_TOKENS);
  t.set_quote_chars("'");

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("foo='a, b'", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ(",", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ(",", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ(",", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ(",", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("baz", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ(",", t.token());

  ASSERT_TRUE(t.GetNext());
  EXPECT_EQ("quux", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ManyDelims) {
  string input = "this: is, a-test";
  StringTokenizer t(input, ": ,-");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("this", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("is", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("a", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("test", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseHeader) {
  string input = "Content-Type: text/html ; charset=UTF-8";
  StringTokenizer t(input, ": ;=");
  t.set_options(StringTokenizer::RETURN_DELIMS);
  EXPECT_TRUE(t.token_is_delim());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("Content-Type", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(":", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("text/html", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(";", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ(" ", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("charset", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
  EXPECT_EQ("=", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_FALSE(t.token_is_delim());
  EXPECT_EQ("UTF-8", t.token());

  EXPECT_FALSE(t.GetNext());
  EXPECT_TRUE(t.token_is_delim());
}

TEST(StringTokenizerTest, ParseQuotedString) {
  string input = "foo bar 'hello world' baz";
  StringTokenizer t(input, " ");
  t.set_quote_chars("'");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("foo", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("'hello world'", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("baz", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseQuotedString_Malformed) {
  string input = "bar 'hello wo";
  StringTokenizer t(input, " ");
  t.set_quote_chars("'");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("'hello wo", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseQuotedString_Multiple) {
  string input = "bar 'hel\"lo\" wo' baz\"";
  StringTokenizer t(input, " ");
  t.set_quote_chars("'\"");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("'hel\"lo\" wo'", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("baz\"", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseQuotedString_EscapedQuotes) {
  string input = "foo 'don\\'t do that'";
  StringTokenizer t(input, " ");
  t.set_quote_chars("'");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("foo", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("'don\\'t do that'", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseQuotedString_EscapedQuotes2) {
  string input = "foo='a, b', bar";
  StringTokenizer t(input, ", ");
  t.set_quote_chars("'");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("foo='a, b'", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseWithWhitespace_NoQuotes) {
  string input = "\t\t\t     foo=a,\r\n b,\r\n\t\t\t      bar\t ";
  StringTokenizer t(input, ",", StringTokenizer::WhitespacePolicy::kSkipOver);

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("foo=a", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("b", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_FALSE(t.GetNext());
}

TEST(StringTokenizerTest, ParseWithWhitespace_Quotes) {
  string input = "\t\t\t     foo='a, b',\t\t\t      bar\t ";
  StringTokenizer t(input, ",", StringTokenizer::WhitespacePolicy::kSkipOver);
  t.set_quote_chars("'");

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("foo='a, b'", t.token());

  EXPECT_TRUE(t.GetNext());
  EXPECT_EQ("bar", t.token());

  EXPECT_FALSE(t.GetNext());
}

}  // namespace

}  // namespace base
