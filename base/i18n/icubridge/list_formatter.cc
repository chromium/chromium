// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icubridge/list_formatter.h"

#include <stdint.h>

#include <memory>
#include <string>
#include <vector>

#include "base/containers/span.h"
#include "base/i18n/icu4c_tag_converter.h"  // nogncheck
#include "base/i18n/icubridge/icu_bridge_helpers.h"
#include "base/i18n/language_tag.h"
#include "base/numerics/safe_conversions.h"
#include "base/strings/string_util.h"
#include "third_party/icu/source/common/unicode/locid.h"
#include "third_party/icu/source/common/unicode/unistr.h"
#include "third_party/icu/source/common/unicode/utypes.h"
#include "third_party/icu/source/i18n/unicode/listformatter.h"
#include "third_party/icu/source/i18n/unicode/ulistformatter.h"

namespace base::i18n {

namespace {

// Creates an `icu::ListFormatter` for `icu_locale`, `u_type`, and `u_width`,
// or returns `nullptr` if ICU fails.
std::unique_ptr<icu::ListFormatter> CreateIcuListFormatter(
    const icu::Locale& icu_locale,
    UListFormatterType u_type,
    UListFormatterWidth u_width) {
  UErrorCode error = U_ZERO_ERROR;
  std::unique_ptr<icu::ListFormatter> formatter(
      icu::ListFormatter::createInstance(icu_locale, u_type, u_width, error));
  if (U_FAILURE(error)) {
    return nullptr;
  }
  return formatter;
}

// Formats `items` with `formatter`, falling back to joining with `u", "` if
// `formatter` is null or ICU fails.
std::u16string FormatListInternal(base::span<const std::u16string> items,
                                  const icu::ListFormatter* formatter) {
  if (items.empty()) {
    return std::u16string();
  }
  if (!formatter) {
    return base::JoinString(items, u", ");
  }
  std::vector<icu::UnicodeString> strings;
  strings.reserve(items.size());
  for (const std::u16string& item : items) {
    strings.emplace_back(item.data(), base::checked_cast<int32_t>(item.size()));
  }
  UErrorCode error = U_ZERO_ERROR;
  icu::UnicodeString formatted;
  formatter->format(strings.data(), base::checked_cast<int32_t>(strings.size()),
                    formatted, error);
  if (U_FAILURE(error)) {
    return base::JoinString(items, u", ");
  }
  return UnicodeStringToString16(formatted);
}

}  // namespace

IcuBridge::ListFormatter::ListFormatter(base::PassKey<IcuBridge>,
                                        const LanguageTag& locale)
    : icu_locale_(IcuLocaleConverter::GetInstance().FromLanguageTag(locale)),
      standard_formatter_(CreateIcuListFormatter(icu_locale_,
                                                 ULISTFMT_TYPE_AND,
                                                 ULISTFMT_WIDTH_WIDE)),
      narrow_formatter_(CreateIcuListFormatter(icu_locale_,
                                               ULISTFMT_TYPE_AND,
                                               ULISTFMT_WIDTH_NARROW)),
      unit_formatter_(CreateIcuListFormatter(icu_locale_,
                                             ULISTFMT_TYPE_UNITS,
                                             ULISTFMT_WIDTH_WIDE)) {}

IcuBridge::ListFormatter::~ListFormatter() = default;

std::u16string IcuBridge::ListFormatter::FormatStandardList(
    base::span<const std::u16string> items) const {
  return FormatListInternal(items, standard_formatter_.get());
}

std::u16string IcuBridge::ListFormatter::FormatNarrowList(
    base::span<const std::u16string> items) const {
  return FormatListInternal(items, narrow_formatter_.get());
}

std::u16string IcuBridge::ListFormatter::FormatUnitList(
    base::span<const std::u16string> items) const {
  return FormatListInternal(items, unit_formatter_.get());
}

}  // namespace base::i18n
