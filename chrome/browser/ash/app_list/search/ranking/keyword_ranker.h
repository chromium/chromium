// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_

#include <vector>

#include "chrome/browser/ash/app_list/search/common/keyword_util.h"
#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/search/util/keyword_cache.h"

namespace app_list {

// A ranker that boost the scores of results that contains certain keywords.
class KeywordRanker : public Ranker {
 public:
  using ProviderToScoreMap = std::map<ProviderType, double>;

  KeywordRanker();
  ~KeywordRanker() override;

  KeywordRanker(const KeywordRanker&) = delete;
  KeywordRanker& operator=(const KeywordRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void Train(const LaunchData& launch) override;

 private:
  std::unique_ptr<KeywordCache> keyword_cache_;

  std::u16string last_query_;
  ProviderToScoreMap matched_provider_score_;

  // Extracts each provider and its corresponding best scores into a map from
  // std::vector<KeywordInfo>.
  void StoreMaxProviderScores(
      const std::vector<KeywordInfo>& extracted_keywords_to_providers);
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_
