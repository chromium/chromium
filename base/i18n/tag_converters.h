// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TAG_CONVERTERS_H_
#define BASE_I18N_TAG_CONVERTERS_H_

#include <memory>
#include <optional>
#include <string_view>

#include "base/component_export.h"
#include "base/containers/fixed_flat_set.h"
#include "base/i18n/language_tag.h"

namespace icu4x {
class Locale;
}

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
class COMPONENT_EXPORT(LANGUAGE_TAG_WITH_ICU) LanguageTagConverter {
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
  LanguageTag FromIcu4xCapiLocale(const icu4x::Locale& locale) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Helper function to obtain a `LanguageTag` from a string. It is just a
// convenient function to avoid people having to call the `LanguageTagConverter`
// singleton as it is quite verbose to do it.
COMPONENT_EXPORT(LANGUAGE_TAG_WITH_ICU)
std::optional<LanguageTag> GetLanguageTagFromString(std::string_view tag);

}  // namespace base::i18n

#endif  // BASE_I18N_TAG_CONVERTERS_H_
