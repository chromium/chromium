// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_helper.h"

#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"

namespace ash {

namespace {

// These are from "third_party/fontconfig/include/fc-lang/fclang.h" data.
const char* kLocales[] = {
    "aa",       "ab",     "af",     "ak",    "am",    "an",    "ar",
    "as",       "ast",    "av",     "ay",    "az-az", "az-ir", "ba",
    "be",       "ber-dz", "ber-ma", "bg",    "bh",    "bho",   "bi",
    "bin",      "bm",     "bn",     "bo",    "br",    "brx",   "bs",
    "bua",      "byn",    "ca",     "ce",    "ch",    "chm",   "chr",
    "co",       "crh",    "cs",     "csb",   "cu",    "cv",    "cy",
    "da",       "de",     "doi",    "dv",    "dz",    "ee",    "el",
    "en",       "eo",     "es",     "et",    "eu",    "fa",    "fat",
    "ff",       "fi",     "fil",    "fj",    "fo",    "fr",    "fur",
    "fy",       "ga",     "gd",     "gez",   "gl",    "gn",    "gu",
    "gv",       "ha",     "haw",    "he",    "hi",    "hne",   "ho",
    "hr",       "hsb",    "ht",     "hu",    "hy",    "hz",    "ia",
    "id",       "ie",     "ig",     "ii",    "ik",    "io",    "is",
    "it",       "iu",     "ja",     "jv",    "ka",    "kaa",   "kab",
    "ki",       "kj",     "kk",     "kl",    "km",    "kn",    "ko",
    "kok",      "kr",     "ks",     "ku-am", "ku-iq", "ku-ir", "ku-tr",
    "kum",      "kv",     "kw",     "kwm",   "ky",    "la",    "lah",
    "lb",       "lez",    "lg",     "li",    "ln",    "lo",    "lt",
    "lv",       "mai",    "mg",     "mh",    "mi",    "mk",    "ml",
    "mn-cn",    "mn-mn",  "mni",    "mo",    "mr",    "ms",    "mt",
    "my",       "na",     "nb",     "nds",   "ne",    "ng",    "nl",
    "nn",       "no",     "nqo",    "nr",    "nso",   "nv",    "ny",
    "oc",       "om",     "or",     "os",    "ota",   "pa",    "pa-pk",
    "pap-an",   "pap-aw", "pl",     "ps-af", "ps-pk", "pt",    "qu",
    "quz",      "rm",     "rn",     "ro",    "ru",    "rw",    "sa",
    "sah",      "sat",    "sc",     "sco",   "sd",    "se",    "sel",
    "sg",       "sh",     "shs",    "si",    "sid",   "sk",    "sl",
    "sm",       "sma",    "smj",    "smn",   "sms",   "sn",    "so",
    "sq",       "sr",     "ss",     "st",    "su",    "sv",    "sw",
    "syr",      "ta",     "te",     "tg",    "th",    "ti-er", "ti-et",
    "tig",      "tk",     "tl",     "tn",    "to",    "tr",    "ts",
    "tt",       "tw",     "ty",     "tyv",   "ug",    "uk",    "und-zmth",
    "und-zsye", "ur",     "uz",     "ve",    "vi",    "vo",    "vot",
    "wa",       "wal",    "wen",    "wo",    "xh",    "yap",   "yi",
    "yo",       "za",     "zh-cn",  "zh-hk", "zh-mo", "zh-sg", "zh-tw",
    "zu"};
}  // namespace

class DateHelperUnittest : public AshTestBase {
 public:
  DateHelperUnittest() = default;
  DateHelperUnittest(const DateHelperUnittest& other) = delete;
  DateHelperUnittest& operator=(const DateHelperUnittest& other) = delete;
  ~DateHelperUnittest() override = default;

  void SetDefaultLocale(const std::string& lang) {
    base::i18n::SetICUDefaultLocale(lang);
    DateHelper::GetInstance()->ResetFormatters();
    DateHelper::GetInstance()->CalculateLocalWeekTitles();
  }

  std::u16string Format12HrClockInterval(const base::Time& start_time,
                                         const base::Time& end_time) {
    return DateHelper::GetInstance()->GetFormattedInterval(
        DateHelper::GetInstance()->twelve_hour_clock_interval_formatter(),
        start_time, end_time);
  }

