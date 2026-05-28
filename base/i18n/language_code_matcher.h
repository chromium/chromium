// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_CODE_MATCHER_H_
#define BASE_I18N_LANGUAGE_CODE_MATCHER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_code.h"
#include "base/i18n/language_codes.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base {
namespace i18n::internal {
struct IcuFallbacker;
}

// A class that matches a preferred language code against a set of supported
// language codes using ICU fallback rules and precomputed distances.
//
// Example usage:
//   const LanguageCodeBuilder& builder = LanguageCodeBuilder::GetInstance();
//   LanguageCodeMatcher matcher = LanguageCodeMatcher::Create({
//      language_codes::ENGLISH_US(),
//      language_codes::FRENCH_FRANCE(),
//      language_codes::SPANISH_ARGENTINA(),
//   });
//
//   // Exact match:
//   matcher.Match(language_codes::ENGLISH_US()); // Returns "en-US"
//
//   // Fallback match:
//   matcher.Match(language_codes::BRITISH_ENGLISH()); // Returns "en-US"
//
//   // Macro-region match:
//   matcher.Match(language_codes::SPANISH_MEXICO()); // Returns "es-419"
//
//   // No match:
//   matcher.Match(language_codes::GERMAN()); // Returns nullopt
class BASE_I18N_EXPORT LanguageCodeMatcher {
 public:
  // Creates a new matcher for the given set of supported locales.
  // Precomputes matching logic for the provided list of supported locales.
  // This operation can be expensive and should typically be performed once
  // (e.g., during application startup or when the set of supported languages
  // changes).
  static LanguageCodeMatcher Create(
      base::span<const LanguageCode> supported_locales);

  ~LanguageCodeMatcher();

  LanguageCodeMatcher(const LanguageCodeMatcher&) = delete;
  LanguageCodeMatcher& operator=(const LanguageCodeMatcher&) = delete;

  // Finds the best match between the supported locales and the preferred
  // locale. Returns the matched LanguageCode from the supported list, or
  // std::nullopt if no match is found.
  //
  // The matching algorithm uses ICU fallback rules, likely subtags, and
  // script/regional affinity.
  //
  // Examples:
  // - Exact match:
  //   Supported: {"en-US", "fr-FR"}, Preferred: "en-US" -> Matches "en-US"
  // - Fallback:
  //   Supported: {"en", "fr"}, Preferred: "en-US" -> Matches "en"
  // - Maximization:
  //   Supported: {"en-US"}, Preferred: "en" -> Matches "en-US"
  // - Script Affinity:
  //   Supported: {"zh-TW"}, Preferred: "zh-HK" -> Matches "zh-TW" (Traditional)
  // - Macro-region:
  //   Supported: {"es-419"}, Preferred: "es-AR" -> Matches "es-419" (Latin Am.)
  std::optional<LanguageCode> Match(const LanguageCode& preferred_locale) const;

 private:
  explicit LanguageCodeMatcher(
      base::flat_map<LanguageCode, LanguageCode> closest_supported_locale,
      rust::Box<i18n::internal::IcuFallbacker> icu_fallbacker);

  const base::flat_map<LanguageCode, LanguageCode> closest_supported_locale_;
  rust::Box<i18n::internal::IcuFallbacker> icu_fallbacker_;
};

}  // namespace base

#endif  // BASE_I18N_LANGUAGE_CODE_MATCHER_H_
