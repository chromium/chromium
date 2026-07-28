// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TAG_CONVERTERS_H_
#define BASE_I18N_TAG_CONVERTERS_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/containers/fixed_flat_set.h"
#include "base/containers/flat_map.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base {
template <typename T>
class NoDestructor;
}

namespace base::i18n_internal {
struct Icu4xLocale;
}  // namespace base::i18n_internal

namespace base::i18n {

// Helper class for parsing and validating language tags.
//
// This class provides methods to create LanguageTag objects from strings
// with various options for validation and normalization. It uses Rust for the
// heavy lifting of BCP 47 parsing while providing a C++ interface.
//
// Example usage:
//   std::optional<LanguageTag> lang =
//       LanguageTagConverter::GetInstance().FromString("en-US");
//   if (lang) {
//     // Valid language tag
//   }
//
// Examples of valid language tags:
// Valid: "en-US", "en-GB", "en-US-POSIX", "zh-Hans-CN", "und"
class BASE_I18N_EXPORT LanguageTagConverter {
 public:
  LanguageTagConverter();
  ~LanguageTagConverter();

  LanguageTagConverter(const LanguageTagConverter&) = delete;
  LanguageTagConverter& operator=(const LanguageTagConverter&) = delete;

  static const LanguageTagConverter& GetInstance();

  // Creates a LanguageTag from a string view.
  //
  // Returns: std::optional<LanguageTag> containing the parsed language tag
  //            or std::nullopt if parsing fails.
  // Performs normalization on the input language tag:
  //  - Normalize case (e.g. "EN-US" -> "en-US").
  //  - Normalize separator (e.g. "en_US" -> "en-US").
  std::optional<LanguageTag> FromString(std::string_view tag) const;
  // Internal usage.
  LanguageTag FromIcu4xLocale(
      const i18n_internal::Icu4xLocale& icu_locale) const;
  LanguageTag FromIcuLocale(const icu::Locale& icu_locale) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Helper function to obtain a `LanguageTag` from a string. It is just a
// convenient function to avoid people having to call the `LanguageTagConverter`
// singleton as it is quite verbose to do it.
BASE_I18N_EXPORT std::optional<LanguageTag> GetLanguageTagFromString(
    std::string_view tag);

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

 private:
  IcuLocaleConverter();
  ~IcuLocaleConverter();

  friend class base::NoDestructor<IcuLocaleConverter>;

  base::flat_map<std::string, icu::Locale> cached_locales_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_TAG_CONVERTERS_H_
