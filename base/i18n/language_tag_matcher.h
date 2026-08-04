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
#include "base/memory/raw_ptr_exclusion.h"

namespace icu4x {
class LocaleFallbacker;
}

namespace base::i18n {

// A class that matches a preferred language tag against a set of supported
// language tags using ICU fallback rules and precomputed distances.
//
// Example usage:
//   const LanguageTagConverter& builder = LanguageTagConverter::GetInstance();
//   LanguageTagMatcher matcher = LanguageTagMatcher::Create({
//      GetKnownLanguageTag<"en-US">(),
//      GetKnownLanguageTag<"fr-FR">(),
//      GetKnownLanguageTag<"es-AR">(),
//   });
//
//   // Exact match:
//   matcher.Match(GetKnownLanguageTag<"en-US">()); // Returns "en-US"
//
//   // Fallback match:
//   matcher.Match(GetKnownLanguageTag<"en-GB">()); // Returns "en-US"
//
//   // Macro-region match:
//   matcher.Match(GetKnownLanguageTag<"es-MX">()); // Returns "es-419"
//
//   // No match:
//   matcher.Match(GetKnownLanguageTag<"de">()); // Returns nullopt
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

  LanguageTagMatcher(LanguageTagMatcher&&) noexcept;
  LanguageTagMatcher& operator=(LanguageTagMatcher&&) noexcept;

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

  // Returns true whether there is an exact match or not.
  bool HasExactMatch(const LanguageTag& preferred_tag) const;

 private:
  explicit LanguageTagMatcher(
      base::flat_map<LanguageTag, LanguageTag> closest_supported_tag,
      std::unique_ptr<icu4x::LocaleFallbacker> icu_fallbacker);

  base::flat_map<LanguageTag, LanguageTag> closest_supported_tag_;
  std::unique_ptr<icu4x::LocaleFallbacker> icu_fallbacker_;
};

// This class provides the same methods as `LanguageTagMatcher` with an
// additional `MatchOrDefault` that always returns a `LanguageTag`. The default
// language tag needs to be given during construction which makes it useful for
// usages where a default is needed but the client does not necessarily know
// which language to use as default.
class BASE_I18N_EXPORT LanguageTagMatcherWithDefault {
 public:
  // Similar to `LanguageTagMatcher::Create` but also takes as the first
  // argument, a default locale.
  static LanguageTagMatcherWithDefault Create(
      LanguageTag default_tag,
      base::span<const LanguageTag> supported_tags);

  // Same as `LanguageTagMatcher::Match`.
  std::optional<LanguageTag> Match(const LanguageTag& preferred_tag) const;

  // Same as `LanguageTagMatcher::HasExactMatch`.
  bool HasExactMatch(const LanguageTag& preferred_tag) const;

  // Same as `LanguageTagMatcher::Match` but returns the default tag (received
  // during construction) if there is no match in the set of supported language
  // tags.
  LanguageTag MatchOrDefault(const LanguageTag& preferred_tag) const;

 private:
  LanguageTagMatcherWithDefault(LanguageTag default_tag,
                                LanguageTagMatcher matcher);
  LanguageTag default_tag_;
  LanguageTagMatcher matcher_;
};

}  // namespace base::i18n

#endif  // BASE_I18N_LANGUAGE_TAG_MATCHER_H_
