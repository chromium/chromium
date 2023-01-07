// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/file_manager/file_manager_string_util.h"

#include "ash/system/time/date_helper.h"
#include "base/i18n/rtl.h"
#include "chrome/test/base/chrome_ash_test_base.h"
#include "chromeos/ash/components/settings/scoped_timezone_settings.h"
#include "testing/gtest/include/gtest/gtest.h"

class FileManagerStringUtilTest : public ChromeAshTestBase {
 public:
  FileManagerStringUtilTest(const FileManagerStringUtilTest&) = delete;
  FileManagerStringUtilTest& operator=(const FileManagerStringUtilTest&) =
      delete;
  FileManagerStringUtilTest() = default;
  ~FileManagerStringUtilTest() override = default;

  void SetDefaultLocale(const std::string& locale) {
    base::i18n::SetICUDefaultLocale(locale);
    ash::DateHelper::GetInstance()->ResetForTesting();
  }
};

TEST_F(FileManagerStringUtilTest, GetLocaleBasedWeekStart_Locale) {
  const struct DateTestData {
    const char* locale;
    int start_of_week;
  } kDateTestData[] = {
      // Australia uses Monday as the start day of the week.
      {"en_AU", 1},
      // United States uses Sunday as the start day of the week.
      {"en_US", 0},
      // Afghanistan uses Saturday as the start day of the week.
      {"ps-AF", 6},
      {"pa-PK", 0}};

  for (const auto& test : kDateTestData) {
    SetDefaultLocale(test.locale);
    EXPECT_EQ(GetLocaleBasedWeekStart(), test.start_of_week);
  }
}

TEST_F(FileManagerStringUtilTest, GetLocaleBasedWeekStart_Timezone) {
  const std::u16string kTimezoneIds[] = {
      u"GMT",      u"PST",      u"GMT+0800", u"GMT+1000", u"GMT+1300",
      u"GMT+1400", u"GMT-0800", u"GMT-1100", u"GMT-1200",
  };

  SetDefaultLocale("en_AU");
  for (const auto& timezone_id : kTimezoneIds) {
    ash::system::ScopedTimezoneSettings timezone_settings(timezone_id);
    // Regardless the timezone, week always starts with Monday for AU locale.
    EXPECT_EQ(GetLocaleBasedWeekStart(), 1);
  }
}
