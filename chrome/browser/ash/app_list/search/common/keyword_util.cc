// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"

#include "base/containers/flat_map.h"
#include "chromeos/ash/components/string_matching/fuzzy_tokenized_string_match.h"
#include "chromeos/ash/components/string_matching/tokenized_string.h"

namespace app_list {

// Default parameters.
constexpr bool kUseWeightedRatio = false;
constexpr double kRelevancyScoreThreshold = 0.75;

KeywordInfo::KeywordInfo(const std::u16string& query_token,
                         double relevance_score,
                         const std::vector<ProviderType>& search_providers)
    : query_token(query_token),
      relevance_score(relevance_score),
      search_providers(search_providers) {}

KeywordInfo::~KeywordInfo() = default;
KeywordInfo::KeywordInfo(const KeywordInfo& other) = default;

namespace {

using ::ash::string_matching::FuzzyTokenizedStringMatch;
using ::ash::string_matching::TokenizedString;

// Return a dictionary of keywords and their associated search providers.
// Structure: { keyword: [SearchProviders] }
KeywordToProvidersMap MakeMap() {
  KeywordToProvidersMap keyword_map(
      {{u"assistant", {ProviderType::kAssistantText}},
       {u"help", {ProviderType::kHelpApp}},
       {u"explore", {ProviderType::kHelpApp}},
       {u"shortcut", {ProviderType::kKeyboardShortcut}},
       {u"keyboard", {ProviderType::kKeyboardShortcut}},
       {u"settings", {ProviderType::kOsSettings}},
       {u"personalization", {ProviderType::kPersonalization}},
       {u"drive", {ProviderType::kDriveSearch}},
       {u"file", {ProviderType::kDriveSearch, ProviderType::kFileSearch}},
       {u"app",
        {ProviderType::kInstalledApp, ProviderType::kArcAppShortcut,
         ProviderType::kPlayStoreApp}},
       {u"android",
        {ProviderType::kArcAppShortcut, ProviderType::kPlayStoreApp}},
       {u"game", {ProviderType::kGames}},
       {u"gaming", {ProviderType::kGames}},
       {u"google", {ProviderType::kOmnibox}},
       {u"web", {ProviderType::kOmnibox}},
       {u"search", {ProviderType::kOmnibox}}});

  return keyword_map;
}

// Used for fuzzy string matching, calculates and returns the relevance score
// of a query relative to a keyword.
// Ranges from [0, 1] with 0 representing no match and 1 representing best
// match.
double CalculateRelevance(const std::u16string& query_token,
                          const std::u16string& canonical_keyword) {
  FuzzyTokenizedStringMatch match;
  return match.Relevance(TokenizedString(query_token),
                         TokenizedString(canonical_keyword), kUseWeightedRatio);
}

}  // namespace

KeywordExtractedInfoList ExtractKeywords(const std::u16string& query) {
  // Given the user query, process into a tokenized string and
  // check if keyword exists as one of the tokens.

  TokenizedString tokenized_string(query, TokenizedString::Mode::kWords);
  KeywordToProvidersMap keyword_map = MakeMap();
  KeywordExtractedInfoList extracted_keywords_to_providers = {};

  // TODO(b/262623111): We will need to revisit the O(n^2) implementation if the
  // keyword dictionary grows significantly.
  // Current implementation is iterating through map for each token and using
  // pair-wise comparison to calculate relevancy score which may need to be
  // optimised.
  for (const std::u16string& token : tokenized_string.tokens()) {
    for (KeywordToProvidersPair& keyword_to_providers_pair : keyword_map) {
      const double relevance =
          CalculateRelevance(token, keyword_to_providers_pair.first);
      if (relevance > kRelevancyScoreThreshold) {
        // TODO(b/262623111): Check if relevance threshold is suitable.
        extracted_keywords_to_providers.emplace_back(
            token, relevance, keyword_to_providers_pair.second);
      }
    }
  }

  return extracted_keywords_to_providers;
}

// Strips the keyword from the query
const std::u16string StripQuery(const std::u16string query) {
  KeywordExtractedInfoList extracted_keywords = ExtractKeywords(query);
  std::u16string stripped_query = query;

  for (const KeywordInfo& keyword_info : extracted_keywords) {
    const std::u16string& keyword = keyword_info.query_token;
    auto iter_start = stripped_query.find(keyword);

    if (iter_start != std::string::npos) {
      stripped_query = stripped_query.erase(iter_start, keyword.length() + 1);
    }
  }

  return stripped_query;
}

}  // namespace app_list
