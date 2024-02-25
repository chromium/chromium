// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.
//
// This file defines a helper class for selecting a supported language from a
// set of candidates. It is used to get localized strings that are directly
// embedded into the executable / library instead of stored in external
// .pak files.

#include "base/win/embedded_i18n/language_selector.h"

#include <algorithm>
#include <functional>
#include <string_view>

#include "base/check_op.h"
#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "base/win/i18n.h"

namespace base {
namespace win {
namespace i18n {

namespace {

using LangToOffset = LanguageSelector::LangToOffset;

// Holds pointers to LangToOffset pairs for specific languages that are the
// targets of exceptions (where one language is mapped to another) or wildcards
// (where a raw language identifier is mapped to a specific localization).
struct AvailableLanguageAliases {
  raw_ptr<const LangToOffset> en_gb_language_offset;
  raw_ptr<const LangToOffset> en_us_language_offset;
  raw_ptr<const LangToOffset> es_language_offset;
  raw_ptr<const LangToOffset> es_419_language_offset;
  raw_ptr<const LangToOffset> fil_language_offset;
  raw_ptr<const LangToOffset> iw_language_offset;
  raw_ptr<const LangToOffset> no_language_offset;
  raw_ptr<const LangToOffset> pt_br_language_offset;
  raw_ptr<const LangToOffset> zh_cn_language_offset;
  raw_ptr<const LangToOffset> zh_tw_language_offset;
};

#if DCHECK_IS_ON()
// Returns true if the items in the given range are sorted and lower cased.
bool IsArraySortedAndLowerCased(span<const LangToOffset> languages_to_offset) {
  return std::is_sorted(languages_to_offset.begin(),
                        languages_to_offset.end()) &&
         base::ranges::all_of(languages_to_offset, [](const auto& lang) {
           auto language = AsStringPiece16(lang.first);
           return ToLowerASCII(language) == language;
         });
}
#endif  // DCHECK_IS_ON()

// Determines the availability of all languages that may be used as aliases in
// GetAliasedLanguageOffset or GetCompatibleNeutralLanguageOffset
AvailableLanguageAliases DetermineAvailableAliases(
    span<const LangToOffset> languages_to_offset) {
  AvailableLanguageAliases available_aliases = {};

  for (const LangToOffset& lang_to_offset : languages_to_offset) {
    if (lang_to_offset.first == L"en-gb")
      available_aliases.en_gb_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"en-us")
      available_aliases.en_us_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"es")
      available_aliases.es_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"es-419")
      available_aliases.es_419_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"fil")
      available_aliases.fil_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"iw")
      available_aliases.iw_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"no")
      available_aliases.no_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"pt-br")
      available_aliases.pt_br_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"zh-cn")
      available_aliases.zh_cn_language_offset = &lang_to_offset;
    else if (lang_to_offset.first == L"zh-tw")
      available_aliases.zh_tw_language_offset = &lang_to_offset;
  }

  // Fallback language must exist.
  DCHECK(available_aliases.en_us_language_offset);
  return available_aliases;
}

// Returns true if a LangToOffset entry can be found in |languages_to_offset|
// that matches the |language| exactly. |offset| will store the offset of the
// language that matches if any. |languages_to_offset| must be sorted by
// language and all languages must lower case.
bool GetExactLanguageOffset(span<const LangToOffset> languages_to_offset,
                            const std::wstring& language,
                            const LangToOffset** matched_language_to_offset) {
  DCHECK(matched_language_to_offset);

  // Binary search in the sorted arrays to find the offset corresponding
  // to a given language |name|.
  auto search_result = std::lower_bound(
      languages_to_offset.begin(), languages_to_offset.end(), language,
      [](const LangToOffset& left, const std::wstring& to_find) {
        return left.first < to_find;
      });
  if (languages_to_offset.end() != search_result &&
      search_result->first == language) {
    *matched_language_to_offset = &*search_result;
    return true;
  }
  return false;
}

