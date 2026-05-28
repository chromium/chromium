// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_CODE_BUILDER_H_
#define BASE_I18N_LANGUAGE_CODE_BUILDER_H_

#include <algorithm>
#include <optional>
#include <string_view>
#include <type_traits>

#include "base/containers/fixed_flat_set.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/internal/icu_bridge.rs.h"
#include "base/i18n/language_code.h"

namespace base {
namespace i18n::internal {
struct Icu4xLocale;
}

// Helper class for parsing and validating language codes.
//
// This class provides methods to create LanguageCode objects from strings
// with various options for validation and normalization. It uses Rust for the
// heavy lifting of BCP 47 parsing while providing a C++ interface.
//
// Example usage:
//   std::optional<LanguageCode> lang =
//       LanguageCodeBuilder::GetInstance().FromString("en-US");
//   if (lang) {
//     // Valid language code
//   }
//
// Examples of valid and invalid language codes:
// Valid: "en-US", "en-GB", "en-US-POSIX", "zh-Hans-CN", "und"
class BASE_I18N_EXPORT LanguageCodeBuilder {
 public:
  LanguageCodeBuilder();
  ~LanguageCodeBuilder();

  LanguageCodeBuilder(const LanguageCodeBuilder&) = delete;
  LanguageCodeBuilder& operator=(const LanguageCodeBuilder&) = delete;

  static const LanguageCodeBuilder& GetInstance();

  // Creates a LanguageCode from a string view.
  //
  // Returns: std::optional<LanguageCode> containing the parsed language code
  //            or std::nullopt if parsing fails.
  // We do run some normalization on the input language code:
  //  - Normalize case (e.g. "EN-US" -> "en-US").
  //  - Normalize separator (e.g. "en_US" -> "en-US").
  std::optional<LanguageCode> FromString(std::string_view code) const;
  // Internal usage.
  LanguageCode FromIcu4xLocale(
      const base::i18n::internal::Icu4xLocale& icu_locale) const;

 private:
  class Impl;
  std::unique_ptr<Impl> impl_;
};

}  // namespace base

#endif  // BASE_I18N_LANGUAGE_CODE_BUILDER_H_
