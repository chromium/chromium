// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/locale/locale_change_guard.h"

#include <stddef.h>
#include <string.h>

#include "base/containers/fixed_flat_set.h"
#include "testing/gtest/include/gtest/gtest.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash {

namespace {

// These languages require user notification when locale is automatically
// switched between different regions within the same language.
constexpr auto kShowNotificationLanguages =
    base::MakeFixedFlatSet<std::string_view>({
        "af",   // Afrikaans
        "ak",   // Twi
        "am",   // Amharic
        "an",   // Aragonese
        "ar",   // Arabic
        "as",   // Assamese
        "ast",  // Asturian
        "ay",   // Aymara
        "az",   // Azerbaijani
        "be",   // Belarusian
        "bg",   // Bulgarian
        "bh",   // Bihari
        "bho",  // Bhojpuri
        "bm",   // Bambara
        "bn",   // Bengali
        "br",   // Breton
        "bs",   // Bosnian
        "ca",   // Catalan
        "ceb",  // Cebuano
        "chr",  // Cherokee
        "ckb",  // Sorani (Kurdish-Arabic)
        "co",   // Corsican
        "cs",   // Czech
        "cy",   // Welsh
        "da",   // Danish
        "doi",  // Dogri
        "dv",   // Dhivehi
        "ee",   // Ewe
        "el",   // Greek
        "eo",   // Esperanto
        "es",   // Spanish
        "et",   // Estonian
        "eu",   // Basque
        "fa",   // Persian
        "fi",   // Finnish
        "fil",  // Filipino
        "fo",   // Faroese
        "fy",   // Frisian
        "ga",   // Irish
        "gd",   // Scots Gaelic
        "gl",   // Galician
        "gn",   // Guarani
        "gu",   // Gujarati
        "ha",   // Hausa
        "haw",  // Hawaiian
        "he",   // Hebrew
        "hi",   // Hindi
        "hmn",  // Hmong
        "hr",   // Croatian
        "ht",   // Haitian Creole
        "hu",   // Hungarian
        "hy",   // Armenian
        "ia",   // Interlingua
        "id",   // Indonesian
        "ig",   // Igbo
        "ilo",  // Ilocano
        "is",   // Icelandic
        "ja",   // Japanese
        "jv",   // Javanese
        "ka",   // Georgian
        "kk",   // Kazakh
        "km",   // Cambodian
        "kn",   // Kannada
        "ko",   // Korean
        "kok",  // Konkani
        "kri",  // Krio
        "ku",   // Kurdish
        "ky",   // Kyrgyz
        "la",   // Latin
        "lb",   // Luxembourgish
        "lg",   // Luganda
        "ln",   // Lingala
        "lo",   // Laothian
        "lt",   // Lithuanian
        "lus",  // Mizo
        "lv",   // Latvian
        "mai",  // Maithili
        "mg",   // Malagasy
        "mi",   // Maori
        "mk",   // Macedonian
        "ml",   // Malayalam
        "mn",   // Mongolian
        "mni",  // Manipuri (Meitei Mayek)
        "mo",   // Moldavian
        "mr",   // Marathi
        "ms",   // Malay
        "mt",   // Maltese
        "my",   // Burmese
        "nb",   // Norwegian (Bokmal)
        "ne",   // Nepali
        "nl",   // Dutch
        "nn",   // Norwegian (Nynorsk)
        "no",   // Norwegian
        "nso",  // Sepedi
        "ny",   // Nyanja
        "oc",   // Occitan
        "om",   // Oromo
        "or",   // Oriya
        "pa",   // Punjabi
        "pl",   // Polish
        "ps",   // Pashto
        "pt",   // Portuguese
        "qu",   // Quechua
        "rm",   // Romansh
        "ro",   // Romanian
        "ru",   // Russian
        "rw",   // Kinyarwanda
        "sa",   // Sanskrit
        "sd",   // Sindhi
        "sh",   // Serbo-Croatian
        "si",   // Sinhalese
        "sk",   // Slovak
        "sl",   // Slovenian
        "sm",   // Samoan
        "sn",   // Shona
        "so",   // Somali
        "sq",   // Albanian
        "sr",   // Serbian
        "st",   // Sesotho
        "su",   // Sundanese
        "sv",   // Swedish
        "sw",   // Swahili
        "ta",   // Tamil
        "te",   // Telugu
        "tg",   // Tajik
        "th",   // Thai
        "ti",   // Tigrinya
        "tk",   // Turkmen
        "tn",   // Tswana
        "to",   // Tonga
        "tr",   // Turkish
        "ts",   // Tsonga
        "tt",   // Tatar
        "tw",   // Twi
        "ug",   // Uighur
        "uk",   // Ukrainian
        "ur",   // Urdu
        "uz",   // Uzbek
        "vi",   // Vietnamese
        "wa",   // Walloon
        "wo",   // Wolof
        "xh",   // Xhosa
        "yi",   // Yiddish
        "yo",   // Yoruba
        "zh",   // Chinese
        "zu",   // Zulu
    });

}  // namespace

TEST(LocaleChangeGuardTest, ShowNotificationLocaleChanged) {
  // "en" is used as "global default" in many places.
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en", "it"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("it", "en"));

  // Between two latin locales.
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("fr", "it"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("it", "fr"));

  // en <-> non-latin locale
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en", "zh"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("zh", "en"));

  // latin <-> non-latin locale
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("fr", "zh"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("zh", "fr"));

  // same language
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en", "en"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("fr", "fr"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("zh", "zh"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en", "en-US"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en-GB", "en-US"));

  // Different regions within the same language
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en", "en-au"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en-AU", "en"));
  EXPECT_FALSE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("en-AU", "en-GB"));

  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("zh", "zh-CN"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("zh-CN", "zh-TW"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("es", "es-419"));
  EXPECT_TRUE(
      LocaleChangeGuard::ShouldShowLocaleChangeNotification("es", "es-ES"));
}

TEST(LocaleChangeGuardTest, ShowNotificationLocaleChangedList) {
  for (std::string_view locale : l10n_util::GetAcceptLanguageListForTesting()) {
    const std::string language = std::string(locale, 0, locale.find('-'));

    const bool notification_allowed =
        kShowNotificationLanguages.contains(language);

    const char* const* skipped_begin =
        LocaleChangeGuard::GetSkipShowNotificationLanguagesForTesting();
    const char* const* skipped_end =
        skipped_begin +
        LocaleChangeGuard::GetSkipShowNotificationLanguagesSizeForTesting();
    const bool notification_skipped =
        (std::find(skipped_begin, skipped_end, language) != skipped_end);

    EXPECT_TRUE(notification_allowed ^ notification_skipped)
        << "Language '" << language << "' (from locale '" << locale
        << "') must be in exactly one list: either "
           "kSkipShowNotificationLanguages (found=" << notification_skipped
        << ") or kShowNotificationLanguages (found=" << notification_allowed
        << ").";
  }
}

}  // namespace ash
