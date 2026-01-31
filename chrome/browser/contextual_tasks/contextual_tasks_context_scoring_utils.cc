// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"

#include <cmath>
#include <optional>

namespace contextual_tasks {

namespace {

// Probabilistic OR - any high score leads to high score.
float ProbOr(const float score1, const float score2) {
  return 1.0f - (1.0f - score1) * (1.0f - score2);
}

// HarmonicMean - any low score leads to low score.
float HarmonicMean(const float score1, const float score2) {
  float denominator = (score1 + score2);
  if (denominator == 0.0f) {
    return 0.0f;
  }
  return (2.0f * score1 * score2) / denominator;
}

// Consider both semantic match and lexical match. If either of them is high,
// overall score should be high.
std::optional<double> ComputeContentScore(const TabSignals& signals) {
  std::optional<double> score;
  if (signals.embedding_score.has_value()) {
    score = ProbOr(score.value_or(0.0f), *(signals.embedding_score));
  }
  if (signals.num_query_title_matching_words.has_value()) {
    // Monotonically increasing; Always < 1.
    // 0 matches = 0 score; 1 match = 0.57; 2 matches = 0.81 and so on.
    float lexical_match_score =
        1.0f - std::exp(-0.85 * *(signals.num_query_title_matching_words));
    score = ProbOr(score.value_or(0.0f), lexical_match_score);
  }
  return score;
}

// Consider recency as a positive signal, only when duration spent on the tab
// was significant (>=5 seconds).
std::optional<double> ComputeEngagementScore(const TabSignals& signals) {
  if (!signals.duration_since_last_active.has_value()) {
    return std::nullopt;
  }
  if (!signals.duration_of_last_visit.has_value()) {
    return std::nullopt;
  }

  double recency_score =
      std::pow(0.7, signals.duration_since_last_active->InSeconds() / 180);
  double duration_score =
      1.0 - std::pow(0.3, signals.duration_of_last_visit->InSeconds() / 5.0);
  return HarmonicMean(recency_score, duration_score);
}

}  // namespace

double GetScoreWithStaticSignals(const TabSignals& signals) {
  return ComputeContentScore(signals).value_or(0.0f);
}

// TODO: crbug.com/452036470 - Update this scoring function with one obtained
// after analysis/training on logs, or get rid of engagement signals.
double GetScoreWithAllSignals(const TabSignals& signals) {
  std::optional<double> score_based_on_static_signals =
      ComputeContentScore(signals);
  std::optional<double> score_based_on_engagement_signals =
      ComputeEngagementScore(signals);

  double score = 0.0f;
  if (score_based_on_static_signals.has_value()) {
    score = ProbOr(score, *(score_based_on_static_signals));
  }
  if (score_based_on_engagement_signals.has_value()) {
    score = ProbOr(score, *(score_based_on_engagement_signals));
  }
  return score;
}

}  // namespace contextual_tasks
