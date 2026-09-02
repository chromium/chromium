// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_ICU4C_TAG_CONVERTER_H_
#define BASE_I18N_ICU4C_TAG_CONVERTER_H_

#include <string>

#include "base/containers/flat_map.h"
#include "base/i18n/base_i18n_export.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base {
template <typename T>
class NoDestructor;
}  // namespace base

namespace base::i18n {

class LanguageTag;

// Helper class for converting type-safe BCP 47 `LanguageTag`s to legacy
// C++ ICU `icu::Locale` objects.
//
// Example usage:
//   const IcuLocaleConverter& converter = IcuLocaleConverter::GetInstance();
//   icu::Locale locale = converter.FromLanguageTag(language_tag);
class BASE_I18N_EXPORT IcuLocaleConverter {
 public:
  IcuLocaleConverter(const IcuLocaleConverter&) = delete;
  IcuLocaleConverter& operator=(const IcuLocaleConverter&) = delete;

  static const IcuLocaleConverter& GetInstance();

  // Converts a type-safe `LanguageTag` into a corresponding `icu::Locale`.
  //
  // Returns: An `icu::Locale` instance constructed from the BCP 47 string
  //            represented by `language_tag`.
  icu::Locale FromLanguageTag(const LanguageTag& language_tag) const;

  // Converts an `icu::Locale` into a type-safe `LanguageTag`.
  //
  // Returns: A `LanguageTag` representing the BCP 47 language tag of the
  // `icu_locale`.
  LanguageTag ToLanguageTag(const icu::Locale& icu_locale) const;

 private:
  IcuLocaleConverter();
  ~IcuLocaleConverter();

  friend class base::NoDestructor<IcuLocaleConverter>;

  base::flat_map<std::string, icu::Locale> cached_locales_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_ICU4C_TAG_CONVERTER_H_
