// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <string_view>

#include "base/containers/contains.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/pref_names.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash {
namespace {

const char kDefaultProfileLocale[] = "en-US";

// Determines the user's language or locale from the system, first trying
// the current IME language and falling back to the application locale.
std::string GetUserLangOrLocaleFromSystem(Profile* profile) {
  // Convert from the ID used in the pref to a language identifier.
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(
      profile->GetPrefs()->GetString(::prefs::kLanguageCurrentInputMethod));
  std::vector<std::string> languages;
  input_method::InputMethodManager::Get()
      ->GetInputMethodUtil()
      ->GetLanguageCodesFromInputMethodIds(input_method_ids, &languages);

  std::string user_language;
  if (!languages.empty())
    user_language = languages[0];

  // If we don't find an IME language, fall back to using the application
  // locale.
  if (user_language.empty())
    user_language = g_browser_process->GetApplicationLocale();

  return user_language.empty() ? kDefaultProfileLocale : user_language;
}

std::string GetSupportedLocale(const std::string& lang_or_locale) {
  if (lang_or_locale.empty())
    return std::string();

  // Map of language code to supported locale for the open web API.
  // Chrome OS does not support Chinese languages with "cmn", so this
  // map also includes a map from Open Speech API "cmn" languages to
  // their equivalent default locale.
  static constexpr auto kLangsToDefaultLocales =
      base::MakeFixedFlatMap<std::string_view, std::string_view>(
          {{"af", "af-ZA"},          {"am", "am-ET"},
           {"ar", "ar-001"},         {"az", "az-AZ"},
           {"bg", "bg-BG"},          {"bn", "bn-IN"},
           {"bs", "bs-BA"},          {"ca", "ca-ES"},
           {"cs", "cs-CZ"},          {"da", "da-DK"},
           {"de", "de-DE"},          {"el", "el-GR"},
           {"en", "en-US"},          {"es", "es-ES"},
           {"et", "et-EE"},          {"eu", "eu-ES"},
           {"fa", "fa-IR"},          {"fi", "fi-FI"},
           {"fil", "fil-PH"},        {"fr", "fr-FR"},
           {"gl", "gl-ES"},          {"gu", "gu-IN"},
           {"he", "iw-IL"},          {"hi", "hi-IN"},
           {"hr", "hr-HR"},          {"hu", "hu-HU"},
           {"hy", "hy-AM"},          {"id", "id-ID"},
           {"is", "is-IS"},          {"it", "it-IT"},
           {"iw", "iw-IL"},          {"ja", "ja-JP"},
           {"jv", "jv-ID"},          {"ka", "ka-GE"},
           {"kk", "kk-KZ"},          {"km", "km-KH"},
           {"kn", "kn-IN"},          {"ko", "ko-KR"},
           {"lo", "lo-LA"},          {"lt", "lt-LT"},
           {"lv", "lv-LV"},          {"mk", "mk-MK"},
           {"ml", "ml-IN"},          {"mn", "mn-MN"},
           {"mo", "ro-RO"},          {"mr", "mr-IN"},
           {"ms", "ms-MY"},          {"my", "my-MM"},
           {"ne", "ne-NP"},          {"nl", "nl-NL"},
           {"no", "no-NO"},          {"pa", "pa-Guru-IN"},
           {"pl", "pl-PL"},          {"pt", "pt-BR"},
           {"ro", "ro-RO"},          {"ru", "ru-RU"},
           {"si", "si-LK"},          {"sk", "sk-SK"},
           {"sl", "sl-SI"},          {"sq", "sq-AL"},
           {"sr", "sr-RS"},          {"su", "su-ID"},
           {"sv", "sv-SE"},          {"sw", "sw-TZ"},
           {"ta", "ta-IN"},          {"te", "te-IN"},
           {"tl", "fil-PH"},         {"th", "th-TH"},
           {"tr", "tr-TR"},          {"uk", "uk-UA"},
           {"ur", "ur-PK"},          {"uz", "uz-UZ"},
           {"vi", "vi-VN"},          {"yue", "yue-Hant-HK"},
           {"zh", "zh-CN"},          {"zu", "zu-ZA"},
           {"zh-cmn-CN", "zh-CN"},   {"zh-cmn", "zh-CN"},
           {"zh-cmn-Hans", "zh-CN"}, {"zh-cmn-Hans-CN", "zh-CN"},
           {"cmn-CN", "zh-CN"},      {"cmn-Hans", "zh-CN"},
           {"cmn-Hans-CN", "zh-CN"}, {"cmn-Hant-TW", "zh-TW"},
           {"zh-cmn-TW", "zh-TW"},   {"zh-cmn-Hant-TW", "zh-TW"},
           {"cmn-TW", "zh-TW"}});

  // First check if this is a language code supported in the map above.
  auto iter = kLangsToDefaultLocales.find(lang_or_locale);
  if (iter != kLangsToDefaultLocales.end())
    return std::string(iter->second);

  // If it's only a language code, we can return early, because no other
  // language-only codes are supported.
  std::pair<std::string_view, std::string_view> lang_and_locale_pair =
      language::SplitIntoMainAndTail(lang_or_locale);
  if (lang_and_locale_pair.second.size() == 0)
    return std::string();

  // The code is a supported locale. Return itself.
  // Note that it doesn't matter if the supported locale is online or offline.
  if (base::Contains(Dictation::GetAllSupportedLocales(), lang_or_locale))
    return lang_or_locale;

  // Finally, get the language code from the locale and try to use it to map
  // to a default locale. For example, "en-XX" should map to "en-US" if "en-XX"
  // does not exist.
  iter = kLangsToDefaultLocales.find(lang_and_locale_pair.first);
  if (iter != kLangsToDefaultLocales.end())
    return std::string(iter->second);
  return std::string();
}

}  // namespace