// Returns true if the current language can be aliased to another language.
bool GetAliasedLanguageOffset(const AvailableLanguageAliases& available_aliases,
                              const std::wstring& language,
                              const LangToOffset** matched_language_to_offset) {
  DCHECK(matched_language_to_offset);

  // Alias some English variants to British English (all others wildcard to
  // US).
  if (available_aliases.en_gb_language_offset &&
      (language == L"en-au" || language == L"en-ca" || language == L"en-nz" ||
       language == L"en-za")) {
    *matched_language_to_offset = available_aliases.en_gb_language_offset;
    return true;
  }
  // Alias es-es to es (all others wildcard to es-419).
  if (available_aliases.es_language_offset && language == L"es-es") {
    *matched_language_to_offset = available_aliases.es_language_offset;
    return true;
  }
  // Google web properties use iw for he. Handle both just to be safe.
  if (available_aliases.iw_language_offset && language == L"he") {
    *matched_language_to_offset = available_aliases.iw_language_offset;
    return true;
  }
  // Google web properties use no for nb. Handle both just to be safe.
  if (available_aliases.no_language_offset && language == L"nb") {
    *matched_language_to_offset = available_aliases.no_language_offset;
    return true;
  }
  // Some Google web properties use tl for fil. Handle both just to be safe.
  // They're not completely identical, but alias it here.
  if (available_aliases.fil_language_offset && language == L"tl") {
    *matched_language_to_offset = available_aliases.fil_language_offset;
    return true;
  }
  if (available_aliases.zh_cn_language_offset &&
      // Pre-Vista alias for Chinese w/ script subtag.
      (language == L"zh-chs" ||
       // Vista+ alias for Chinese w/ script subtag.
       language == L"zh-hans" ||
       // Although the wildcard entry for zh would result in this, alias zh-sg
       // so that it will win if it precedes another valid tag in a list of
       // candidates.
       language == L"zh-sg")) {
    *matched_language_to_offset = available_aliases.zh_cn_language_offset;
    return true;
  }
  if (available_aliases.zh_tw_language_offset &&
      // Pre-Vista alias for Chinese w/ script subtag.
      (language == L"zh-cht" ||
       // Vista+ alias for Chinese w/ script subtag.
       language == L"zh-hant" ||
       // Alias Hong Kong and Macau to Taiwan.
       language == L"zh-hk" || language == L"zh-mo")) {
    *matched_language_to_offset = available_aliases.zh_tw_language_offset;
    return true;
  }

  return false;
}

// Returns true if the current neutral language can be aliased to another
// language.
bool GetCompatibleNeutralLanguageOffset(
    const AvailableLanguageAliases& available_aliases,
    const std::wstring& neutral_language,
    const LangToOffset** matched_language_to_offset) {
  DCHECK(matched_language_to_offset);

  if (available_aliases.en_us_language_offset && neutral_language == L"en") {
    // Use the U.S. region for anything English.
    *matched_language_to_offset = available_aliases.en_us_language_offset;
    return true;
  }
  if (available_aliases.es_419_language_offset && neutral_language == L"es") {
    // Use the Latin American region for anything Spanish.
    *matched_language_to_offset = available_aliases.es_419_language_offset;
    return true;
  }
  if (available_aliases.pt_br_language_offset && neutral_language == L"pt") {
    // Use the Brazil region for anything Portugese.
    *matched_language_to_offset = available_aliases.pt_br_language_offset;
    return true;
  }
  if (available_aliases.zh_cn_language_offset && neutral_language == L"zh") {
    // Use the P.R.C. region for anything Chinese.
    *matched_language_to_offset = available_aliases.zh_cn_language_offset;
    return true;
  }

  return false;
}

