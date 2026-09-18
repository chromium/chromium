// Copyright 2015 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/test/icu_test_util.h"

#include "base/i18n/icu_util.h"
#include "base/i18n/icubridge/default_icu_locale.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "third_party/icu/source/i18n/unicode/timezone.h"

namespace base::test {

namespace {

// Returns the `LanguageTag` for `locale`, or the locale currently in use if
// `locale` is empty or cannot be parsed.
i18n::LanguageTag GetLanguageTagOrCurrent(const std::string& locale) {
  if (locale.empty()) {
    return i18n::GetDefaultIcuLocale();
  }
  return i18n::GetLanguageTagFromString(locale).value_or(
      i18n::GetDefaultIcuLocale());
}

}  // namespace

ScopedRestoreICUDefaultLocale::ScopedRestoreICUDefaultLocale()
    : ScopedRestoreICUDefaultLocale(std::string()) {}

ScopedRestoreICUDefaultLocale::ScopedRestoreICUDefaultLocale(
    const std::string& locale)
    : scoped_locale_(GetLanguageTagOrCurrent(locale)) {}

ScopedRestoreICUDefaultLocale::~ScopedRestoreICUDefaultLocale() = default;

ScopedRestoreDefaultTimezone::ScopedRestoreDefaultTimezone(const char* zoneid) {
  original_zone_.reset(icu::TimeZone::createDefault());
  icu::TimeZone::adoptDefault(icu::TimeZone::createTimeZone(zoneid));
}

ScopedRestoreDefaultTimezone::~ScopedRestoreDefaultTimezone() {
  icu::TimeZone::adoptDefault(original_zone_.release());
}

void InitializeICUForTesting() {
  i18n::AllowMultipleInitializeCallsForTesting();
  i18n::InitializeICU();
}

}  // namespace base::test
