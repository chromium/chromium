// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(GetTabScoreTest, EmptySignals) {
  TabSignals signals;
  EXPECT_DOUBLE_EQ(GetTabScore(signals), 0.0);
}

TEST(GetTabScoreTest, OnlyEmbeddingScore) {
  TabSignals signals;
  signals.embedding_score = 0.5;
  EXPECT_DOUBLE_EQ(GetTabScore(signals), 0.5);
}

TEST(GetTabScoreTest, OnlyRecencyScore) {
  TabSignals signals;
  signals.duration_since_last_active = base::Seconds(0);
  // ProbOr(0, 0.7^(0/180)) = ProbOr(0, 1) = 1.0
  EXPECT_NEAR(GetTabScore(signals), 1.0, 0.001);

  signals.duration_since_last_active = base::Seconds(180);
  // ProbOr(0, 0.7^(180/180)) = ProbOr(0, 0.7) = 0.7
  EXPECT_NEAR(GetTabScore(signals), 0.7, 0.001);
}

TEST(GetTabScoreTest, OnlyLexicalMatchScore) {
  TabSignals signals;
  signals.num_query_title_matching_words = 0;
  // 1 - exp(0) = 0
  EXPECT_DOUBLE_EQ(GetTabScore(signals), 0.0);

  signals.num_query_title_matching_words = 1;
  // 1 - exp(-0.85) = 1 - 0.4274... = 0.5725...
  EXPECT_NEAR(GetTabScore(signals), 0.572, 0.001);
}

TEST(GetTabScoreTest, CombinedScores) {
  TabSignals signals;
  signals.embedding_score = 0.5;
  signals.duration_since_last_active = base::Seconds(180);  // score 0.7

  // ProbOr(0.5, 0.7) = 1 - (0.5 * 0.3) = 1 - 0.15 = 0.85
  EXPECT_NEAR(GetTabScore(signals), 0.85, 0.001);
}

TEST(GetTabScoreTest, RecencyScoreStepFunction) {
  TabSignals signals;
  signals.duration_since_last_active = base::Seconds(179);
  // 179 / 180 = 0. pow(0.7, 0) = 1.
  EXPECT_DOUBLE_EQ(GetTabScore(signals), 1.0);

  signals.duration_since_last_active = base::Seconds(359);
  // 359 / 180 = 1. pow(0.7, 1) = 0.7.
  EXPECT_NEAR(GetTabScore(signals), 0.7, 0.001);
}

}  // namespace contextual_tasks
