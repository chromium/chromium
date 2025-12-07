// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_BEST_MATCH_RANKER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_BEST_MATCH_RANKER_H_

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"

namespace app_list {

class BestMatchRankerTest;

namespace test {
class BestMatchRankerTest;
}

// A ranker that inspects the scores of incoming results and picks out some of
// them to become 'best matches' to be moved to the top of the results list.
//
// Best Match candidates are identified by thresholding the scores of incoming
// results. It is assumed that this runs after scores have been transformed to a
// near-uniform distribution per-provider by the ScoreNormalizingRanker, and so
// a score of 0.95 indicates a 95th percentile result from a provider, for
// example. It is also assumed that this runs before the category ranker, which
// moves the scores out of the normalized range [0,1].
//
// In the pre-burn-in period, Best Match candidates are simply ranked by
// normalized relevance score. This is the same in the post-burn-n period, with
// the addition that the highest-ranked kNumBestMatches results are stabilized
// in their positions, and cannot be displaced by later arriving best matches.
class BestMatchRanker : public Ranker {
 public:
  BestMatchRanker();
  ~BestMatchRanker() override;

  BestMatchRanker(const BestMatchRanker&) = delete;
  BestMatchRanker& operator=(const BestMatchRanker&) = delete;

  // Ranker:
  void Start(const std::u16string& query,
             const CategoriesList& categories) override;
  void UpdateResultRanks(ResultsMap& results, ProviderType provider) override;
  void OnBurnInPeriodElapsed() override;

 private:
  friend class test::BestMatchRankerTest;

  // Whether the overall search session is within the pre-burn-in period.
  // Is unset via a callback from SearchControllerImplNew.
  bool is_pre_burnin_ = true;

  // Vector to track best matches throughout a given search query.
  std::vector<base::WeakPtr<ChromeSearchResult>> best_matches_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_RANKING_BEST_MATCH_RANKER_H_
