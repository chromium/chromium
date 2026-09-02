// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/i18n/icu4c_tag_converter.h"

#include <iterator>
#include <utility>
#include <vector>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/no_destructor.h"
#include "third_party/icu/source/common/unicode/locid.h"

namespace base::i18n {

IcuLocaleConverter::IcuLocaleConverter() {
  static constexpr const char* kCanonicalLanguageTags[] = {
#define IMPL_LANGUAGECODE_TAG_NAME(tag, name) tag,
#include "base/i18n/internal/canonical_language_tags.inc"
#undef IMPL_LANGUAGECODE_TAG_NAME
  };

  std::vector<std::pair<std::string, icu::Locale>> locales;
  locales.reserve(std::size(kCanonicalLanguageTags));
  for (const char* tag : kCanonicalLanguageTags) {
    UErrorCode status = U_ZERO_ERROR;
    locales.emplace_back(tag, icu::Locale::forLanguageTag(tag, status));
  }

  cached_locales_ =
      base::flat_map<std::string, icu::Locale>(std::move(locales));
}

IcuLocaleConverter::~IcuLocaleConverter() = default;

// static
const IcuLocaleConverter& IcuLocaleConverter::GetInstance() {
  static base::NoDestructor<IcuLocaleConverter> instance;
  return *instance;
}

icu::Locale IcuLocaleConverter::FromLanguageTag(
    const LanguageTag& language_tag) const {
  auto it = cached_locales_.find(language_tag.tag_string());
  if (it != cached_locales_.end()) {
    return it->second;
  }
  UErrorCode status = U_ZERO_ERROR;
  return icu::Locale::forLanguageTag(language_tag.tag_string(), status);
}

LanguageTag IcuLocaleConverter::ToLanguageTag(
    const icu::Locale& icu_locale) const {
  UErrorCode status = U_ZERO_ERROR;
  std::string tag = icu_locale.toLanguageTag<std::string>(status);
  if (U_FAILURE(status)) {
    return GetKnownLanguageTag("und");
  }

  // The `tag` returned by ICU4C will certainly produce a valid LanguageTag,
  // that is why we always return a valid LanguageTag.
  // Note: we call FromString on the `tag` to make sure we apply the same
  // canonicalizations.
  return LanguageTagConverter::GetInstance().FromString(tag).value_or(
      GetKnownLanguageTag("und"));
}

}  // namespace base::i18n
