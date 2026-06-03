// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LANGUAGE_TAG_MATCHER_H_
#define BASE_I18N_LANGUAGE_TAG_MATCHER_H_

#include <optional>

#include "base/containers/flat_map.h"
#include "base/containers/span.h"
#include "base/i18n/base_i18n_export.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tags.h"
#include "third_party/rust/cxx/v1/cxx.h"

namespace base {
namespace i18n::internal {
struct IcuFallbacker;
}

// A class that matches a preferred language tag against a set of supported
// language tags using ICU fallback rules and precomputed distances.
//
// Example usage:
//   const LanguageTagConverter& builder = LanguageTagConverter::GetInstance();
//   LanguageTagMatcher matcher = LanguageTagMatcher::Create({
//      language_tags::ENGLISH_US(),
//      language_tags::FRENCH_FRANCE(),
//      language_tags::SPANISH_ARGENTINA(),
//   });
//
//   // Exact match:
//   matcher.Match(language_tags::ENGLISH_US()); // Returns "en-US"
//
//   // Fallback match:
//   matcher.Match(language_tags::BRITISH_ENGLISH()); // Returns "en-US"
//
//   // Macro-region match:
//   matcher.Match(language_tags::SPANISH_MEXICO()); // Returns "es-419"
//
//   // No match:
//   matcher.Match(language_tags::GERMAN()); // Returns nullopt
class BASE_I18N_EXPORT LanguageTagMatcher {
 public:
  // Creates a new matcher for the given set of supported locales.
  // Precomputes matching logic for the provided list of supported locales.
  // This operation can be expensive and should typically be performed once
  // (e.g., during application startup or when the set of supported languages
  // changes).
  static LanguageTagMatcher Create(
      base::span<const LanguageTag> supported_tags);

  ~LanguageTagMatcher();

  LanguageTagMatcher(const LanguageTagMatcher&) = delete;
  LanguageTagMatcher& operator=(const LanguageTagMatcher&) = delete;

  // Finds the best match between the supported tags and the preferred
  // tag. Returns the matched LanguageTag from the supported list, or
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
  std::optional<LanguageTag> Match(const LanguageTag& preferred_tag) const;

 private:
  explicit LanguageTagMatcher(
      base::flat_map<LanguageTag, LanguageTag> closest_supported_tag,
      rust::Box<i18n::internal::IcuFallbacker> icu_fallbacker);

  const base::flat_map<LanguageTag, LanguageTag> closest_supported_tag_;
  rust::Box<i18n::internal::IcuFallbacker> icu_fallbacker_;
};

}  // namespace base

#endif  // BASE_I18N_LANGUAGE_TAG_MATCHER_H_
