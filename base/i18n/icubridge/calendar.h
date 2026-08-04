// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_CALENDAR_H_
#define BASE_I18N_ICUBRIDGE_CALENDAR_H_

#include <set>

#include "base/i18n/base_i18n_export.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/types/pass_key.h"

namespace base::i18n {

class BASE_I18N_EXPORT LanguageTag;

// IcuBridge::Calendar provides calendar-related localization utilities,
// such as getting localized week information (first weekday and weekend).
class BASE_I18N_EXPORT IcuBridge::Calendar {
 public:
  enum class Weekday {
    kSunday = 1,
    kMonday = 2,
    kTuesday = 3,
    kWednesday = 4,
    kThursday = 5,
    kFriday = 6,
    kSaturday = 7,
  };
  using WeekDay = Weekday;

  struct BASE_I18N_EXPORT WeekInformation {
    Weekday first_weekday;
    std::set<Weekday> weekend;

    bool operator==(const WeekInformation&) const = default;
  };

  // Returns week information for the default locale.
  WeekInformation GetWeekInformation() const;

  explicit Calendar(base::PassKey<IcuBridge>) {}
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_CALENDAR_H_
