// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/on_device_translation/translation_manager_util.h"

#include "base/rand_util.h"
#include "base/strings/string_split.h"
#include "chrome/browser/on_device_translation/language_pack_util.h"
#include "chrome/browser/on_device_translation/pref_names.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/prefs/pref_service.h"
#include "components/services/on_device_translation/public/cpp/features.h"
#include "third_party/blink/public/common/features.h"
#include "third_party/blink/public/common/features_generated.h"
#include "ui/base/l10n/l10n_util.h"

namespace on_device_translation {

using blink::mojom::TranslationAvailability;
using blink::mojom::TranslatorLanguageCode;
using blink::mojom::TranslatorLanguageCodePtr;

// The number of language categories in the availability matrix.
constexpr size_t kLanguageCategoriesSize = 8u;

TranslationAvailability CalculateTranslationAvailability(
    const LanguageCategory& source,
    const LanguageCategory& target,
    bool accept_languages_check_enabled,
    size_t installable_package_count) {
  if (accept_languages_check_enabled) {
    // If both the source and the destination language are not in the user's
    // accept language, the translation is not available.
    if (!(source.preferred || target.preferred)) {
      return TranslationAvailability::kNo;
    }
    // If the languages which is not in the user's accept language is not a
    // popular language, the translation is not available.
    if ((!source.preferred && !source.popular) ||
        (!target.preferred && !target.popular)) {
      return TranslationAvailability::kNo;
    }
  }

  // If both the source and the destination language are installed, the
  // translation is available.
  if (source.installed && target.installed) {
    return TranslationAvailability::kReadily;
  }
  // If both the source and the destination language are not installed, that
  // means the user has to download the two language packs.
  if (!source.installed && !target.installed) {
    // If the user can download two language packs, the translation is available
    // after download, otherwise it is not available.
    return installable_package_count >= 2
               ? TranslationAvailability::kAfterDownload
               : TranslationAvailability::kNo;
  }

  // If one of the source or the destination language is installed, that means
  // the user only needs to download one language pack.
  // So if the user can download one language pack, the translation is available
  // after download, otherwise it is not available.
  return installable_package_count >= 1
             ? TranslationAvailability::kAfterDownload
             : TranslationAvailability::kNo;
}

std::vector<std::vector<TranslationAvailability>> CreateAvailabilityMatrix(
    bool accept_languages_check_enabled,
    size_t installable_package_count) {
  const std::vector<LanguageCategory> categories = CreateLanguageCategoryList();
  std::vector<std::vector<TranslationAvailability>> matrix;
  matrix.reserve(kLanguageCategoriesSize);
  for (const auto& source : categories) {
    std::vector<TranslationAvailability> availability_row;
    availability_row.reserve(kLanguageCategoriesSize);
    for (auto target : categories) {
      availability_row.emplace_back(CalculateTranslationAvailability(
          source, target, accept_languages_check_enabled,
          installable_package_count));
    }
    matrix.emplace_back(std::move(availability_row));
  }
  return matrix;
}

std::vector<std::vector<TranslatorLanguageCodePtr>> CreateLanguageCategories(
    const std::vector<std::string_view>& accept_languages,
    const std::set<LanguagePackKey>& installed_packs,
    bool is_en_preferred) {
  std::vector<std::vector<TranslatorLanguageCodePtr>> language_categories(
      kLanguageCategoriesSize);
  language_categories[GetLanguageCategoryIndex(/*installed=*/true,
                                               is_en_preferred,
                                               /*popular=*/true)]
      .emplace_back(TranslatorLanguageCode::New("en"));

  for (const auto& it : kLanguagePackComponentConfigMap) {
    const LanguagePackKey key = it.first;
    const SupportedLanguage supported_language =
        NonEnglishSupportedLanguageFromLanguagePackKey(key);
    const std::string_view language_code = ToLanguageCode(supported_language);
    const bool installed = installed_packs.contains(key);
    const bool preferred = IsInAcceptLanguage(accept_languages, language_code);
    const bool popular = IsPopularLanguage(supported_language);
    const size_t index =
        GetLanguageCategoryIndex(installed, preferred, popular);
    language_categories[index].push_back(
        TranslatorLanguageCode::New(std::string(language_code)));
  }
  return language_categories;
}

std::vector<LanguageCategory> CreateLanguageCategoryList() {
  std::vector<LanguageCategory> list;
  list.reserve(kLanguageCategoriesSize);
  for (bool installed : {true, false}) {
    for (bool preferred : {true, false}) {
      for (bool popular : {true, false}) {
        CHECK_EQ(GetLanguageCategoryIndex(installed, preferred, popular),
                 list.size());
        list.emplace_back(LanguageCategory{
            .installed = installed,
            .preferred = preferred,
            .popular = popular,
        });
      }
    }
  }
  return list;
}

const std::vector<std::string_view> GetAcceptLanguages(
    content::BrowserContext* browser_context) {
  CHECK(browser_context);

  PrefService* profile_pref =
      Profile::FromBrowserContext(browser_context)->GetPrefs();
  const std::vector<std::string_view> accept_languages = base::SplitStringPiece(
      profile_pref->GetString(language::prefs::kAcceptLanguages), ",",
      base::TRIM_WHITESPACE, base::SPLIT_WANT_NONEMPTY);

  return accept_languages;
}

size_t GetLanguageCategoryIndex(bool installed, bool preferred, bool popular) {
  return (installed ? 0 : 4) + (preferred ? 0 : 2) + (popular ? 0 : 1);
}

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang) {
  const std::string normalized_lang = l10n_util::GetLanguage(lang);
  return std::find_if(accept_languages.begin(), accept_languages.end(),
                      [&](const std::string_view lang) {
                        return l10n_util::GetLanguage(lang) == normalized_lang;
                      }) != accept_languages.end();
}

bool IsSupportedPopularLanguage(const std::string& lang) {
  const std::optional<SupportedLanguage> supported_lang =
      ToSupportedLanguage(lang);
  if (!supported_lang) {
    return false;
  }
  return IsPopularLanguage(*supported_lang);
}

bool IsTranslatorAllowed(content::BrowserContext* browser_context) {
  CHECK(browser_context);
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kTranslatorAPIAllowed);
}

bool MaskReadilyResult(const std::vector<std::string_view>& accept_languages,
                       const std::string& source_language,
                       const std::string& target_language) {
  bool mask_readily_result =
      (source_language != "en" &&
       !IsInAcceptLanguage(accept_languages, source_language)) ||
      (target_language != "en" &&
       !IsInAcceptLanguage(accept_languages, target_language));

  // TODO(crbug.com/385173766): Remove once V1 is launched.
  return base::FeatureList::IsEnabled(blink::features::kTranslationAPIV1) &&
         mask_readily_result;
}

bool PassAcceptLanguagesCheck(
    const std::vector<std::string_view>& accept_languages,
    const std::string& source_lang,
    const std::string& target_lang) {
  if (base::FeatureList::IsEnabled(blink::features::kTranslationAPIV1) ||
      !kTranslationAPIAcceptLanguagesCheck.Get()) {
    return true;
  }

  // TODO(crbug.com/371899260): Implement better language code handling.

  // One of the source or the destination language must be in the user's accept
  // language.
  const bool source_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, source_lang);
  const bool target_lang_is_in_accept_langs =
      IsInAcceptLanguage(accept_languages, target_lang);

  // The other language must be a popular language.
  if (!source_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(source_lang)) {
    return false;
  }
  if (!target_lang_is_in_accept_langs &&
      !IsSupportedPopularLanguage(target_lang)) {
    return false;
  }
  return true;
}

}  // namespace on_device_translation
