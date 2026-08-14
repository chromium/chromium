// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_LEGACY_LANGUAGE_TAG_HELPERS_H_
#define BASE_I18N_LEGACY_LANGUAGE_TAG_HELPERS_H_

#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"

namespace base::i18n {

inline std::string GetLanguageSubtagUsingLanguageTag(std::string_view locale) {
  return GetLanguageTagFromString(locale)
      .transform(&LanguageTag::language_subtag)
      .transform([](std::string_view subtag) { return std::string(subtag); })
      .value_or(std::string());
}

}  // namespace base::i18n

#endif  // BASE_I18N_LEGACY_LANGUAGE_TAG_HELPERS_H_