// Runs through the set of candidates, sending their downcased representation
// through |select_predicate|.  Returns true if the predicate selects a
// candidate, in which case |matched_name| is assigned the value of the
// candidate and |matched_offset| is assigned the language offset of the
// selected translation.
// static
bool SelectIf(const std::vector<std::wstring>& candidates,
              span<const LangToOffset> languages_to_offset,
              const AvailableLanguageAliases& available_aliases,
              const LangToOffset** matched_language_to_offset,
              std::wstring* matched_name) {
  DCHECK(matched_language_to_offset);
  DCHECK(matched_name);

  // Note: always perform the exact match first so that an alias is never
  // selected in place of a future translation.

  // An earlier candidate entry matching on an exact match or alias match takes
  // precedence over a later candidate entry matching on an exact match.
  for (const std::wstring& scan : candidates) {
    std::wstring lower_case_candidate =
        AsWString(ToLowerASCII(AsStringPiece16(scan)));
    if (GetExactLanguageOffset(languages_to_offset, lower_case_candidate,
                               matched_language_to_offset) ||
        GetAliasedLanguageOffset(available_aliases, lower_case_candidate,
                                 matched_language_to_offset)) {
      matched_name->assign(scan);
      return true;
    }
  }

  // If no candidate matches exactly or by alias, try to match by locale neutral
  // language.
  for (const std::wstring& scan : candidates) {
    std::wstring lower_case_candidate =
        AsWString(ToLowerASCII(AsStringPiece16(scan)));

    // Extract the locale neutral language from the language to search and try
    // to find an exact match for that language in the provided table.
    std::wstring neutral_language =
        lower_case_candidate.substr(0, lower_case_candidate.find(L'-'));

    if (GetCompatibleNeutralLanguageOffset(available_aliases, neutral_language,
                                           matched_language_to_offset)) {
      matched_name->assign(scan);
      return true;
    }
  }

  return false;
}

void SelectLanguageMatchingCandidate(
    const std::vector<std::wstring>& candidates,
    span<const LangToOffset> languages_to_offset,
    size_t* selected_offset,
    std::wstring* matched_candidate,
    std::wstring* selected_language) {
  DCHECK(selected_offset);
  DCHECK(matched_candidate);
  DCHECK(selected_language);
  DCHECK(!languages_to_offset.empty());
  DCHECK_EQ(static_cast<size_t>(*selected_offset), languages_to_offset.size());
  DCHECK(matched_candidate->empty());
  DCHECK(selected_language->empty());
  // Note: While DCHECK_IS_ON() seems redundant here, this is required to avoid
  // compilation errors, since IsArraySortedAndLowerCased is not defined
  // otherwise.
#if DCHECK_IS_ON()
  DCHECK(IsArraySortedAndLowerCased(languages_to_offset))
      << "languages_to_offset is not sorted and lower cased";
#endif  // DCHECK_IS_ON()

  // Get which languages that are commonly used as aliases and wildcards are
  // available for use to match candidates.
  AvailableLanguageAliases available_aliases =
      DetermineAvailableAliases(languages_to_offset);

  // The fallback must exist.
  DCHECK(available_aliases.en_us_language_offset);

  // Try to find the first matching candidate from all the language mappings
  // that are given. Failing that, used en-us as the fallback language.
  const LangToOffset* matched_language_to_offset = nullptr;
  if (!SelectIf(candidates, languages_to_offset, available_aliases,
                &matched_language_to_offset, matched_candidate)) {
    matched_language_to_offset = available_aliases.en_us_language_offset;
    *matched_candidate =
        std::wstring(available_aliases.en_us_language_offset->first);
  }

  DCHECK(matched_language_to_offset);
  // Get the real language being used for the matched candidate.
  *selected_language = std::wstring(matched_language_to_offset->first);
  *selected_offset = matched_language_to_offset->second;
}

std::vector<std::wstring> GetCandidatesFromSystem(
    std::wstring_view preferred_language) {
  std::vector<std::wstring> candidates;

  // Get the initial candidate list for this particular implementation (if
  // applicable).
  if (!preferred_language.empty())
    candidates.emplace_back(preferred_language);

  // Now try the UI languages.  Use the thread preferred ones since that will
  // kindly return us a list of all kinds of fallbacks.
  win::i18n::GetThreadPreferredUILanguageList(&candidates);
  return candidates;
}

}  // namespace

LanguageSelector::LanguageSelector(std::wstring_view preferred_language,
                                   span<const LangToOffset> languages_to_offset)
    : LanguageSelector(GetCandidatesFromSystem(preferred_language),
                       languages_to_offset) {}

LanguageSelector::LanguageSelector(const std::vector<std::wstring>& candidates,
                                   span<const LangToOffset> languages_to_offset)
    : selected_offset_(languages_to_offset.size()) {
  SelectLanguageMatchingCandidate(candidates, languages_to_offset,
                                  &selected_offset_, &matched_candidate_,
                                  &selected_language_);
}

LanguageSelector::~LanguageSelector() = default;

}  // namespace i18n
}  // namespace win
}  // namespace base
