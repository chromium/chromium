// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef BASE_I18N_CHINESE_HELPERS_H_
#define BASE_I18N_CHINESE_HELPERS_H_

#include "base/component_export.h"
#include "base/i18n/language_tag.h"

namespace base::i18n {

// Returns whether `tag` represents a Chinese (zh) language.
COMPONENT_EXPORT(LANGUAGE_TAG_WITH_ICU) bool IsChinese(const LanguageTag& tag);
// Returns whether `tag` represents a Chinese Simplified (zh-Hans) language.
COMPONENT_EXPORT(LANGUAGE_TAG_WITH_ICU)
bool IsSimplifiedChinese(const LanguageTag& tag);
// Returns whether `tag` represents a Chinese Traditional (zh-Hant) language.
COMPONENT_EXPORT(LANGUAGE_TAG_WITH_ICU)
bool IsTraditionalChinese(const LanguageTag& tag);

}  // namespace base::i18n

#endif  // BASE_I18N_CHINESE_HELPERS_H_
