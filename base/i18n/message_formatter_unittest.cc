// Copyright 2015 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/message_formatter.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/strings/string_piece.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/msgfmt.h"

typedef testing::Test MessageFormatterTest;

namespace base {
namespace i18n {

class MessageFormatterTest : public testing::Test {
 protected:
  MessageFormatterTest() {
    original_locale_ = GetConfiguredLocale();
    SetICUDefaultLocale("en-US");
  }
  ~MessageFormatterTest() override {
    SetICUDefaultLocale(original_locale_);
  }

 private:
  std::string original_locale_;
};

namespace {

void AppendFormattedDateTime(const std::unique_ptr<icu::DateFormat>& df,
                             const Time& now,
                             std::string* result) {
  icu::UnicodeString formatted;
  df->format(static_cast<UDate>(now.ToJsTime()), formatted).
      toUTF8String(*result);
}

}  // namespace

TEST_F(MessageFormatterTest, PluralNamedArgs) {
  const std::u16string pattern = ASCIIToUTF16(
      "{num_people, plural, "
      "=0 {I met nobody in {place}.}"
      "=1 {I met a person in {place}.}"
      "other {I met # people in {place}.}}");

  std::string result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris"));
  EXPECT_EQ("I met nobody in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 1, "place", "Paris"));
  EXPECT_EQ("I met a person in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 5, "place", "Paris"));
  EXPECT_EQ("I met 5 people in Paris.", result);
}

TEST_F(MessageFormatterTest, PluralNamedArgsWithOffset) {
  const std::u16string pattern = ASCIIToUTF16(
      "{num_people, plural, offset:1 "
      "=0 {I met nobody in {place}.}"
      "=1 {I met {person} in {place}.}"
      "=2 {I met {person} and one other person in {place}.}"
      "=13 {I met {person} and a dozen other people in {place}.}"
      "other {I met {person} and # other people in {place}.}}");

  std::string result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris"));
  EXPECT_EQ("I met nobody in Paris.", result);
  // {person} is ignored if {num_people} is 0.
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris", "person", "Peter"));
  EXPECT_EQ("I met nobody in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 1, "place", "Paris", "person", "Peter"));
  EXPECT_EQ("I met Peter in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 2, "place", "Paris", "person", "Peter"));
  EXPECT_EQ("I met Peter and one other person in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 13, "place", "Paris", "person", "Peter"));
  EXPECT_EQ("I met Peter and a dozen other people in Paris.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 50, "place", "Paris", "person", "Peter"));
  EXPECT_EQ("I met Peter and 49 other people in Paris.", result);
}

TEST_F(MessageFormatterTest, PluralNumberedArgs) {
  const std::u16string pattern = ASCIIToUTF16(
      "{1, plural, "
      "=1 {The cert for {0} expired yesterday.}"
      "=7 {The cert for {0} expired a week ago.}"
      "other {The cert for {0} expired # days ago.}}");

  std::string result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "example.com", 1));
  EXPECT_EQ("The cert for example.com expired yesterday.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "example.com", 7));
  EXPECT_EQ("The cert for example.com expired a week ago.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "example.com", 15));
  EXPECT_EQ("The cert for example.com expired 15 days ago.", result);
}

TEST_F(MessageFormatterTest, PluralNumberedArgsWithDate) {
  const std::u16string pattern = ASCIIToUTF16(
      "{1, plural, "
      "=1 {The cert for {0} expired yesterday. Today is {2,date,full}}"
      "other {The cert for {0} expired # days ago. Today is {2,date,full}}}");

  base::Time now = base::Time::Now();
  using icu::DateFormat;
  std::unique_ptr<DateFormat> df(
      DateFormat::createDateInstance(DateFormat::FULL));
  std::string second_sentence = " Today is ";
  AppendFormattedDateTime(df, now, &second_sentence);

  std::string result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "example.com", 1, now));
  EXPECT_EQ("The cert for example.com expired yesterday." + second_sentence,
            result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "example.com", 15, now));
  EXPECT_EQ("The cert for example.com expired 15 days ago." + second_sentence,
            result);
}

TEST_F(MessageFormatterTest, DateTimeAndNumber) {
  // Note that using 'mph' for all locales is not a good i18n practice.
  const std::u16string pattern = ASCIIToUTF16(
      "At {0,time, short} on {0,date, medium}, "
      "there was {1} at building {2,number,integer}. "
      "The speed of the wind was {3,number,###.#} mph.");

  using icu::DateFormat;
  std::unique_ptr<DateFormat> tf(
      DateFormat::createTimeInstance(DateFormat::SHORT));
  std::unique_ptr<DateFormat> df(
      DateFormat::createDateInstance(DateFormat::MEDIUM));

  base::Time now = base::Time::Now();
  std::string expected = "At ";
  AppendFormattedDateTime(tf, now, &expected);
  expected.append(" on ");
  AppendFormattedDateTime(df, now, &expected);
  expected.append(", there was an explosion at building 3. "
                  "The speed of the wind was 37.4 mph.");

  EXPECT_EQ(expected, UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, now, "an explosion", 3, 37.413)));
}

TEST_F(MessageFormatterTest, SelectorSingleOrMultiple) {
  const std::u16string pattern = ASCIIToUTF16(
      "{0, select,"
      "single {Select a file to upload.}"
      "multiple {Select files to upload.}"
      "other {UNUSED}}");

  std::string result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "single"));
  EXPECT_EQ("Select a file to upload.", result);
  result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "multiple"));
  EXPECT_EQ("Select files to upload.", result);

  // fallback if a parameter is not selectors specified in the message pattern.
  result = UTF16ToASCII(MessageFormatter::FormatWithNumberedArgs(
      pattern, "foobar"));
  EXPECT_EQ("UNUSED", result);
}

}  // namespace i18n
}  // namespace base
