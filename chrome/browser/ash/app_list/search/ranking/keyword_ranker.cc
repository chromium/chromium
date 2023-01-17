// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/scoring.h"

namespace app_list {

KeywordRanker::KeywordRanker() = default;

KeywordRanker::~KeywordRanker() = default;

void KeywordRanker::Start(const std::u16string& query,
                          ResultsMap& results,
                          CategoriesList& categories) {
  // When the user start input, this function will be called.
  last_query_ = query;

  // Stores the providers that match with the keyword within the input query.

  // TODO(b/262623111): Type which ExtractKeyword() will return is a struct
  // containing {keyword string, score, Search Providers}
  KeywordToProvidersPairs extracted_keywords_to_providers =
      ExtractKeyword(last_query_);

  if (!extracted_keywords_to_providers.empty()) {
    // TODO(b/262623111): To iterate through the extracted keywords to providers
    // to place the Search Providers into the vector matched_providers_
    matched_providers_ = extracted_keywords_to_providers[0].second;
  }
}

void KeywordRanker::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  // Return if the given provider does not matched a keyword in the query
  // as this does not require modification of results.
  if (std::find(matched_providers_.begin(), matched_providers_.end(),
                provider) == matched_providers_.end()) {
    return;
  }

  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  for (auto& result : it->second) {
    result->scoring().set_keyword_multiplier(1.2);
  }
}

}  // namespace app_list