  std::u16string Format24HrClockInterval(const base::Time& start_time,
                                         const base::Time& end_time) {
    return DateHelper::GetInstance()->GetFormattedInterval(
        DateHelper::GetInstance()->twenty_four_hour_clock_interval_formatter(),
        start_time, end_time);
  }
};

// Gets the calendar week titles in different language and order.
TEST_F(DateHelperUnittest, GetWeekTitle) {
  SetDefaultLocale("zh-CN");
  std::vector<std::u16string> week_titles =
      DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"日", week_titles[0]);  // Sunday
  EXPECT_EQ(u"一", week_titles[1]);  // Monday
  EXPECT_EQ(u"二", week_titles[2]);  // Tuesday
  EXPECT_EQ(u"三", week_titles[3]);  // Wednesday
  EXPECT_EQ(u"四", week_titles[4]);  // Thursday
  EXPECT_EQ(u"五", week_titles[5]);  // Friday
  EXPECT_EQ(u"六", week_titles[6]);  // Saturday

  SetDefaultLocale("ar");
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"س", week_titles[0]);  // s
  EXPECT_EQ(u"ح", week_titles[1]);  // h
  EXPECT_EQ(u"ن", week_titles[2]);  // n
  EXPECT_EQ(u"ث", week_titles[3]);  // w
  EXPECT_EQ(u"ر", week_titles[4]);  // R
  EXPECT_EQ(u"خ", week_titles[5]);  // Kh
  EXPECT_EQ(u"ج", week_titles[6]);  // c

  SetDefaultLocale("ko");
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"일", week_titles[0]);
  EXPECT_EQ(u"월", week_titles[1]);
  EXPECT_EQ(u"화", week_titles[2]);
  EXPECT_EQ(u"수", week_titles[3]);
  EXPECT_EQ(u"목", week_titles[4]);
  EXPECT_EQ(u"금", week_titles[5]);
  EXPECT_EQ(u"토", week_titles[6]);

  SetDefaultLocale("en_US");
  week_titles = DateHelper::GetInstance()->week_titles();
  EXPECT_EQ(u"S", week_titles[0]);
  EXPECT_EQ(u"M", week_titles[1]);
  EXPECT_EQ(u"T", week_titles[2]);
  EXPECT_EQ(u"W", week_titles[3]);
  EXPECT_EQ(u"T", week_titles[4]);
  EXPECT_EQ(u"F", week_titles[5]);
  EXPECT_EQ(u"S", week_titles[6]);
}

// Tests getting the calendar week titles in all languages.
TEST_F(DateHelperUnittest, GetWeekTitleForAllLocales) {
  for (auto* local : kLocales) {
    SetDefaultLocale(local);
    EXPECT_EQ(7U, DateHelper::GetInstance()->week_titles().size());
  }
}

// Formats the interval between two dates in different languages.
TEST_F(DateHelperUnittest, GetFormattedInterval) {
  ash::system::TimezoneSettings::GetInstance()->SetTimezoneFromID(u"GMT");

  base::Time date1;  // Start date
  base::Time date2;  // End date, same meridiem
  base::Time date3;  // End date, different meridiem
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 10:00 GMT", &date1));
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 11:45 GMT", &date2));
  ASSERT_TRUE(base::Time::FromString("22 Nov 2021 22:30 GMT", &date3));

  SetDefaultLocale("en_US");
  EXPECT_EQ(u"10:00 – 11:45 AM", Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 AM – 10:30 PM", Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00 – 11:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 – 22:30", Format24HrClockInterval(date1, date3));

  SetDefaultLocale("zh_Hant");
  EXPECT_EQ(u"上午10:00至11:45", Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"上午10:00至晚上10:30", Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00 – 11:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 – 22:30", Format24HrClockInterval(date1, date3));

  SetDefaultLocale("ar");
  EXPECT_EQ(u"10:00–11:45 ص", Format12HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00 ص – 10:30 م", Format12HrClockInterval(date1, date3));
  EXPECT_EQ(u"10:00–11:45", Format24HrClockInterval(date1, date2));
  EXPECT_EQ(u"10:00–22:30", Format24HrClockInterval(date1, date3));
}

}  // namespace ash
