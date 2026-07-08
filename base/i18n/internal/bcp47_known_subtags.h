// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_INTERNAL_BCP47_KNOWN_SUBTAGS_H_
#define BASE_I18N_INTERNAL_BCP47_KNOWN_SUBTAGS_H_

#include <string_view>

#include "base/containers/fixed_flat_set.h"

namespace base::i18n_internal {

// Returns true if `subtag` is a known language subtag in Chromium.
// No normalization is applied to `subtag`. The known subtags are supposed to be
// in their 'normalized' form as per:
// https://www.rfc-editor.org/info/rfc5646/#section-3.1.4
constexpr bool IsKnownLanguageSubtag(std::string_view subtag) {
  static constexpr auto kKnownLanguages =
      base::MakeFixedFlatSet<std::string_view>({
#define IMPL_BCP47_LANGUAGE(tag) tag,
#include "base/i18n/internal/bcp47_languages.inc"
#undef IMPL_BCP47_LANGUAGE
      });
  return kKnownLanguages.contains(subtag);
}

// Returns true if `subtag` is a known script subtag in Chromium.
// No normalization is applied to `subtag`. The known subtags are supposed to be
// in their 'normalized' form as per:
// https://www.rfc-editor.org/info/rfc5646/#section-3.1.4
constexpr bool IsKnownScriptSubtag(std::string_view subtag) {
  static constexpr auto kKnownScripts =
      base::MakeFixedFlatSet<std::string_view>({
#define IMPL_BCP47_SCRIPT(tag) tag,
#include "base/i18n/internal/bcp47_scripts.inc"
#undef IMPL_BCP47_SCRIPT
      });
  return kKnownScripts.contains(subtag);
}

// Returns true if `subtag` is a known region subtag in Chromium.
// No normalization is applied to `subtag`. The known subtags are supposed to be
// in their 'normalized' form as per:
// https://www.rfc-editor.org/info/rfc5646/#section-3.1.4
constexpr bool IsKnownRegionSubtag(std::string_view subtag) {
  static constexpr auto kKnownRegions =
      base::MakeFixedFlatSet<std::string_view>({
#define IMPL_BCP47_REGION(tag) tag,
#include "base/i18n/internal/bcp47_regions.inc"
#undef IMPL_BCP47_REGION
      });
  return kKnownRegions.contains(subtag);
}

// Returns true if `subtag` is a known variant subtag in Chromium.
// No normalization is applied to `subtag`. The known subtags are supposed to be
// in their 'normalized' form as per:
// https://www.rfc-editor.org/info/rfc5646/#section-3.1.4
constexpr bool IsKnownVariantSubtag(std::string_view subtag) {
  static constexpr auto kKnownVariants =
      base::MakeFixedFlatSet<std::string_view>({
#define IMPL_BCP47_VARIANT(tag) tag,
#include "base/i18n/internal/bcp47_variants.inc"
#undef IMPL_BCP47_VARIANT
      });
  return kKnownVariants.contains(subtag);
}

}  // namespace base::i18n_internal

#endif  // BASE_I18N_INTERNAL_BCP47_KNOWN_SUBTAGS_H_
