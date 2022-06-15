// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_string_util.h"

#include "ash/system/time/date_helper.h"
#include "base/i18n/rtl.h"
#include "testing/gtest/include/gtest/gtest.h"

TEST(FileManagerStringUtilTest, GetLocaleBasedWeekStart) {
  const struct DateTestData {
    const char* locale;
    int start_of_week;
  } kDateTestData[] = {
      // Australia uses Monday as the start day of the week.
      {"en_AU", 1},
      // United States uses Sunday as the start day of the week.
      {"en_US", 0},
      // Afghanistan uses Saturday as the start day of the week.
      {"ps-AF", 6}};

  for (const auto& test : kDateTestData) {
    base::i18n::SetICUDefaultLocale(test.locale);
    ash::DateHelper::GetInstance()->ResetFormatters();
    EXPECT_EQ(GetLocaleBasedWeekStart(), test.start_of_week);
  }
}
