// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/accessibility/dictation.h"

#include <optional>
#include <string>
#include <string_view>
#include <vector>

#include "ash/constants/ash_pref_names.h"
#include "base/containers/fixed_flat_map.h"
#include "base/containers/flat_map.h"
#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "components/language/core/browser/pref_names.h"
#include "components/language/core/common/locale_util.h"
#include "components/prefs/pref_service.h"
#include "components/soda/soda_installer.h"
#include "ui/accessibility/accessibility_features.h"
#include "ui/base/ime/ash/input_method_manager.h"
#include "ui/base/ime/ash/input_method_util.h"

namespace ash {
namespace {

constexpr base::i18n::LanguageTag kDefaultProfileLocale =
    base::i18n::GetKnownLanguageTag("en-US");

// Determines the user's language or locale from the system, first trying
// the current IME language and falling back to the application locale.
base::i18n::LanguageTag GetUserLangOrLocaleFromSystem(
    Profile* profile,
    const base::i18n::LanguageTag& application_locale) {
  // Convert from the ID used in the pref to a language identifier.
  std::vector<std::string> input_method_ids;
  input_method_ids.push_back(
      profile->GetPrefs()->GetString(ash::prefs::kLanguageCurrentInputMethod));
  const std::vector<base::i18n::LanguageTag> languages =
      input_method::InputMethodManager::Get()
          ->GetInputMethodUtil()
          ->GetLanguageCodesFromInputMethodIds(input_method_ids);

  // If we don't find an IME language, fall back to using the application
  // locale.
  if (languages.empty()) {
    return application_locale;
  }
  return languages[0];
}

base::i18n::LanguageTag GetSupportedLocale(
    const base::i18n::LanguageTag& lang_or_locale) {
  // Map of language code to supported locale for the open web API.
  // Chrome OS does not support Chinese languages with "cmn", so this
  // map also includes a map from Open Speech API "cmn" languages to
  // their equivalent default locale.
  static constexpr auto kLangsToDefaultLocales =
      base::MakeFixedFlatMap<base::i18n::LanguageTag, base::i18n::LanguageTag>({
          {base::i18n::GetKnownLanguageTag("af"),
           base::i18n::GetKnownLanguageTag("af-ZA")},
          {base::i18n::GetKnownLanguageTag("am"),
           base::i18n::GetKnownLanguageTag("am-ET")},
          {base::i18n::GetKnownLanguageTag("ar"),
           base::i18n::GetKnownLanguageTag("ar-001")},
          {base::i18n::GetKnownLanguageTag("az"),
           base::i18n::GetKnownLanguageTag("az-AZ")},
          {base::i18n::GetKnownLanguageTag("bg"),
           base::i18n::GetKnownLanguageTag("bg-BG")},
          {base::i18n::GetKnownLanguageTag("bn"),
           base::i18n::GetKnownLanguageTag("bn-IN")},
          {base::i18n::GetKnownLanguageTag("bs"),
           base::i18n::GetKnownLanguageTag("bs-BA")},
          {base::i18n::GetKnownLanguageTag("ca"),
           base::i18n::GetKnownLanguageTag("ca-ES")},
          {base::i18n::GetKnownLanguageTag("cs"),
           base::i18n::GetKnownLanguageTag("cs-CZ")},
          {base::i18n::GetKnownLanguageTag("da"),
           base::i18n::GetKnownLanguageTag("da-DK")},
          {base::i18n::GetKnownLanguageTag("de"),
           base::i18n::GetKnownLanguageTag("de-DE")},
          {base::i18n::GetKnownLanguageTag("el"),
           base::i18n::GetKnownLanguageTag("el-GR")},
          {base::i18n::GetKnownLanguageTag("en"),
           base::i18n::GetKnownLanguageTag("en-US")},
          {base::i18n::GetKnownLanguageTag("es"),
           base::i18n::GetKnownLanguageTag("es-ES")},
          {base::i18n::GetKnownLanguageTag("et"),
           base::i18n::GetKnownLanguageTag("et-EE")},
          {base::i18n::GetKnownLanguageTag("eu"),
           base::i18n::GetKnownLanguageTag("eu-ES")},
          {base::i18n::GetKnownLanguageTag("fa"),
           base::i18n::GetKnownLanguageTag("fa-IR")},
          {base::i18n::GetKnownLanguageTag("fi"),
           base::i18n::GetKnownLanguageTag("fi-FI")},
          {base::i18n::GetKnownLanguageTag("fil"),
           base::i18n::GetKnownLanguageTag("fil-PH")},
          {base::i18n::GetKnownLanguageTag("fr"),
           base::i18n::GetKnownLanguageTag("fr-FR")},
          {base::i18n::GetKnownLanguageTag("gl"),
           base::i18n::GetKnownLanguageTag("gl-ES")},
          {base::i18n::GetKnownLanguageTag("gu"),
           base::i18n::GetKnownLanguageTag("gu-IN")},
          {base::i18n::GetKnownLanguageTag("he"),
           base::i18n::GetKnownLanguageTag("he-IL")},
          {base::i18n::GetKnownLanguageTag("hi"),
           base::i18n::GetKnownLanguageTag("hi-IN")},
          {base::i18n::GetKnownLanguageTag("hr"),
           base::i18n::GetKnownLanguageTag("hr-HR")},
          {base::i18n::GetKnownLanguageTag("hu"),
           base::i18n::GetKnownLanguageTag("hu-HU")},
          {base::i18n::GetKnownLanguageTag("hy"),
           base::i18n::GetKnownLanguageTag("hy-AM")},
          {base::i18n::GetKnownLanguageTag("id"),
           base::i18n::GetKnownLanguageTag("id-ID")},
          {base::i18n::GetKnownLanguageTag("is"),
           base::i18n::GetKnownLanguageTag("is-IS")},
          {base::i18n::GetKnownLanguageTag("it"),
           base::i18n::GetKnownLanguageTag("it-IT")},
          {base::i18n::GetKnownLanguageTag("ja"),
           base::i18n::GetKnownLanguageTag("ja-JP")},
          {base::i18n::GetKnownLanguageTag("jv"),
           base::i18n::GetKnownLanguageTag("jv-ID")},
          {base::i18n::GetKnownLanguageTag("ka"),
           base::i18n::GetKnownLanguageTag("ka-GE")},
          {base::i18n::GetKnownLanguageTag("kk"),
           base::i18n::GetKnownLanguageTag("kk-KZ")},
          {base::i18n::GetKnownLanguageTag("km"),
           base::i18n::GetKnownLanguageTag("km-KH")},
          {base::i18n::GetKnownLanguageTag("kn"),
           base::i18n::GetKnownLanguageTag("kn-IN")},
          {base::i18n::GetKnownLanguageTag("ko"),
           base::i18n::GetKnownLanguageTag("ko-KR")},
          {base::i18n::GetKnownLanguageTag("lo"),
           base::i18n::GetKnownLanguageTag("lo-LA")},
          {base::i18n::GetKnownLanguageTag("lt"),
           base::i18n::GetKnownLanguageTag("lt-LT")},
          {base::i18n::GetKnownLanguageTag("lv"),
           base::i18n::GetKnownLanguageTag("lv-LV")},
          {base::i18n::GetKnownLanguageTag("mk"),
           base::i18n::GetKnownLanguageTag("mk-MK")},
          {base::i18n::GetKnownLanguageTag("ml"),
           base::i18n::GetKnownLanguageTag("ml-IN")},
          {base::i18n::GetKnownLanguageTag("mn"),
           base::i18n::GetKnownLanguageTag("mn-MN")},
          {base::i18n::GetKnownLanguageTag("mr"),
           base::i18n::GetKnownLanguageTag("mr-IN")},
          {base::i18n::GetKnownLanguageTag("ms"),
           base::i18n::GetKnownLanguageTag("ms-MY")},
          {base::i18n::GetKnownLanguageTag("my"),
           base::i18n::GetKnownLanguageTag("my-MM")},
          {base::i18n::GetKnownLanguageTag("ne"),
           base::i18n::GetKnownLanguageTag("ne-NP")},
          {base::i18n::GetKnownLanguageTag("nl"),
           base::i18n::GetKnownLanguageTag("nl-NL")},
          {base::i18n::GetKnownLanguageTag("no"),
           base::i18n::GetKnownLanguageTag("no-NO")},
          {base::i18n::GetKnownLanguageTag("pa"),
           base::i18n::GetKnownLanguageTag("pa-Guru-IN")},
          {base::i18n::GetKnownLanguageTag("pl"),
           base::i18n::GetKnownLanguageTag("pl-PL")},
          {base::i18n::GetKnownLanguageTag("pt"),
           base::i18n::GetKnownLanguageTag("pt-BR")},
          {base::i18n::GetKnownLanguageTag("ro"),
           base::i18n::GetKnownLanguageTag("ro-RO")},
          {base::i18n::GetKnownLanguageTag("ru"),
           base::i18n::GetKnownLanguageTag("ru-RU")},
          {base::i18n::GetKnownLanguageTag("si"),
           base::i18n::GetKnownLanguageTag("si-LK")},
          {base::i18n::GetKnownLanguageTag("sk"),
           base::i18n::GetKnownLanguageTag("sk-SK")},
          {base::i18n::GetKnownLanguageTag("sl"),
           base::i18n::GetKnownLanguageTag("sl-SI")},
          {base::i18n::GetKnownLanguageTag("sq"),
           base::i18n::GetKnownLanguageTag("sq-AL")},
          {base::i18n::GetKnownLanguageTag("sr"),
           base::i18n::GetKnownLanguageTag("sr-RS")},
          {base::i18n::GetKnownLanguageTag("su"),
           base::i18n::GetKnownLanguageTag("su-ID")},
          {base::i18n::GetKnownLanguageTag("sv"),
           base::i18n::GetKnownLanguageTag("sv-SE")},
          {base::i18n::GetKnownLanguageTag("sw"),
           base::i18n::GetKnownLanguageTag("sw-TZ")},
          {base::i18n::GetKnownLanguageTag("ta"),
           base::i18n::GetKnownLanguageTag("ta-IN")},
          {base::i18n::GetKnownLanguageTag("te"),
           base::i18n::GetKnownLanguageTag("te-IN")},
          {base::i18n::GetKnownLanguageTag("th"),
           base::i18n::GetKnownLanguageTag("th-TH")},
          {base::i18n::GetKnownLanguageTag("tr"),
           base::i18n::GetKnownLanguageTag("tr-TR")},
          {base::i18n::GetKnownLanguageTag("uk"),
           base::i18n::GetKnownLanguageTag("uk-UA")},
          {base::i18n::GetKnownLanguageTag("ur"),
           base::i18n::GetKnownLanguageTag("ur-PK")},
          {base::i18n::GetKnownLanguageTag("uz"),
           base::i18n::GetKnownLanguageTag("uz-UZ")},
          {base::i18n::GetKnownLanguageTag("vi"),
           base::i18n::GetKnownLanguageTag("vi-VN")},
          {base::i18n::GetKnownLanguageTag("yue"),
           base::i18n::GetKnownLanguageTag("yue-Hant-HK")},
          {base::i18n::GetKnownLanguageTag("zh"),
           base::i18n::GetKnownLanguageTag("zh-Hans")},
          {base::i18n::GetKnownLanguageTag("zu"),
           base::i18n::GetKnownLanguageTag("zu-ZA")},
      });

  // First check if this is a language code supported in the map above.
  auto iter = kLangsToDefaultLocales.find(lang_or_locale);
  if (iter != kLangsToDefaultLocales.end()) {
    return iter->second;
  }

  // If it's only a language code, we can return early, because no other
  // language-only codes are supported.
  if (lang_or_locale.region_subtag().empty() &&
      lang_or_locale.script_subtag().empty() &&
      lang_or_locale.variant_subtags().empty()) {
    return kDefaultProfileLocale;
  }

  // The code is a supported locale. Return itself.
  // Note that it doesn't matter if the supported locale is online or offline.
  if (Dictation::GetAllSupportedLocales().contains(lang_or_locale)) {
    return lang_or_locale;
  }

  // Finally, get the language code from the locale and try to use it to map
  // to a default locale. For example, "en-XX" should map to "en-US" if "en-XX"
  // does not exist.
  iter = kLangsToDefaultLocales.find(lang_or_locale.WithLanguageSubtagOnly());
  if (iter != kLangsToDefaultLocales.end()) {
    return iter->second;
  }
  return kDefaultProfileLocale;
}

}  // namespace

// static
const base::flat_map<base::i18n::LanguageTag, Dictation::LocaleData>
Dictation::GetAllSupportedLocales() {
  base::flat_map<base::i18n::LanguageTag, LocaleData> supported_locales;
  // If new RTL locales are added, ensure that
  // accessibility_common/dictation/commands.js RTLLocales is updated
  // appropriately.
  static constexpr base::i18n::LanguageTag kWebSpeechSupportedLocales[] = {
      base::i18n::GetKnownLanguageTag("af-ZA"),
      base::i18n::GetKnownLanguageTag("am-ET"),
      base::i18n::GetKnownLanguageTag("ar-AE"),
      base::i18n::GetKnownLanguageTag("ar-BH"),
      base::i18n::GetKnownLanguageTag("ar-DZ"),
      base::i18n::GetKnownLanguageTag("ar-EG"),
      base::i18n::GetKnownLanguageTag("ar-IL"),
      base::i18n::GetKnownLanguageTag("ar-IQ"),
      base::i18n::GetKnownLanguageTag("ar-JO"),
      base::i18n::GetKnownLanguageTag("ar-KW"),
      base::i18n::GetKnownLanguageTag("ar-LB"),
      base::i18n::GetKnownLanguageTag("ar-MA"),
      base::i18n::GetKnownLanguageTag("ar-OM"),
      base::i18n::GetKnownLanguageTag("ar-PS"),
      base::i18n::GetKnownLanguageTag("ar-QA"),
      base::i18n::GetKnownLanguageTag("ar-SA"),
      base::i18n::GetKnownLanguageTag("ar-TN"),
      base::i18n::GetKnownLanguageTag("ar-YE"),
      base::i18n::GetKnownLanguageTag("az-AZ"),
      base::i18n::GetKnownLanguageTag("bg-BG"),
      base::i18n::GetKnownLanguageTag("bn-BD"),
      base::i18n::GetKnownLanguageTag("bn-IN"),
      base::i18n::GetKnownLanguageTag("bs-BA"),
      base::i18n::GetKnownLanguageTag("ca-ES"),
      base::i18n::GetKnownLanguageTag("cs-CZ"),
      base::i18n::GetKnownLanguageTag("da-DK"),
      base::i18n::GetKnownLanguageTag("de-AT"),
      base::i18n::GetKnownLanguageTag("de-CH"),
      base::i18n::GetKnownLanguageTag("de-DE"),
      base::i18n::GetKnownLanguageTag("el-GR"),
      base::i18n::GetKnownLanguageTag("en-AU"),
      base::i18n::GetKnownLanguageTag("en-CA"),
      base::i18n::GetKnownLanguageTag("en-GB"),
      base::i18n::GetKnownLanguageTag("en-GH"),
      base::i18n::GetKnownLanguageTag("en-HK"),
      base::i18n::GetKnownLanguageTag("en-IE"),
      base::i18n::GetKnownLanguageTag("en-IN"),
      base::i18n::GetKnownLanguageTag("en-KE"),
      base::i18n::GetKnownLanguageTag("en-NG"),
      base::i18n::GetKnownLanguageTag("en-NZ"),
      base::i18n::GetKnownLanguageTag("en-PH"),
      base::i18n::GetKnownLanguageTag("en-PK"),
      base::i18n::GetKnownLanguageTag("en-SG"),
      base::i18n::GetKnownLanguageTag("en-TZ"),
      base::i18n::GetKnownLanguageTag("en-US"),
      base::i18n::GetKnownLanguageTag("en-ZA"),
      base::i18n::GetKnownLanguageTag("es-AR"),
      base::i18n::GetKnownLanguageTag("es-BO"),
      base::i18n::GetKnownLanguageTag("es-CL"),
      base::i18n::GetKnownLanguageTag("es-CO"),
      base::i18n::GetKnownLanguageTag("es-CR"),
      base::i18n::GetKnownLanguageTag("es-DO"),
      base::i18n::GetKnownLanguageTag("es-EC"),
      base::i18n::GetKnownLanguageTag("es-ES"),
      base::i18n::GetKnownLanguageTag("es-GT"),
      base::i18n::GetKnownLanguageTag("es-HN"),
      base::i18n::GetKnownLanguageTag("es-MX"),
      base::i18n::GetKnownLanguageTag("es-NI"),
      base::i18n::GetKnownLanguageTag("es-PA"),
      base::i18n::GetKnownLanguageTag("es-PE"),
      base::i18n::GetKnownLanguageTag("es-PR"),
      base::i18n::GetKnownLanguageTag("es-PY"),
      base::i18n::GetKnownLanguageTag("es-SV"),
      base::i18n::GetKnownLanguageTag("es-US"),
      base::i18n::GetKnownLanguageTag("es-UY"),
      base::i18n::GetKnownLanguageTag("es-VE"),
      base::i18n::GetKnownLanguageTag("et-EE"),
      base::i18n::GetKnownLanguageTag("eu-ES"),
      base::i18n::GetKnownLanguageTag("fa-IR"),
      base::i18n::GetKnownLanguageTag("fi-FI"),
      base::i18n::GetKnownLanguageTag("fil-PH"),
      base::i18n::GetKnownLanguageTag("fr-BE"),
      base::i18n::GetKnownLanguageTag("fr-CA"),
      base::i18n::GetKnownLanguageTag("fr-CH"),
      base::i18n::GetKnownLanguageTag("fr-FR"),
      base::i18n::GetKnownLanguageTag("gl-ES"),
      base::i18n::GetKnownLanguageTag("gu-IN"),
      base::i18n::GetKnownLanguageTag("hi-IN"),
      base::i18n::GetKnownLanguageTag("hr-HR"),
      base::i18n::GetKnownLanguageTag("hu-HU"),
      base::i18n::GetKnownLanguageTag("hy-AM"),
      base::i18n::GetKnownLanguageTag("id-ID"),
      base::i18n::GetKnownLanguageTag("is-IS"),
      base::i18n::GetKnownLanguageTag("it-CH"),
      base::i18n::GetKnownLanguageTag("it-IT"),
      base::i18n::GetKnownLanguageTag("he-IL"),
      base::i18n::GetKnownLanguageTag("ja-JP"),
      base::i18n::GetKnownLanguageTag("jv-ID"),
      base::i18n::GetKnownLanguageTag("ka-GE"),
      base::i18n::GetKnownLanguageTag("kk-KZ"),
      base::i18n::GetKnownLanguageTag("km-KH"),
      base::i18n::GetKnownLanguageTag("kn-IN"),
      base::i18n::GetKnownLanguageTag("ko-KR"),
      base::i18n::GetKnownLanguageTag("lo-LA"),
      base::i18n::GetKnownLanguageTag("lt-LT"),
      base::i18n::GetKnownLanguageTag("lv-LV"),
      base::i18n::GetKnownLanguageTag("mk-MK"),
      base::i18n::GetKnownLanguageTag("ml-IN"),
      base::i18n::GetKnownLanguageTag("mn-MN"),
      base::i18n::GetKnownLanguageTag("mr-IN"),
      base::i18n::GetKnownLanguageTag("ms-MY"),
      base::i18n::GetKnownLanguageTag("my-MM"),
      base::i18n::GetKnownLanguageTag("ne-NP"),
      base::i18n::GetKnownLanguageTag("nl-BE"),
      base::i18n::GetKnownLanguageTag("nl-NL"),
      base::i18n::GetKnownLanguageTag("no-NO"),
      base::i18n::GetKnownLanguageTag("pa-Guru-IN"),
      base::i18n::GetKnownLanguageTag("pl-PL"),
      base::i18n::GetKnownLanguageTag("pt-BR"),
      base::i18n::GetKnownLanguageTag("pt-PT"),
      base::i18n::GetKnownLanguageTag("ro-RO"),
      base::i18n::GetKnownLanguageTag("ru-RU"),
      base::i18n::GetKnownLanguageTag("si-LK"),
      base::i18n::GetKnownLanguageTag("sk-SK"),
      base::i18n::GetKnownLanguageTag("sl-SI"),
      base::i18n::GetKnownLanguageTag("sq-AL"),
      base::i18n::GetKnownLanguageTag("sr-RS"),
      base::i18n::GetKnownLanguageTag("su-ID"),
      base::i18n::GetKnownLanguageTag("sv-SE"),
      base::i18n::GetKnownLanguageTag("sw-KE"),
      base::i18n::GetKnownLanguageTag("sw-TZ"),
      base::i18n::GetKnownLanguageTag("ta-IN"),
      base::i18n::GetKnownLanguageTag("ta-LK"),
      base::i18n::GetKnownLanguageTag("ta-MY"),
      base::i18n::GetKnownLanguageTag("ta-SG"),
      base::i18n::GetKnownLanguageTag("te-IN"),
      base::i18n::GetKnownLanguageTag("th-TH"),
      base::i18n::GetKnownLanguageTag("tr-TR"),
      base::i18n::GetKnownLanguageTag("uk-UA"),
      base::i18n::GetKnownLanguageTag("ur-IN"),
      base::i18n::GetKnownLanguageTag("ur-PK"),
      base::i18n::GetKnownLanguageTag("uz-UZ"),
      base::i18n::GetKnownLanguageTag("vi-VN"),
      base::i18n::GetKnownLanguageTag("yue-Hant-HK"),
      base::i18n::GetKnownLanguageTag("zh-Hans"),
      base::i18n::GetKnownLanguageTag("zh-TW"),
      base::i18n::GetKnownLanguageTag("zu-ZA"),
      base::i18n::GetKnownLanguageTag("ar-001"),
  };

  for (const base::i18n::LanguageTag& locale : kWebSpeechSupportedLocales) {
    // By default these languages are not supported offline.
    supported_locales[locale] = LocaleData();
  }
  if (features::IsDictationOfflineAvailable()) {
    speech::SodaInstaller* soda_installer =
        speech::SodaInstaller::GetInstance();
    std::vector<std::string> offline_locales =
        soda_installer->GetAvailableLanguages();
    for (const auto& locale : offline_locales) {
      std::optional<base::i18n::LanguageTag> locale_tag =
          base::i18n::GetLanguageTagFromString(locale);
      if (!locale_tag) {
        continue;
      }
      // These are supported offline.
      supported_locales[*locale_tag] = LocaleData();
      supported_locales[*locale_tag].works_offline = true;
      supported_locales[*locale_tag].installed =
          soda_installer->IsSodaInstalled(speech::GetLanguageCode(locale));
    }
  }
  return supported_locales;
}

// static
base::i18n::LanguageTag Dictation::DetermineDefaultSupportedLocale(
    Profile* profile,
    bool new_user,
    const base::i18n::LanguageTag& application_locale) {
  base::i18n::LanguageTag lang_or_locale =
      new_user ? application_locale
               : GetUserLangOrLocaleFromSystem(profile, application_locale);
  return GetSupportedLocale(lang_or_locale);
}

}  // namespace ash
