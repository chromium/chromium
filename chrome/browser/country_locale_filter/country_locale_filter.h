// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_COUNTRY_LOCALE_FILTER_COUNTRY_LOCALE_FILTER_H_
#define CHROME_BROWSER_COUNTRY_LOCALE_FILTER_COUNTRY_LOCALE_FILTER_H_

#include <string>
#include <string_view>

#include "base/i18n/language_tag.h"

// Helpers for restricting a feature to particular countries and locales on the
// client, configured by allow and deny lists (typically supplied by feature
// params with compiled-in defaults, so that the restriction applies even
// without a Finch config but can be adjusted by one).
//
// Lists are separated by commas and/or whitespace. The deny list takes
// precedence over the allow list. An allow list consisting of exactly "*"
// allows everything that is not in the deny list; "*" has no special meaning
// anywhere else (e.g., as one entry of a longer allow list, or in a deny list).
//
// Countries are matched ignoring ASCII case; unknown (empty) countries never
// match. Locale list entries are parsed as BCP47 language tags and compared
// after canonicalization, so "en_us" matches "en-US"; entries which cannot be
// parsed are ignored. Matching is otherwise exact; "en" does not match "en-US".
namespace country_locale_filter {

// Which of the client's variations countries are matched against country lists.
enum class CountryMatch {
  // The session (latest) country, or the permanent country if the session
  // country is unknown (e.g., before the first variations seed fetch).
  kSession,
  // Either the session or the permanent country. Note that this means that a
  // client is blocked if either country is in the deny list.
  kSessionOrPermanent,
};

// The signals used to evaluate a client's region.
struct RegionSignals {
  RegionSignals(std::string permanent_country,
                std::string session_country,
                base::i18n::LanguageTag locale);
  RegionSignals(const RegionSignals&);
  RegionSignals(RegionSignals&&);
  RegionSignals& operator=(const RegionSignals&);
  RegionSignals& operator=(RegionSignals&&);
  ~RegionSignals();

  // ISO 3166-1 alpha-2 country codes, which are compared case-insensitively.
  // Typically, these are the variations service's permanent country and latest
  // (session) country. Either may be empty if unknown.
  std::string permanent_country;
  std::string session_country;

  // Resolved application (UI) locale, e.g., "en-US".
  base::i18n::LanguageTag locale;
};

// Allow and deny lists for both country and locale. The referenced strings must
// outlive this object.
struct FilterLists {
  std::string_view enabled_countries;
  std::string_view disabled_countries;
  std::string_view enabled_locales;
  std::string_view disabled_locales;
};

// The outcome of evaluating both country and locale.
struct RegionFilterResult {
  bool country_allowed = false;
  bool locale_allowed = false;

  // Returns true if both the country and locale are allowed.
  bool IsAllowed() const;
};

// Evaluates `signals` against `lists`, matching countries according to
// `country_match`.
RegionFilterResult Evaluate(const FilterLists& lists,
                            const RegionSignals& signals,
                            CountryMatch country_match);

}  // namespace country_locale_filter

#endif  // CHROME_BROWSER_COUNTRY_LOCALE_FILTER_COUNTRY_LOCALE_FILTER_H_
