// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/calendar_utils.h"

#include "base/i18n/unicodestring.h"
#include "third_party/icu/source/i18n/unicode/datefmt.h"
#include "third_party/icu/source/i18n/unicode/dtptngen.h"
#include "third_party/icu/source/i18n/unicode/smpdtfmt.h"

namespace ash {

namespace calendar_utils {

bool IsToday(const base::Time::Exploded& selected_date) {
  base::Time::Exploded today_exploded = GetExploded(base::Time::Now());
  return selected_date.year == today_exploded.year &&
         selected_date.month == today_exploded.month &&
         selected_date.day_of_month == today_exploded.day_of_month;
}

base::Time::Exploded GetExploded(const base::Time& date) {
  base::Time::Exploded exploded;
  date.LocalExplode(&exploded);
  return exploded;
}

std::u16string GetMonthName(const base::Time date) {
  // Inits status with no error, no warning.
  UErrorCode status = U_ZERO_ERROR;

  // Generates the "MMMM" pattern.
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(UDAT_MONTH), status);
  DCHECK(U_SUCCESS(status));

  // Then, creates the formatter and formats `date` to string with the pattern.
  auto dfmt =
      std::make_unique<icu::SimpleDateFormat>(generated_pattern, status);
  UDate unicode_date = static_cast<UDate>(date.ToDoubleT() * 1000);
  icu::UnicodeString unicode_string;
  unicode_string = dfmt->format(unicode_date, unicode_string);

  return base::i18n::UnicodeStringToString16(unicode_string);
}

}  // namespace calendar_utils

}  // namespace ash
