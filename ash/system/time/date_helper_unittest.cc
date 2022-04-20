// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_helper.h"

#include "ash/test/ash_test_base.h"
#include "base/i18n/rtl.h"

namespace ash {

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
}  // namespace ash
