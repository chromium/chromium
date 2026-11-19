// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/country_locale_filter/country_locale_filter.h"

#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"
#include "base/i18n/tag_converters.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace country_locale_filter {
namespace {

// Evaluates only the country, with any locale allowed.
bool IsCountryAllowed(std::string_view enabled_countries,
                      std::string_view disabled_countries,
                      std::string_view permanent_country,
                      std::string_view session_country,
                      CountryMatch match = CountryMatch::kSessionOrPermanent) {
  RegionFilterResult result =
      Evaluate({.enabled_countries = enabled_countries,
                .disabled_countries = disabled_countries,
                .enabled_locales = "*"},
               {std::string(permanent_country), std::string(session_country),
                base::i18n::GetKnownLanguageTag("en-US")},
               match);
  EXPECT_TRUE(result.locale_allowed);
  return result.country_allowed;
}

// Evaluates only the locale, with any country allowed.
bool IsLocaleAllowed(std::string_view enabled_locales,
                     std::string_view disabled_locales,
                     const base::i18n::LanguageTag& locale) {
  RegionFilterResult result =
      Evaluate({.enabled_countries = "*",
                .enabled_locales = enabled_locales,
                .disabled_locales = disabled_locales},
               {"us", "us", locale}, CountryMatch::kSessionOrPermanent);
  EXPECT_TRUE(result.country_allowed);
  return result.locale_allowed;
}

TEST(CountryLocaleFilterTest, CountryInAllowList) {
  EXPECT_TRUE(IsCountryAllowed("us", "", "us", ""));
  EXPECT_TRUE(IsCountryAllowed("ca,us,gb", "", "us", ""));
}

TEST(CountryLocaleFilterTest, CountryNotInAllowList) {
  EXPECT_FALSE(IsCountryAllowed("us", "", "ca", ""));
  EXPECT_FALSE(IsCountryAllowed("", "", "us", ""));
}

TEST(CountryLocaleFilterTest, CountryMatchesEitherPermanentOrSession) {
  EXPECT_TRUE(IsCountryAllowed("us", "", "us", "ca"));
  EXPECT_TRUE(IsCountryAllowed("us", "", "ca", "us"));
  EXPECT_FALSE(IsCountryAllowed("us", "", "ca", "gb"));
}

TEST(CountryLocaleFilterTest, CountryIsCaseInsensitive) {
  EXPECT_TRUE(IsCountryAllowed("US", "", "us", ""));
  EXPECT_TRUE(IsCountryAllowed("us", "", "US", ""));
  EXPECT_TRUE(IsCountryAllowed("us", "", "ca", "Us"));
}

TEST(CountryLocaleFilterTest, EmptyCountriesNeverMatch) {
  EXPECT_FALSE(IsCountryAllowed("us", "", "", ""));
  // An empty session country must not match anything, even via odd lists.
  EXPECT_FALSE(IsCountryAllowed("us,,", "", "ca", ""));
}

TEST(CountryLocaleFilterTest, CountryWildcard) {
  EXPECT_TRUE(IsCountryAllowed("*", "", "us", ""));
  EXPECT_TRUE(IsCountryAllowed("*", "", "", ""));
  // The wildcard only applies when it is the sole entry.
  EXPECT_FALSE(IsCountryAllowed("*,ca", "", "us", ""));
}

TEST(CountryLocaleFilterTest, CountryDenyListTakesPrecedence) {
  EXPECT_FALSE(IsCountryAllowed("us", "us", "us", ""));
  EXPECT_FALSE(IsCountryAllowed("*", "gb", "gb", ""));
  EXPECT_TRUE(IsCountryAllowed("*", "gb", "us", ""));
  // Either country being denied blocks.
  EXPECT_FALSE(IsCountryAllowed("*", "gb", "us", "gb"));
  EXPECT_FALSE(IsCountryAllowed("us,gb", "gb", "gb", "us"));
}

TEST(CountryLocaleFilterTest, ListSeparators) {
  EXPECT_TRUE(IsCountryAllowed(" ca , us ", "", "us", ""));
  EXPECT_TRUE(IsCountryAllowed("ca us\tgb\nau", "", "au", ""));
  EXPECT_TRUE(IsCountryAllowed("'ca','us'", "", "us", ""));
  EXPECT_TRUE(IsCountryAllowed("\"*\"", "", "ca", ""));
}

TEST(CountryLocaleFilterTest, SessionCountryMatch) {
  // When the session country is known, only it is considered, for both the
  // allow and deny lists.
  EXPECT_FALSE(IsCountryAllowed("us", "", "us", "ca", CountryMatch::kSession));
  EXPECT_TRUE(IsCountryAllowed("us", "", "ca", "us", CountryMatch::kSession));
  EXPECT_TRUE(IsCountryAllowed("*", "ca", "ca", "us", CountryMatch::kSession));
  EXPECT_FALSE(IsCountryAllowed("*", "ca", "us", "ca", CountryMatch::kSession));

  // Otherwise, the permanent country is used.
  EXPECT_TRUE(IsCountryAllowed("us", "", "us", "", CountryMatch::kSession));
  EXPECT_FALSE(IsCountryAllowed("us", "", "ca", "", CountryMatch::kSession));
  EXPECT_FALSE(IsCountryAllowed("us", "", "", "", CountryMatch::kSession));
}

TEST(CountryLocaleFilterTest, LocaleInAllowList) {
  constexpr base::i18n::LanguageTag kEnUS =
      base::i18n::GetKnownLanguageTag("en-US");
  EXPECT_TRUE(IsLocaleAllowed("en-US", "", kEnUS));
  EXPECT_TRUE(IsLocaleAllowed("en-GB,en-US", "", kEnUS));
}

TEST(CountryLocaleFilterTest, LocaleListCanonicalization) {
  constexpr base::i18n::LanguageTag kEnUS =
      base::i18n::GetKnownLanguageTag("en-US");
  EXPECT_TRUE(IsLocaleAllowed("en-us", "", kEnUS));
  EXPECT_TRUE(IsLocaleAllowed("en_US", "", kEnUS));
  EXPECT_TRUE(IsLocaleAllowed("EN_us", "", kEnUS));
  // Deprecated language codes are replaced with their preferred equivalents.
  EXPECT_TRUE(IsLocaleAllowed("iw", "", base::i18n::GetKnownLanguageTag("he")));
}

TEST(CountryLocaleFilterTest, MalformedLocaleEntriesAreIgnored) {
  constexpr base::i18n::LanguageTag kEnUS =
      base::i18n::GetKnownLanguageTag("en-US");
  EXPECT_FALSE(IsLocaleAllowed("e", "", kEnUS));
  EXPECT_TRUE(IsLocaleAllowed("e,en-US", "", kEnUS));
  EXPECT_TRUE(IsLocaleAllowed("en-US", "e", kEnUS));
}

TEST(CountryLocaleFilterTest, LocaleMatchingIsExact) {
  EXPECT_FALSE(
      IsLocaleAllowed("en-US", "", base::i18n::GetKnownLanguageTag("en-GB")));
  EXPECT_FALSE(
      IsLocaleAllowed("en", "", base::i18n::GetKnownLanguageTag("en-US")));
  EXPECT_FALSE(
      IsLocaleAllowed("es", "", base::i18n::GetKnownLanguageTag("es-419")));
}

TEST(CountryLocaleFilterTest, LocaleWildcardAndDenyList) {
  constexpr base::i18n::LanguageTag kFr = base::i18n::GetKnownLanguageTag("fr");
  EXPECT_TRUE(IsLocaleAllowed("*", "", kFr));
  EXPECT_FALSE(IsLocaleAllowed("*", "fr", kFr));
  EXPECT_FALSE(IsLocaleAllowed("en-US", "en_us",
                               base::i18n::GetKnownLanguageTag("en-US")));
}

TEST(CountryLocaleFilterTest, Evaluate) {
  const FilterLists lists{
      .enabled_countries = "us",
      .disabled_countries = "",
      .enabled_locales = "en-US",
      .disabled_locales = "",
  };

  RegionFilterResult result =
      Evaluate(lists, {"us", "", base::i18n::GetKnownLanguageTag("en-US")},
               CountryMatch::kSessionOrPermanent);
  EXPECT_TRUE(result.country_allowed);
  EXPECT_TRUE(result.locale_allowed);
  EXPECT_TRUE(result.IsAllowed());

  result = Evaluate(lists, {"ca", "", base::i18n::GetKnownLanguageTag("en-US")},
                    CountryMatch::kSessionOrPermanent);
  EXPECT_FALSE(result.country_allowed);
  EXPECT_TRUE(result.locale_allowed);
  EXPECT_FALSE(result.IsAllowed());

  result = Evaluate(lists, {"us", "", base::i18n::GetKnownLanguageTag("fr")},
                    CountryMatch::kSessionOrPermanent);
  EXPECT_TRUE(result.country_allowed);
  EXPECT_FALSE(result.locale_allowed);
  EXPECT_FALSE(result.IsAllowed());
}

}  // namespace
}  // namespace country_locale_filter
