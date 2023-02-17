// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_

#include <map>
#include <string>
#include <vector>

#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// Struct containing information on the identified extracted token from
// user-typed query. This includes 3 members as detailed below.
struct KeywordInfo {
  //  1. The extracted token from query.
  std::u16string query_token;
  //  2. Through fuzzy string matching, the relevance score between the query
  //  token and the best matched canonical keyword.
  double relevance_score;
  //  3. Vector of the associated Search Providers to the matched keyword.
  std::vector<ProviderType> search_providers;

  KeywordInfo(const std::u16string& query_token,
              double relevance_score,
              const std::vector<ProviderType>& search_providers);

  ~KeywordInfo();
  KeywordInfo(const KeywordInfo& other);

  bool operator==(const KeywordInfo& other) const {
    return (query_token == other.query_token) &&
           (relevance_score == other.relevance_score) &&
           (search_providers == other.search_providers);
  }
};

using KeywordToProvidersMap =
    base::flat_map<std::u16string, std::vector<ProviderType>>;

using KeywordToProvidersPair =
    std::pair<std::u16string, std::vector<ProviderType>>;

using KeywordExtractedInfoList = std::vector<KeywordInfo>;

// Provided the list of tokens produced from the user query, returns a list of
// keywords and its associated SearchProviders.
//   - A given keyword can be associated with 1 or more SearchProviders.
//   - Multiple keywords may map to the same SearchProvider.
KeywordExtractedInfoList ExtractKeywords(const std::u16string& query);

// Strips the user query from the keyword.
const std::u16string StripQuery(const std::u16string query);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_COMMON_KEYWORD_UTIL_H_
