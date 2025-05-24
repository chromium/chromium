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

namespace {

bool IsSupportedPopularLanguage(const std::string& lang) {
  const std::optional<SupportedLanguage> supported_lang =
      ToSupportedLanguage(lang);
  if (!supported_lang) {
    return false;
  }
  return IsPopularLanguage(*supported_lang);
}

}  // namespace

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

bool IsInAcceptLanguage(const std::vector<std::string_view>& accept_languages,
                        const std::string_view lang) {
  const std::string normalized_lang = l10n_util::GetLanguage(lang);

  return std::find_if(accept_languages.begin(), accept_languages.end(),
                      [&](const std::string_view lang) {
                        return l10n_util::GetLanguage(lang) == normalized_lang;
                      }) != accept_languages.end();
}

bool IsTranslatorAllowed(content::BrowserContext* browser_context) {
  CHECK(browser_context);
  return Profile::FromBrowserContext(browser_context)
      ->GetPrefs()
      ->GetBoolean(prefs::kTranslatorAPIAllowed);
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
