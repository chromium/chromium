// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/common/keyword_util.h"

namespace app_list {

KeywordRanker::KeywordRanker()
    : keyword_cache_(std::make_unique<KeywordCache>()) {}

KeywordRanker::~KeywordRanker() = default;

void KeywordRanker::Start(const std::u16string& query,
                          const CategoriesList& categories) {
  // When the user start input, this function will be called.
  last_query_ = query;
  matched_provider_score_.clear();
  keyword_cache_->Clear();

  // Type which ExtractKeyword() will return is a struct containing {keyword
  // string, score, Search Providers}.
  std::vector<KeywordInfo> extracted_keywords_to_providers =
      ExtractKeywords(last_query_);

  if (!extracted_keywords_to_providers.empty()) {
    StoreMaxProviderScores(extracted_keywords_to_providers);
  }
}

void KeywordRanker::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end()) {
    return;
  }

  // If the provider does not have matched keyword within the input query,
  // store the scores and return.
  if (matched_provider_score_.find(provider) == matched_provider_score_.end()) {
    keyword_cache_->RecordNonMatch(results, provider);
    return;
  }

  // If the provider have matched keyword within the input query, set all
  // results a different keyword_multiplier and stored the results.
  double boost_factor = keyword_cache_->boost_factor();
  double keyword_multiplier =
      matched_provider_score_[provider] * boost_factor + 1;
  keyword_cache_->ScoreAndRecordMatch(results, provider, keyword_multiplier);
}

void KeywordRanker::Train(const LaunchData& launch) {
  keyword_cache_->Train(launch.id);
}

void KeywordRanker::StoreMaxProviderScores(
    const std::vector<KeywordInfo>& extracted_keywords_to_providers) {
  for (const KeywordInfo& keyword_info : extracted_keywords_to_providers) {
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
