// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/scoring.h"

#include <algorithm>
#include <cmath>

#include "base/metrics/histogram_macros.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/ash/app_list/search/ranking/constants.h"
#include "chrome/browser/ash/app_list/search/search_features.h"

namespace app_list {

namespace {

// The expression of tanh ensures that the scores stay below 1
// regardless of the constants and controls how quickly scores asymptote to 1.
// The constant 2.65 is chosen so that the score of 1 will be converted to 0.99.
constexpr double kKeywordScale = 2.65;

}  // namespace

double Scoring::FinalScore() const {
  if (filtered_ && !override_filter_for_test_) {
    return -1.0;
  }

  // Keyword Ranker's Train() depends on the calculation of Final Score.
  if (search_features::IsLauncherKeywordExtractionScoringEnabled()) {
    double ftrl_score_after_ranking =
        tanh(kKeywordScale * ftrl_result_score_ * keyword_multiplier_);
    return ftrl_score_after_ranking;
  }
  return ftrl_result_score_;
}

double Scoring::BestMatchScore() const {
  if (filtered_) {
    return -1.0;
  } else {
    double score = std::max(mrfu_result_score_, normalized_relevance_);
    if (search_features::IsLauncherKeywordExtractionScoringEnabled()) {
      return tanh(kKeywordScale * score * keyword_multiplier_);
    }
    return score;
  }
}

::std::ostream& operator<<(::std::ostream& os, const Scoring& scoring) {
  if (scoring.filtered()) {
    return os << "{" << scoring.FinalScore() << " | filtered}";
  }

  return os << base::StringPrintf(
             "{%.2f | nr:%.2f rs:%.2f bm:%d cr:%d bi:%d}", scoring.FinalScore(),
             scoring.normalized_relevance(), scoring.ftrl_result_score(),
             scoring.best_match_rank(), scoring.continue_rank(),
             scoring.burn_in_iteration());
}

void Scoring::set_filtered(bool filtered) {
  filtered_ = filtered;
}

void Scoring::set_normalized_relevance(double normalized_relevance) {
  normalized_relevance_ = normalized_relevance;
}

void Scoring::set_mrfu_result_score(double mrfu_result_score) {
  mrfu_result_score_ = mrfu_result_score;
}

void Scoring::set_ftrl_result_score(double ftrl_result_score) {
  ftrl_result_score_ = ftrl_result_score;
}

void Scoring::set_keyword_multiplier(double keyword_multiplier) {
  keyword_multiplier_ = keyword_multiplier;
}

void Scoring::set_continue_rank(int continue_rank) {
  continue_rank_ = continue_rank;
}

void Scoring::set_best_match_rank(int best_match_rank) {
  best_match_rank_ = best_match_rank;
}

void Scoring::set_burn_in_iteration(int burn_in_iteration) {
  burn_in_iteration_ = burn_in_iteration;
}

void Scoring::override_filter_for_test(bool override) {
  override_filter_for_test_ = override;
}

}  // namespace app_list
