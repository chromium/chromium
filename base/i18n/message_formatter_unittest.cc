// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/message_formatter.h"

#include <memory>

#include "base/i18n/rtl.h"
#include "base/i18n/unicodestring.h"
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
                             std::u16string* result) {
  icu::UnicodeString formatted;
  result->append(UnicodeStringToString16(df->format(
      static_cast<UDate>(now.InMillisecondsFSinceUnixEpoch()), formatted)));
}

}  // namespace

TEST_F(MessageFormatterTest, PluralNamedArgs) {
  const std::u16string pattern =
      u"{num_people, plural, "
      u"=0 {I met nobody in {place}.}"
      u"=1 {I met a person in {place}.}"
      u"other {I met # people in {place}.}}";

  std::u16string result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris");
  EXPECT_EQ(u"I met nobody in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(pattern, "num_people", 1,
                                                 "place", "Paris");
  EXPECT_EQ(u"I met a person in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(pattern, "num_people", 5,
                                                 "place", "Paris");
  EXPECT_EQ(u"I met 5 people in Paris.", result);
}

TEST_F(MessageFormatterTest, PluralNamedArgsWithOffset) {
  const std::u16string pattern =
      u"{num_people, plural, offset:1 "
      u"=0 {I met nobody in {place}.}"
      u"=1 {I met {person} in {place}.}"
      u"=2 {I met {person} and one other person in {place}.}"
      u"=13 {I met {person} and a dozen other people in {place}.}"
      u"other {I met {person} and # other people in {place}.}}";

  std::u16string result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris");
  EXPECT_EQ(u"I met nobody in Paris.", result);
  // {person} is ignored if {num_people} is 0.
  result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 0, "place", "Paris", "person", "Peter");
  EXPECT_EQ(u"I met nobody in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 1, "place", "Paris", "person", "Peter");
  EXPECT_EQ(u"I met Peter in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 2, "place", "Paris", "person", "Peter");
  EXPECT_EQ(u"I met Peter and one other person in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 13, "place", "Paris", "person", "Peter");
  EXPECT_EQ(u"I met Peter and a dozen other people in Paris.", result);
  result = MessageFormatter::FormatWithNamedArgs(
      pattern, "num_people", 50, "place", "Paris", "person", "Peter");
  EXPECT_EQ(u"I met Peter and 49 other people in Paris.", result);
}

TEST_F(MessageFormatterTest, PluralNumberedArgs) {
  const std::u16string pattern =
      u"{1, plural, "
      u"=1 {The cert for {0} expired yesterday.}"
      u"=7 {The cert for {0} expired a week ago.}"
      u"other {The cert for {0} expired # days ago.}}";

  std::u16string result =
      MessageFormatter::FormatWithNumberedArgs(pattern, "example.com", 1);
  EXPECT_EQ(u"The cert for example.com expired yesterday.", result);
  result = MessageFormatter::FormatWithNumberedArgs(pattern, "example.com", 7);
  EXPECT_EQ(u"The cert for example.com expired a week ago.", result);
  result = MessageFormatter::FormatWithNumberedArgs(pattern, "example.com", 15);
  EXPECT_EQ(u"The cert for example.com expired 15 days ago.", result);
}

TEST_F(MessageFormatterTest, PluralNumberedArgsWithDate) {
  const std::u16string pattern =
      u"{1, plural, "
      u"=1 {The cert for {0} expired yesterday. Today is {2,date,full}}"
      u"other {The cert for {0} expired # days ago. Today is {2,date,full}}}";

  base::Time now = base::Time::Now();
  using icu::DateFormat;
  std::unique_ptr<DateFormat> df(
      DateFormat::createDateInstance(DateFormat::FULL));
  std::u16string second_sentence = u" Today is ";
  AppendFormattedDateTime(df, now, &second_sentence);

  std::u16string result =
      MessageFormatter::FormatWithNumberedArgs(pattern, "example.com", 1, now);
  EXPECT_EQ(u"The cert for example.com expired yesterday." + second_sentence,
            result);
  result =
      MessageFormatter::FormatWithNumberedArgs(pattern, "example.com", 15, now);
  EXPECT_EQ(u"The cert for example.com expired 15 days ago." + second_sentence,
            result);
}

TEST_F(MessageFormatterTest, DateTimeAndNumber) {
  // Note that using 'mph' for all locales is not a good i18n practice.
  const std::u16string pattern =
      u"At {0,time, short} on {0,date, medium}, "
      u"there was {1} at building {2,number,integer}. "
      u"The speed of the wind was {3,number,###.#} mph.";

  using icu::DateFormat;
  std::unique_ptr<DateFormat> tf(
      DateFormat::createTimeInstance(DateFormat::SHORT));
  std::unique_ptr<DateFormat> df(
      DateFormat::createDateInstance(DateFormat::MEDIUM));

  base::Time now = base::Time::Now();
  std::u16string expected = u"At ";
  AppendFormattedDateTime(tf, now, &expected);
  expected.append(u" on ");
  AppendFormattedDateTime(df, now, &expected);
  expected.append(
      u", there was an explosion at building 3. "
      "The speed of the wind was 37.4 mph.");

  EXPECT_EQ(expected, MessageFormatter::FormatWithNumberedArgs(
                          pattern, now, "an explosion", 3, 37.413));
}

TEST_F(MessageFormatterTest, SelectorSingleOrMultiple) {
  const std::u16string pattern =
      u"{0, select,"
      u"single {Select a file to upload.}"
      u"multiple {Select files to upload.}"
      u"other {UNUSED}}";

  std::u16string result =
      MessageFormatter::FormatWithNumberedArgs(pattern, "single");
  EXPECT_EQ(u"Select a file to upload.", result);
  result = MessageFormatter::FormatWithNumberedArgs(pattern, "multiple");
  EXPECT_EQ(u"Select files to upload.", result);

  // fallback if a parameter is not selectors specified in the message pattern.
  result = MessageFormatter::FormatWithNumberedArgs(pattern, "foobar");
  EXPECT_EQ(u"UNUSED", result);
}

}  // namespace i18n
}  // namespace base
