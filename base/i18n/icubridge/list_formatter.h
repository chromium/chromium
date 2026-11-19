// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICUBRIDGE_LIST_FORMATTER_H_
#define BASE_I18N_ICUBRIDGE_LIST_FORMATTER_H_

#include <memory>
#include <string>

#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/icubridge/icu_bridge.h"
#include "base/i18n/language_tag.h"
#include "base/memory/ref_counted.h"
#include "base/types/pass_key.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"

namespace base::i18n {

// `IcuBridge::ListFormatter` provides locale-aware list formatting utilities.
//
// Each instance is bound to the locale it was created with. Instances are
// cached by `IcuBridge` and obtained via `IcuBridge::list_formatter()`, which
// returns the formatter for the default ICU locale (`GetDefaultIcuLocale()`),
// or via `IcuBridge::list_formatter(locale)` for a specific locale.
//
// These methods format conjunction lists (e.g. "A, B, and C") and unit lists.
// If a disjunction list is ever needed to display "A, B, or C", a similar
// helper using `ULISTFMT_TYPE_OR` should be introduced.
class BASE_I18N_EXPORT IcuBridge::ListFormatter
    : public base::RefCountedThreadSafe<IcuBridge::ListFormatter> {
 public:
  ListFormatter(base::PassKey<IcuBridge>, const LanguageTag& locale);

  ListFormatter(const ListFormatter&) = delete;
  ListFormatter& operator=(const ListFormatter&) = delete;

  // Formats `items` using the standard conjunction pattern with "and" (e.g.
  // "A, B, and C" in English, "A, B et C" in French).
  std::u16string FormatStandardList(
      base::span<const std::u16string> items) const;

  // Formats `items` using the narrow conjunction pattern without "and" in
  // English (e.g. "A, B, C" in English, "A、B、C" in Japanese/Chinese).
  std::u16string FormatNarrowList(base::span<const std::u16string> items) const;

  // Formats `items` using the unit list pattern for measurements (e.g.
  // "3 ft, 7 in").
  std::u16string FormatUnitList(base::span<const std::u16string> items) const;

 private:
  friend class base::RefCountedThreadSafe<ListFormatter>;
  ~ListFormatter();

  const icu::Locale icu_locale_;
  const std::unique_ptr<icu::ListFormatter> standard_formatter_;
  const std::unique_ptr<icu::ListFormatter> narrow_formatter_;
  const std::unique_ptr<icu::ListFormatter> unit_formatter_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICUBRIDGE_LIST_FORMATTER_H_
