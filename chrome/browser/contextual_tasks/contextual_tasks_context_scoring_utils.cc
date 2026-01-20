// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"

#include <cmath>

namespace contextual_tasks {

namespace {

// Probabilistic OR - any high score leads to high score.
float ProbOr(const float score1, const float score2) {
  return 1.0f - (1.0f - score1) * (1.0f - score2);
}

}  // namespace

// TODO: crbug.com/452036470 - Add a proper scoring function based on analysis.
double GetTabScore(const TabSignals& signals) {
  double score = 0;
  if (signals.embedding_score.has_value()) {
    score = ProbOr(score, *(signals.embedding_score));
  }
  if (signals.duration_since_last_active.has_value()) {
    score = ProbOr(
        score,
        std::pow(0.7, signals.duration_since_last_active->InSeconds() / 180));
  }
  if (signals.num_query_title_matching_words.has_value()) {
    // Monotonically increasing; Always < 1.
    // 0 matches = 0 score; 1 match = 0.57; 2 matches = 0.81 and so on.
    float lexical_match_score =
        1.0f - std::exp(-0.85 * *(signals.num_query_title_matching_words));
    score = ProbOr(score, lexical_match_score);
  }
  return score;
}

}  // namespace contextual_tasks
