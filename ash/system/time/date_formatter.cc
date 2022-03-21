// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/system/time/date_formatter.h"

#include "base/i18n/unicodestring.h"

namespace ash {

// static
DateFormatter* DateFormatter::GetInstance() {
  return base::Singleton<DateFormatter>::get();
}

icu::SimpleDateFormat DateFormatter::CreateSimpleDateFormatter(
    const char* pattern) {
  // Generate a locale-dependent format pattern. The generator will take
  // care of locale-dependent formatting issues like which separator to
  // use (some locales use '.' instead of ':'), and where to put the am/pm
  // marker.=
  UErrorCode status = U_ZERO_ERROR;
  DCHECK(U_SUCCESS(status));
  std::unique_ptr<icu::DateTimePatternGenerator> generator(
      icu::DateTimePatternGenerator::createInstance(status));
  DCHECK(U_SUCCESS(status));
  icu::UnicodeString generated_pattern =
      generator->getBestPattern(icu::UnicodeString(pattern), status);
  DCHECK(U_SUCCESS(status));

  // Then, format the time using the generated pattern.
  icu::SimpleDateFormat formatter(generated_pattern, status);
  DCHECK(U_SUCCESS(status));

  return formatter;
}

std::u16string DateFormatter::GetFormattedTime(const icu::DateFormat* formatter,
                                               const base::Time& time) {
  DCHECK(formatter);
  icu::UnicodeString date_string;

  formatter->format(static_cast<UDate>(time.ToDoubleT() * 1000), date_string);
  return base::i18n::UnicodeStringToString16(date_string);
}

DateFormatter::DateFormatter()
    : day_of_month_formatter_(CreateSimpleDateFormatter("d")),
      month_day_formatter_(CreateSimpleDateFormatter("MMMMd")),
      month_day_year_formatter_(CreateSimpleDateFormatter("MMMMdyyyy")),
      month_name_formatter_(CreateSimpleDateFormatter("MMMM")),
      month_name_year_formatter_(CreateSimpleDateFormatter("MMMM yyyy")),
      time_zone_formatter_(CreateSimpleDateFormatter("zzzz")),
      twelve_hour_clock_formatter_(CreateSimpleDateFormatter("h:mm a")),
      year_formatter_(CreateSimpleDateFormatter("YYYY")) {}

DateFormatter::~DateFormatter() = default;

}  // namespace ash