// static
const base::flat_map<std::string, Dictation::LocaleData>
Dictation::GetAllSupportedLocales() {
  base::flat_map<std::string, LocaleData> supported_locales;
  // If new RTL locales are added, ensure that
  // accessibility_common/dictation/commands.js RTLLocales is updated
  // appropriately.
  static const char* kWebSpeechSupportedLocales[] = {
      "af-ZA",       "am-ET",      "ar-AE", "ar-BH", "ar-DZ", "ar-EG", "ar-IL",
      "ar-IQ",       "ar-JO",      "ar-KW", "ar-LB", "ar-MA", "ar-OM", "ar-PS",
      "ar-QA",       "ar-SA",      "ar-TN", "ar-YE", "az-AZ", "bg-BG", "bn-BD",
      "bn-IN",       "bs-BA",      "ca-ES", "cs-CZ", "da-DK", "de-AT", "de-CH",
      "de-DE",       "el-GR",      "en-AU", "en-CA", "en-GB", "en-GH", "en-HK",
      "en-IE",       "en-IN",      "en-KE", "en-NG", "en-NZ", "en-PH", "en-PK",
      "en-SG",       "en-TZ",      "en-US", "en-ZA", "es-AR", "es-BO", "es-CL",
      "es-CO",       "es-CR",      "es-DO", "es-EC", "es-ES", "es-GT", "es-HN",
      "es-MX",       "es-NI",      "es-PA", "es-PE", "es-PR", "es-PY", "es-SV",
      "es-US",       "es-UY",      "es-VE", "et-EE", "eu-ES", "fa-IR", "fi-FI",
      "fil-PH",      "fr-BE",      "fr-CA", "fr-CH", "fr-FR", "gl-ES", "gu-IN",
      "hi-IN",       "hr-HR",      "hu-HU", "hy-AM", "id-ID", "is-IS", "it-CH",
      "it-IT",       "iw-IL",      "ja-JP", "jv-ID", "ka-GE", "kk-KZ", "km-KH",
      "kn-IN",       "ko-KR",      "lo-LA", "lt-LT", "lv-LV", "mk-MK", "ml-IN",
      "mn-MN",       "mr-IN",      "ms-MY", "my-MM", "ne-NP", "nl-BE", "nl-NL",
      "no-NO",       "pa-Guru-IN", "pl-PL", "pt-BR", "pt-PT", "ro-RO", "ru-RU",
      "si-LK",       "sk-SK",      "sl-SI", "sq-AL", "sr-RS", "su-ID", "sv-SE",
      "sw-KE",       "sw-TZ",      "ta-IN", "ta-LK", "ta-MY", "ta-SG", "te-IN",
      "th-TH",       "tr-TR",      "uk-UA", "ur-IN", "ur-PK", "uz-UZ", "vi-VN",
      "yue-Hant-HK", "zh-CN",      "zh-TW", "zu-ZA", "ar-001"};

  for (const char* locale : kWebSpeechSupportedLocales) {
    // By default these languages are not supported offline.
    supported_locales[locale] = LocaleData();
  }
  if (features::IsDictationOfflineAvailable()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    std::vector<std::string> offline_locales =
        soda_installer->GetAvailableLanguages();
    for (auto locale : offline_locales) {
      // These are supported offline.
      supported_locales[locale] = LocaleData();
      supported_locales[locale].works_offline = true;
      supported_locales[locale].installed =
          soda_installer->IsSodaInstalled(speech::GetLanguageCode(locale));
    }
  }
  return supported_locales;
}

// static
std::string Dictation::DetermineDefaultSupportedLocale(Profile* profile,
                                                       bool new_user) {
  std::string lang_or_locale;
  if (new_user) {
    // This is the first time this user has enabled Dictation. Pick the default
    // language preference based on their application locale.
    lang_or_locale = g_browser_process->GetApplicationLocale();
  } else {
    // This user has already had Dictation enabled, but now we need to map
    // from the language they've previously used to a supported locale.
    lang_or_locale = GetUserLangOrLocaleFromSystem(profile);
  }
  std::string supported_locale = GetSupportedLocale(lang_or_locale);
  return supported_locale.empty() ? kDefaultProfileLocale : supported_locale;
}

}  // namespace ash
