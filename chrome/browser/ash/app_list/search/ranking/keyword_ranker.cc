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

  // Type which ExtractKeyword() will return is a struct
  // containing {keyword string, score, Search Providers}
  KeywordExtractedInfoList extracted_keywords_to_providers =
      ExtractKeywords(last_query_);

  if (!extracted_keywords_to_providers.empty()) {
    SetProviderMap(extracted_keywords_to_providers);
  }
}

void KeywordRanker::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  // Return if the given provider does not matched a keyword in the query
  // as this does not require modification of results.
  if (matched_provider_score_.find(provider) == matched_provider_score_.end()) {
    return;
  }

  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  // The relevance score of how accurate the input query token matched the
  // provider's keyword is ranged from 0.75 to 1, hence the provider_boost_score
  // will be ranged from 1.375 to 1.5.
  double provider_boost_score = matched_provider_score_[provider] * 0.5 + 1;

  for (auto& result : it->second) {
    result->scoring().set_keyword_multiplier(provider_boost_score);
  }
}

void KeywordRanker::SetProviderMap(
    KeywordExtractedInfoList extracted_keywords_to_providers) {
  for (KeywordInfo& keyword_info : extracted_keywords_to_providers) {
    for (ProviderType provider : keyword_info.search_providers) {
      if (matched_provider_score_.find(provider) ==
              matched_provider_score_.end() ||
          keyword_info.relevance_score > matched_provider_score_[provider]) {
        matched_provider_score_[provider] = keyword_info.relevance_score;
      }
    }
  }
}

}  // namespace app_list
