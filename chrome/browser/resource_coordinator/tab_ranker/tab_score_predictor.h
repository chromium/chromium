// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_SCORE_PREDICTOR_H_
#define CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_SCORE_PREDICTOR_H_

#include <map>
#include <memory>

#include "base/compiler_specific.h"
#include "base/macros.h"
#include "base/optional.h"

namespace assist_ranker {
class ExamplePreprocessorConfig;
class RankerExample;
}  // namespace assist_ranker

namespace tab_ranker {

namespace tfnative_model {
struct FixedAllocations;
}  // namespace tfnative_model

namespace pairwise_model {
struct FixedAllocations;
}  // namespace pairwise_model

struct TabFeatures;

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
enum class TabRankerResult {
  kSuccess = 0,
  kPreprocessorInitializationFailed = 1,
  kPreprocessorOtherError = 2,
  kUnrecognizableScorer = 3,
  kMaxValue = kUnrecognizableScorer
};

// Makes predictions using the tab reactivation DNN classifier. Background tabs
// are scored based on how likely they are to be reactivated.
class TabScorePredictor {
 public:
  enum ScorerType {
    kMRUScorer = 0,
    kMLScorer = 1,
    kPairwiseScorer = 2,
    kFrecencyScorer = 3,
    kMaxValue = kFrecencyScorer
  };
  TabScorePredictor();
  ~TabScorePredictor();

  // Scores the tab using the tab reactivation model. A higher score indicates
  // the tab is more likely to be reactivated than a lower score. A lower score
  // indicates the tab is more likely to be closed.
  TabRankerResult ScoreTab(const TabFeatures& tab,
                           float* score) WARN_UNUSED_RESULT;

  // Scores multiple tabs.
  // Input is a map from an id (lifecycle_unit id) to the TabFeatures of that
  // tab.
  // Returns a map from an id to its predicted reactivation score.
  // If the scoring fails at any step, it will set
  // std::numeric_limits<float>::max() as the reactivation score for that tab.
  std::map<int32_t, float> ScoreTabs(
      const std::map<int32_t, base::Optional<TabFeatures>>& tabs);

 private:
  friend class ScoreTabsWithPairwiseScorerTest;

  // Loads the preprocessor config if not already loaded.
  void LazyInitialize();

  // Calculates reactivation score of a single tab with mru feature.
  TabRankerResult ScoreTabWithMRUScorer(const TabFeatures& tab, float* score);
  // Calculates reactivation score of a single tab with ml model.
  TabRankerResult ScoreTabWithMLScorer(const TabFeatures& tab, float* score);
  // Preprocess and inferences on the |example|.
  TabRankerResult PredictWithPreprocess(assist_ranker::RankerExample* example,
                                        float* score);
  // Calculates the relative reaction score between tab1 and tab2.
  // For pairwise model, the ml model is applied to the pair(tab1, tab2).
  // For non-pairwise model, the score is the difference of reactivation
  // scores on these two tabs.
  TabRankerResult ScoreTabsPairs(const TabFeatures& tab1,
                                 const TabFeatures& tab2,
                                 float* score);
  std::map<int32_t, float> ScoreTabsWithPairwiseScorer(
      const std::map<int32_t, base::Optional<TabFeatures>>& tabs);
  TabRankerResult ScoreTabWithFrecencyScorer(const TabFeatures& tab,
                                             float* score);

  std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
      preprocessor_config_;

  // Fixed-size working memory provided to the inferencing function. Lazy
  // initialized once so it isn't reallocated for every inference.
  std::unique_ptr<tfnative_model::FixedAllocations> tfnative_alloc_;
  std::unique_ptr<pairwise_model::FixedAllocations> pairwise_alloc_;

  const float discard_count_penalty_ = 0.0f;
  const float mru_scorer_penalty_ = 1.0f;
  const ScorerType type_ = kMLScorer;

  DISALLOW_COPY_AND_ASSIGN(TabScorePredictor);
};

}  // namespace tab_ranker

#endif  // CHROME_BROWSER_RESOURCE_COORDINATOR_TAB_RANKER_TAB_SCORE_PREDICTOR_H_
