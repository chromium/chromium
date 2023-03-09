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
class Scoring {
 public:
  Scoring() = default;
  Scoring(const Scoring&) = delete;
  Scoring& operator=(const Scoring&) = delete;

  // Score used for ranking within a non-best match category.
  double FinalScore() const;

  // Score used to determine if a result should be considered a best match.
  double BestMatchScore() const;

  // There are many components that filter out results. Be careful with
  // set_filtered(false) as it may override a filter from a different location.
  // TODO(b/268281615): Consider removing the ability to set_filtered(false).
  void set_filtered(bool filtered);
  bool filtered() const { return filtered_; }

  void set_normalized_relevance(double normalized_relevance);
  double normalized_relevance() const { return normalized_relevance_; }

  void set_mrfu_result_score(double mrfu_result_score);
  double mrfu_result_score() const { return mrfu_result_score_; }

  void set_ftrl_result_score(double ftrl_result_score);
  double ftrl_result_score() const { return ftrl_result_score_; }

  void set_keyword_multiplier(double keyword_multiplier);
  double keyword_multiplier() const { return keyword_multiplier_; }

  void set_continue_rank(int continue_rank);
  int continue_rank() const { return continue_rank_; }

  void set_best_match_rank(int best_match_rank);
  int best_match_rank() const { return best_match_rank_; }

  void set_burn_in_iteration(int burn_in_iteration);
  int burn_in_iteration() const { return burn_in_iteration_; }

  void override_filter_for_test(bool override);

 private:
  // = Members used to compute the display score of a result ===================
  bool filtered_ = false;
  double normalized_relevance_ = 0.0;
  double mrfu_result_score_ = 0.0;
  double ftrl_result_score_ = 0.0;
  // TODO(b/259607603) remove 'override_filter_for_test'. This field is used
  // to temporarily disable filtering for a specific result. This is needed
  // due to a race condition with the test beginning before the
  // RemovedResultsRanker is initialized.
  bool override_filter_for_test_ = false;
  double keyword_multiplier_ = 1.0;

  // Used only for results in the Continue section. Continue results are first
  // ordered by |continue_rank|, and then by their display score. -1 indicates
  // this is unset.
  int continue_rank_ = -1;

  // = Members used for sorting in SearchController ============================
  // The rank (0, 1, 2, ...) of this result within
  // the Best Match collection of results, or -1 if this result is not a Best
  // Match.
  int best_match_rank_ = -1;
  // A counter for the burn-in iteration number, where 0 signifies the
  // pre-burn-in state, and 1 and above signify the post-burn-in state.
  // Incremented during the post-burn-in period each time a provider
  // returns. Not applicable to zero-state search.
  int burn_in_iteration_ = 0;
};

::std::ostream& operator<<(::std::ostream& os, const Scoring& result);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SCORING_H_
