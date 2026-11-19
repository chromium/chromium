// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/country_locale_filter/country_locale_filter.h"

#include <algorithm>
#include <utility>
#include <vector>

#include "base/containers/span.h"
#include "base/i18n/tag_converters.h"
#include "base/strings/string_split.h"
#include "base/strings/string_util.h"

namespace country_locale_filter {

namespace {

// Separators accepted between list entries. Quotes are included so that lists
// which have accidentally been quoted are still interpreted as intended.
constexpr char kListSeparators[] = ", \t\n'\"";

std::vector<std::string_view> SplitList(std::string_view list) {
  return base::SplitStringPiece(list, kListSeparators, base::TRIM_WHITESPACE,
                                base::SPLIT_WANT_NONEMPTY);
}

bool IsWildcard(base::span<const std::string_view> list) {
  return list.size() == 1 && list[0] == "*";
}

// Evaluates allow and deny lists, given a predicate which determines whether a
// list entry matches the client. The deny list takes precedence.
template <typename Matches>
bool IsAllowedByLists(std::string_view enabled_list,
                      std::string_view disabled_list,
                      Matches matches) {
  if (std::ranges::any_of(SplitList(disabled_list), matches)) {
    return false;
  }
  std::vector<std::string_view> enabled = SplitList(enabled_list);
  return IsWildcard(enabled) || std::ranges::any_of(enabled, matches);
}

// Returns whether a country list entry matches `country`. Unknown (empty)
// countries never match.
bool CountryMatches(std::string_view entry, std::string_view country) {
  return !country.empty() && base::EqualsCaseInsensitiveASCII(entry, country);
}

bool IsCountryAllowed(std::string_view enabled_list,
                      std::string_view disabled_list,
                      std::string_view permanent_country,
                      std::string_view session_country,
                      CountryMatch match) {
  switch (match) {
    case CountryMatch::kSession: {
      // Fall back to the permanent country only if the session country is
      // unknown.
      std::string_view country =
          session_country.empty() ? permanent_country : session_country;
      return IsAllowedByLists(enabled_list, disabled_list,
                              [&](std::string_view entry) {
                                return CountryMatches(entry, country);
                              });
    }
    case CountryMatch::kSessionOrPermanent:
      return IsAllowedByLists(enabled_list, disabled_list,
                              [&](std::string_view entry) {
                                return CountryMatches(entry, session_country) ||
                                       CountryMatches(entry, permanent_country);
                              });
  }
}

bool IsLocaleAllowed(std::string_view enabled_list,
                     std::string_view disabled_list,
                     const base::i18n::LanguageTag& locale) {
  return IsAllowedByLists(
      enabled_list, disabled_list, [&](std::string_view entry) {
        return base::i18n::GetLanguageTagFromString(entry) == locale;
      });
}

}  // namespace

RegionSignals::RegionSignals(std::string permanent_country,
                             std::string session_country,
                             base::i18n::LanguageTag locale)
    : permanent_country(std::move(permanent_country)),
      session_country(std::move(session_country)),
      locale(std::move(locale)) {}
RegionSignals::RegionSignals(const RegionSignals&) = default;
RegionSignals::RegionSignals(RegionSignals&&) = default;
RegionSignals& RegionSignals::operator=(const RegionSignals&) = default;
RegionSignals& RegionSignals::operator=(RegionSignals&&) = default;
RegionSignals::~RegionSignals() = default;

bool RegionFilterResult::IsAllowed() const {
  return country_allowed && locale_allowed;
}

RegionFilterResult Evaluate(const FilterLists& lists,
                            const RegionSignals& signals,
                            CountryMatch country_match) {
  return {
      .country_allowed = IsCountryAllowed(
          lists.enabled_countries, lists.disabled_countries,
          signals.permanent_country, signals.session_country, country_match),
      .locale_allowed = IsLocaleAllowed(lists.enabled_locales,
                                        lists.disabled_locales, signals.locale),
  };
}

}  // namespace country_locale_filter
