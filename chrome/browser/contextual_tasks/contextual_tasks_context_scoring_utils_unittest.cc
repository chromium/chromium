// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/contextual_tasks/contextual_tasks_context_scoring_utils.h"

#include "base/time/time.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace contextual_tasks {

TEST(GetScoreWithAllSignalsTest, EmptySignals) {
  TabSignals signals;
  EXPECT_DOUBLE_EQ(GetScoreWithAllSignals(signals), 0.0);
}

TEST(GetScoreWithAllSignalsTest, OnlyEmbeddingSignalAvailable) {
  TabSignals signals;
  signals.embedding_score = 0.5;
  EXPECT_DOUBLE_EQ(GetScoreWithStaticSignals(signals), 0.5);
  EXPECT_DOUBLE_EQ(GetScoreWithAllSignals(signals), 0.5);
}

TEST(GetScoreWithAllSignalsTest, EngagementSignals_MissingRequired) {
  TabSignals signals;
  signals.duration_since_last_active = base::Seconds(0);
  EXPECT_DOUBLE_EQ(GetScoreWithAllSignals(signals), 0.0);

  signals.duration_since_last_active.reset();
  signals.duration_of_last_visit = base::Seconds(100);
  EXPECT_DOUBLE_EQ(GetScoreWithAllSignals(signals), 0.0);
}

TEST(GetScoreWithAllSignalsTest, OnlyEngagementSignalsAvailable) {
  TabSignals signals;
  signals.duration_since_last_active = base::Seconds(0);
  // High duration score ~1.0
  signals.duration_of_last_visit = base::Seconds(100);
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 1.0, 0.001);

  signals.duration_since_last_active = base::Seconds(180);
  // Recency: 0.7^(180/180) = 0.7. Duration: ~1.0.
  // HarmonicMean(0.7, 1.0) = 1.4 / 1.7 = 0.8235...
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 0.8235, 0.001);

  signals.duration_of_last_visit = base::Seconds(5);
  // Recency: 0.7.
  // Duration: 1 - 0.3^(5/5) = 0.7.
  // HarmonicMean(0.7, 0.7) = 0.7.
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 0.7, 0.001);
}

TEST(GetScoreWithAllSignalsTest, OnlyLexicalMatchScoreAvailable) {
  TabSignals signals;
  signals.num_query_title_matching_words = 0;
  // 1 - exp(0) = 0
  EXPECT_DOUBLE_EQ(GetScoreWithAllSignals(signals), 0.0);

  signals.num_query_title_matching_words = 1;
  // 1 - exp(-0.85) = 1 - 0.4274... = 0.5725...
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 0.572, 0.001);
}

TEST(GetScoreWithAllSignalsTest, CombinedScores) {
  TabSignals signals;
  signals.embedding_score = 0.5;
  signals.duration_since_last_active = base::Seconds(180);
  signals.duration_of_last_visit = base::Seconds(5);

  // Recency: 0.7. Duration: 0.7. Behavioral: 0.7.
  // Static: 0.5.
  // Total: ProbOr(0.5, 0.7) = 1 - (0.5 * 0.3) = 0.85.
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 0.85, 0.001);
}

TEST(GetScoreWithAllSignalsTest, RecencyScoreStepFunction) {
  TabSignals signals;
  // Duration score ~1.0
  signals.duration_of_last_visit = base::Seconds(100);

  signals.duration_since_last_active = base::Seconds(179);
  // 179 / 180 = 0. pow(0.7, 0) = 1.
  // HarmonicMean(1, 1) = 1.
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 1.0, 0.001);

  signals.duration_since_last_active = base::Seconds(359);
  // 359 / 180 = 1. pow(0.7, 1) = 0.7.
  // HarmonicMean(0.7, 1) = 0.8235
  EXPECT_NEAR(GetScoreWithAllSignals(signals), 0.8235, 0.001);
}

TEST(GetScoreWithStaticSignalsTest, CombinedScores) {
  TabSignals signals;
  signals.embedding_score = 0.5;
  signals.duration_since_last_active = base::Seconds(180);
  signals.duration_of_last_visit = base::Seconds(5);

  // Get score based on only Static Signals: 0.5
  EXPECT_NEAR(GetScoreWithStaticSignals(signals), 0.5, 0.001);
}

}  // namespace contextual_tasks
