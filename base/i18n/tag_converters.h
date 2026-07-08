// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_TAG_CONVERTERS_H_
#define BASE_I18N_TAG_CONVERTERS_H_

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/internal/bcp47_parser.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/internal/immutable_string.h"
#include "base/i18n/language_tag.h"

namespace base {
class Value;
}

namespace base::i18n {

namespace internal {
struct Icu4xLocale;
}

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
// Examples of valid and invalid language tags:
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
  // We do run some normalization on the input language tag:
  //  - Normalize case (e.g. "EN-US" -> "en-US").
  //  - Normalize separator (e.g. "en_US" -> "en-US").
  std::optional<LanguageTag> FromString(std::string_view tag) const;
  // Internal usage.
  LanguageTag FromIcu4xLocale(const internal::Icu4xLocale& icu_locale) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

// Converts a LanguageTag to a string base::Value.
BASE_I18N_EXPORT base::Value LanguageTagToValue(const LanguageTag& tag);

// Parses a LanguageTag from a base::Value.
// Returns std::nullopt if `value` is nullptr, not a string Value, or not a
// valid BCP 47 language tag.
BASE_I18N_EXPORT std::optional<LanguageTag> ValueToLanguageTag(
    const base::Value* value);
BASE_I18N_EXPORT std::optional<LanguageTag> ValueToLanguageTag(
    const base::Value& value);

}  // namespace base::i18n

#endif  // BASE_I18N_TAG_CONVERTERS_H_
