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

using blink::mojom::TranslationAvailability;
using blink::mojom::TranslatorLanguageCodePtr;

// `LanguageCategory` is used to represent the language category in the
// availability matrix.
struct LanguageCategory {
  bool installed;
  bool preferred;
  bool popular;
};

// Calculates the translation availability for the given source and target
// language categories.
TranslationAvailability CalculateTranslationAvailability(
    const LanguageCategory& source,
    const LanguageCategory& target,
    bool accept_languages_check_enabled,
    size_t installable_package_count);

// Creates the availability matrix for each language category.
std::vector<std::vector<TranslationAvailability>> CreateAvailabilityMatrix(
    bool accept_languages_check_enabled,
    size_t installable_package_count);

// Creates the language categories for the availability matrix.
// The language categories are stored in the following order:
//   0. Installed and preferred popular languages
//   1. Installed and preferred non-popular languages
//   2. Installed and non-preferred popular languages
//   3. Installed and non-preferred non-popular languages
//   4. Not installed and preferred popular languages
//   5. Not installed and preferred non-popular languages
//   6. Not installed and non-preferred popular languages
//   7. Not installed and non-preferred non-popular languages
// Note: `preferred` means that the language an Accept Language.
std::vector<std::vector<TranslatorLanguageCodePtr>> CreateLanguageCategories(
    const std::vector<std::string_view>& accept_languages,
    const std::set<LanguagePackKey>& installed_packs,
    bool is_en_preferred);

// Creates the language category list for the availability matrix.
std::vector<LanguageCategory> CreateLanguageCategoryList();

// Returns the Accept Languages for a given Profile.
const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context);

// Returns the index of the language category in the availability matrix.
size_t GetLanguageCategoryIndex(bool installed, bool preferred, bool popular);

// Determines if a given language is an Accept Language.
bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang);

// Determines if a language is both a supported and a popular language.
bool IsSupportedPopularLanguage(const std::string& lang);

// Determines if the Translator API is enabled.
bool IsTranslatorAllowed(content::BrowserContext* browser_context);

// Determines whether to mask a "readily" `availability()` result as
// "downloadable", as a fingerprinting prevention measure.
bool MaskReadilyResult(const std::vector<std::string_view>& accept_languages,
                       const std::string& source_language,
                       const std::string& target_language);

// When the `TranslationAPIAcceptLanguagesCheck` feature is enabled, the
// Translation API will fail if neither the source nor destination language is
// in Accept Languages. This is intended to mitigate privacy concerns.
bool PassAcceptLanguagesCheck(
    const std::vector<std::string_view>& accept_languages,
    const std::string& source_lang,
    const std::string& target_lang);

}  // namespace on_device_translation

#endif  // CHROME_BROWSER_ON_DEVICE_TRANSLATION_TRANSLATION_MANAGER_UTIL_H_
