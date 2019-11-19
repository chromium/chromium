// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>

#include "base/rand_util.h"
#include "base/test/scoped_feature_list.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features_test_helper.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace tab_ranker {
namespace {

// This tests that the TabRanker predictor returns a same score that is
// calcuated when the model is trained.
class TabScorePredictorTest : public testing::Test {
 public:
  TabScorePredictorTest() = default;
  ~TabScorePredictorTest() override = default;

 protected:
  // Returns a prediction for the tab example.
  float ScoreTab(const TabFeatures& tab) {
    float score = 0;
    EXPECT_EQ(TabRankerResult::kSuccess,
              TabScorePredictor().ScoreTab(tab, &score));
    return score;
  }

  base::test::ScopedFeatureList scoped_feature_list_;

 private:
  DISALLOW_COPY_AND_ASSIGN(TabScorePredictorTest);
};

}  // namespace

// Checks the score for an example that we have calculated a known score for
// outside of Chrome.
TEST_F(TabScorePredictorTest, KnownScore) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "1"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), -10.076081);
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeatures) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "1"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetPartialTabFeaturesForTesting()), 5.1401806);
}

// Checks the score for an example that we have calculated a known score for
// outside of Chrome.
TEST_F(TabScorePredictorTest, KnownScorePairwise) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "2"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), -3.8852997);
}

// Checks the score for a different example that we have calculated a known
// score for outside of Chrome. This example omits the optional features.
TEST_F(TabScorePredictorTest, KnownScoreMissingOptionalFeaturesPairwise) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "2"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetPartialTabFeaturesForTesting()), 1.9675488);
}

TEST_F(TabScorePredictorTest, ScoreWithMRUScorer) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker,
      {{"scorer_type", "0"}, {"mru_scorer_penalty", "0.2345"}});
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(GetFullTabFeaturesForTesting()), 0.13639774);
}

TEST_F(TabScorePredictorTest, ScoreWithDiscardPenalty) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "0"},
                             {"mru_scorer_penalty", "0.2345"},
                             {"discard_count_penalty", "0.2468"}});
  // Pre-calculated score using the generated model outside of Chrome.
  auto tab = GetFullTabFeaturesForTesting();
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.13639774);

  tab.discard_count = 3;
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.28599524);
}

TEST_F(TabScorePredictorTest, ScoreTabWithFrecencyScorer) {
  scoped_feature_list_.InitAndEnableFeatureWithParameters(
      features::kTabRanker,
      {{"scorer_type", "3"}, {"discard_count_penalty", "0.2468"}});

  auto tab = GetFullTabFeaturesForTesting();
  // Pre-calculated score using the generated model outside of Chrome.
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.1234f);

  tab.discard_count = 3;
  EXPECT_FLOAT_EQ(ScoreTab(tab), 0.25874191);
}

class ScoreTabsWithPairwiseScorerTest : public testing::Test {
 protected:
  std::map<int32_t, float> ScoreTabsWithPairwiseScorer(
      const std::map<int32_t, base::Optional<TabFeatures>>& tabs) {
    return TabScorePredictor().ScoreTabsWithPairwiseScorer(tabs);
  }
};

TEST_F(ScoreTabsWithPairwiseScorerTest, EmptyTabFeaturesFirst) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "3"}});

  for (int length = 1; length < 30; ++length) {
    // Generates random order of ids.
    std::vector<int32_t> ids;
    for (int i = 0; i < length; ++i) {
      ids.push_back(i + 100);
    }
    base::RandomShuffle(ids.begin(), ids.end());

    std::map<int32_t, base::Optional<TabFeatures>> tabs;
    for (int i = 0; i < length; ++i) {
      TabFeatures tab;
      tab.mru_index = base::RandInt(0, 3000);
      // Set the frecency score in reverse order.
      tab.frecency_score = -i * 5;

      // Set half of the TabFeatures to be null.
      if (i < length / 2) {
        tabs[ids[i]] = base::nullopt;
      } else {
        tabs[ids[i]] = tab;
      }
    }

    const std::map<int32_t, float> scores = ScoreTabsWithPairwiseScorer(tabs);
    for (int i = 0; i < length; ++i) {
      if (i < length / 2) {
        // First half should be all null which have scores > (length+1) / 2.0f;
        EXPECT_GT(scores.at(ids[i]), (length + 1) / 2.0f);
      } else {
        // The second half should be non-empty tab features with descending
        // scores.
        EXPECT_FLOAT_EQ(scores.at(ids[i]), length - i);
      }
    }
  }
}

TEST_F(ScoreTabsWithPairwiseScorerTest, SortByScore) {
  base::test::ScopedFeatureList scoped_feature_list;
  scoped_feature_list.InitAndEnableFeatureWithParameters(
      features::kTabRanker, {{"scorer_type", "3"}});

  // Test all cases with length from 1 to 30.
  for (int length = 1; length < 30; ++length) {
    // Generates random order of ids.
    std::vector<int32_t> ids;
    for (int i = 0; i < length; ++i) {
      ids.push_back(i);
    }
    base::RandomShuffle(ids.begin(), ids.end());

    // set ids[i] to have frecency_score i*5;
    std::map<int32_t, base::Optional<TabFeatures>> tabs;
    for (int i = 0; i < length; ++i) {
      TabFeatures tab;
      tab.mru_index = base::RandInt(0, 3000);
      tab.frecency_score = i * 5;
      tabs[ids[i]] = tab;
    }

    const std::map<int32_t, float> scores = ScoreTabsWithPairwiseScorer(tabs);

    // Should return the same order as the shuffled result.
    for (int i = 0; i < length; ++i) {
      EXPECT_FLOAT_EQ(scores.at(ids[i]), i + 1);
    }
  }
}

}  // namespace tab_ranker
