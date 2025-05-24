// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_
#define CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_

#include <optional>
#include <string_view>
#include <vector>

#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "components/prefs/pref_service.h"
#include "content/public/browser/browser_context.h"
#include "third_party/blink/public/mojom/on_device_translation/translation_manager.mojom.h"

namespace on_device_translation {

// Returns the Accept Languages for a given Profile.
const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context);

// Determines if a given language is in the Accept languages.
bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang);

// Determines if the Translator API is enabled.
bool IsTranslatorAllowed(content::BrowserContext* browser_context);

// When the `TranslationAPIAcceptLanguagesCheck` feature is enabled, the
// Translation API will fail if neither the source nor destination language is
// in Accept Languages. This is intended to mitigate privacy concerns.
bool PassAcceptLanguagesCheck(
    const std::vector<std::string_view>& accept_languages,
    const std::string& source_lang,
    const std::string& target_lang);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_
