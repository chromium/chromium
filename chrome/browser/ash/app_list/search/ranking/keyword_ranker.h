// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_

#include <vector>

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"
#include "chrome/browser/ash/app_list/search/types.h"

namespace app_list {

// A ranker that boost the scores of results that contains
// certain keywords.
class KeywordRanker : public Ranker {
 public:
  KeywordRanker();
  ~KeywordRanker() override;

  KeywordRanker(const KeywordRanker&) = delete;
  KeywordRanker& operator=(const KeywordRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             ResultsMap& results,
             CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;

 private:
  std::u16string last_query_;
  std::vector<ProviderType> matched_providers_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_KEYWORD_RANKER_H_
