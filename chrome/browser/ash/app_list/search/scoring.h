// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SCORING_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SCORING_H_

#include <ostream>

namespace app_list {

// All score information for a single result. This is stored with a result, and
// is used as 'scratch space' for ranking calculations to pass information
// between rankers. Generally, each member is controlled by one ranker.
struct Scoring {
  // = Members used to compute the display score of a result ===================
  bool filter = false;
  double normalized_relevance = 0.0;
  double mrfu_result_score = 0.0;
  double ftrl_result_score = 0.0;
  // TODO(b/259607603) remove 'override_filter_for_test'. This field is used
  // to temporarily disable filtering for a specific result. This is needed
  // due to a race condition with the test beginning before the
  // RemovedResultsRanker is initialized.
  bool override_filter_for_test = false;

  // Used only for results in the Continue section. Continue results are first
  // ordered by |continue_rank|, and then by their display score. -1 indicates
  // this is unset.
  int continue_rank = -1;

  // = Members used for sorting in SearchController ============================
  // The rank (0, 1, 2, ...) of this result within
  // the Best Match collection of results, or -1 if this result is not a Best
  // Match.
  int best_match_rank = -1;
  // A counter for the burn-in iteration number, where 0 signifies the
  // pre-burn-in state, and 1 and above signify the post-burn-in state.
  // Incremented during the post-burn-in period each time a provider
  // returns. Not applicable to zero-state search.
  int burnin_iteration = 0;

  Scoring() = default;

  Scoring(const Scoring&) = delete;
  Scoring& operator=(const Scoring&) = delete;

  // Score used for ranking within a non-best match category.
  double FinalScore() const;

  // Score used to determine if a result should be considered a best match.
  double BestMatchScore() const;
};

::std::ostream& operator<<(::std::ostream& os, const Scoring& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SCORING_H_
