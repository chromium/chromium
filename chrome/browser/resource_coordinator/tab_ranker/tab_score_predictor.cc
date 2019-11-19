// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/resource_coordinator/tab_ranker/tab_score_predictor.h"

#include <memory>
#include <string>

#include "base/logging.h"
#include "base/memory/ref_counted_memory.h"
#include "base/memory/scoped_refptr.h"
#include "base/strings/strcat.h"
#include "chrome/browser/resource_coordinator/tab_manager_features.h"
#include "chrome/browser/resource_coordinator/tab_ranker/native_inference.h"
#include "chrome/browser/resource_coordinator/tab_ranker/pairwise_inference.h"
#include "chrome/browser/resource_coordinator/tab_ranker/tab_features.h"
#include "chrome/grit/browser_resources.h"
#include "components/assist_ranker/example_preprocessing.h"
#include "components/assist_ranker/proto/example_preprocessor.pb.h"
#include "components/assist_ranker/proto/ranker_example.pb.h"
#include "ui/base/resource/resource_bundle.h"

namespace tab_ranker {
namespace {

using resource_coordinator::GetDiscardCountPenaltyTabRanker;
using resource_coordinator::GetMRUScorerPenaltyTabRanker;
using resource_coordinator::GetScorerTypeForTabRanker;

// Maps the |mru_index| to it's reverse rank in (0.0, 1.0).
// High score means more likely to be reactivated.
// We use inverse rank because we think that the first several |mru_index| is
// more significant than the larger ones.
inline float MruToScore(const float mru_index) {
  DCHECK_GE(mru_index, 0.0f);
  return 1.0f / (1.0f + mru_index);
}

// Maps the |discard_count| to a score in (0.0, 1.0), for which
// High score means more likely to be reactivated.
// We use std::exp because we think that the first several |discard_count| is
// not as significant as the larger ones.
inline float DiscardCountToScore(const float discard_count) {
  return std::exp(discard_count);
}

// Loads the preprocessor config protobuf, which lists each feature, their
// types, bucket configurations, etc.
// Returns nullptr if the protobuf was not successfully populated.
std::unique_ptr<assist_ranker::ExamplePreprocessorConfig>
LoadExamplePreprocessorConfig(const int resource_id) {
  auto config = std::make_unique<assist_ranker::ExamplePreprocessorConfig>();

  scoped_refptr<base::RefCountedMemory> raw_config =
      ui::ResourceBundle::GetSharedInstance().LoadDataResourceBytes(
          resource_id);
  if (!raw_config || !raw_config->front()) {
    LOG(ERROR) << "Failed to load TabRanker example preprocessor config.";
    return nullptr;
  }

  if (!config->ParseFromArray(raw_config->front(), raw_config->size())) {
    LOG(ERROR) << "Failed to parse TabRanker example preprocessor config.";
    return nullptr;
  }

  return config;
}

}  // namespace

TabScorePredictor::TabScorePredictor()
    : discard_count_penalty_(GetDiscardCountPenaltyTabRanker()),
      mru_scorer_penalty_(GetMRUScorerPenaltyTabRanker()),
      type_(static_cast<ScorerType>(GetScorerTypeForTabRanker())) {}

TabScorePredictor::~TabScorePredictor() = default;

TabRankerResult TabScorePredictor::ScoreTab(const TabFeatures& tab,
                                            float* score) {
  DCHECK(score);

  // No error is expected, but something could conceivably be misconfigured.
  TabRankerResult result = TabRankerResult::kSuccess;

  if (type_ == kMRUScorer) {
    result = ScoreTabWithMRUScorer(tab, score);
  } else if (type_ == kMLScorer) {
    result = ScoreTabWithMLScorer(tab, score);
  } else if (type_ == kPairwiseScorer) {
    result = ScoreTabsPairs(tab, TabFeatures(), score);
  } else if (type_ == kFrecencyScorer) {
    result = ScoreTabWithFrecencyScorer(tab, score);
  } else {
    return TabRankerResult::kUnrecognizableScorer;
  }

  // Applies DiscardCount adjustment.
  // The default value of discard_count_penalty_ is 0.0f, which will not change
  // the score.
  // The larger the |discard_count_penalty_| is (set from Finch), the quicker
  // the score increases based on the discard_count.
  *score *= DiscardCountToScore(tab.discard_count * discard_count_penalty_);

  return result;
}

std::map<int32_t, float> TabScorePredictor::ScoreTabs(
    const std::map<int32_t, base::Optional<TabFeatures>>& tabs) {
  if (type_ != kPairwiseScorer) {
    std::map<int32_t, float> reactivation_scores;
    for (const auto& pair : tabs) {
      float score = 0.0f;
      if (pair.second && (ScoreTab(pair.second.value(), &score) ==
                          TabRankerResult::kSuccess)) {
        reactivation_scores[pair.first] = score;
      } else {
        reactivation_scores[pair.first] = std::numeric_limits<float>::max();
      }
    }
    return reactivation_scores;
  } else {
    return ScoreTabsWithPairwiseScorer(tabs);
  }
}

TabRankerResult TabScorePredictor::ScoreTabWithMLScorer(const TabFeatures& tab,
                                                        float* score) {
  // Lazy-load the preprocessor config.
  LazyInitialize();
  if (!preprocessor_config_ || !tfnative_alloc_) {
    return TabRankerResult::kPreprocessorInitializationFailed;
  }
  // Build the RankerExample using the tab's features.
  assist_ranker::RankerExample example;
  PopulateTabFeaturesToRankerExample(tab, &example);
  return PredictWithPreprocess(&example, score);
}

TabRankerResult TabScorePredictor::PredictWithPreprocess(
    assist_ranker::RankerExample* example,
    float* score) {
  // Process the RankerExample with the tab ranker config to vectorize the
  // feature list for inference.
  int preprocessor_error = assist_ranker::ExamplePreprocessor::Process(
      *preprocessor_config_, example, true);
  if (preprocessor_error) {
    // kNoFeatureIndexFound can occur normally (e.g., when the domain name
    // isn't known to the model or a rarely seen enum value is used).
    DCHECK_EQ(assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound,
              preprocessor_error);
  }

  // This vector will be provided to the inference function.
  const auto& vectorized_features =
      example->features()
          .at(assist_ranker::ExamplePreprocessor::kVectorizedFeatureDefaultName)
          .float_list()
          .float_value();

  // Call correct inference function based on the type_.
  if (type_ == kMLScorer)
    tfnative_model::Inference(vectorized_features.data(), score,
                              tfnative_alloc_.get());
  if (type_ == kPairwiseScorer)
    pairwise_model::Inference(vectorized_features.data(), score,
                              pairwise_alloc_.get());

  if (preprocessor_error != assist_ranker::ExamplePreprocessor::kSuccess &&
      preprocessor_error !=
          assist_ranker::ExamplePreprocessor::kNoFeatureIndexFound) {
    // May indicate something is wrong with how we create the RankerExample.
    return TabRankerResult::kPreprocessorOtherError;
  } else {
    return TabRankerResult::kSuccess;
  }
}

TabRankerResult TabScorePredictor::ScoreTabWithMRUScorer(const TabFeatures& tab,
                                                         float* score) {
  *score = MruToScore(tab.mru_index * mru_scorer_penalty_);
  return TabRankerResult::kSuccess;
}

TabRankerResult TabScorePredictor::ScoreTabsPairs(const TabFeatures& tab1,
                                                  const TabFeatures& tab2,
                                                  float* score) {
  if (type_ == kPairwiseScorer) {
    // Lazy-load the preprocessor config.
    LazyInitialize();
    if (!preprocessor_config_ || !pairwise_alloc_) {
      return TabRankerResult::kPreprocessorInitializationFailed;
    }

    // Build the RankerExamples using the tab's features.
    assist_ranker::RankerExample example1, example2;
    PopulateTabFeaturesToRankerExample(tab1, &example1);
    PopulateTabFeaturesToRankerExample(tab2, &example2);

    // Merge features from example2 to example1.
    auto& features = *example1.mutable_features();
    for (const auto& feature : example2.features()) {
      const std::string new_name = base::StrCat({feature.first, "_1"});
      features[new_name] = feature.second;
    }

    // Inference on example1.
    return PredictWithPreprocess(&example1, score);
  } else {
    // For non-pairwise scorer, we simply calculate the score of each tab and
    // return the difference.
    float score1, score2;
    const TabRankerResult result1 = ScoreTab(tab1, &score1);
    const TabRankerResult result2 = ScoreTab(tab2, &score2);
    *score = score1 - score2;
    return std::max(result1, result2);
  }
}

std::map<int32_t, float> TabScorePredictor::ScoreTabsWithPairwiseScorer(
    const std::map<int32_t, base::Optional<TabFeatures>>& tabs) {
  const int N = tabs.size();

  std::vector<int32_t> ids;
  for (const auto& pair : tabs) {
    ids.push_back(pair.first);
  }

  // Sort ids by MRU first.
  // Put the tabs without TabFeatures in front so that they won't be discarded
  // mistakenly (including current Foregrounded tab).
  std::sort(ids.begin(), ids.end(),
            [&tabs](const int32_t id1, const int32_t id2) {
              const auto& tab1 = tabs.at(id1);
              const auto& tab2 = tabs.at(id2);
              if (!tab2)
                return false;
              if (!tab1)
                return true;
              return tab1->mru_index < tab2->mru_index;
            });

  std::map<int32_t, float> reactivation_scores;

  // start_index is the first one that has tab_features.
  int start_index = 0;
  for (int i = 0; i < N; ++i) {
    if (!tabs.at(ids[i])) {
      reactivation_scores[ids[i]] = N - i;
      start_index = i + 1;
    } else {
      break;
    }
  }

  // winning_indices records what's the best tab to be put at pos i.
  std::vector<int> winning_indices;
  for (int i = 0; i < N; ++i)
    winning_indices.push_back(i);

  int winning_index = N - 1;
  int swapped_index = N - 1;
  for (int j = start_index; j < N; ++j) {
    // Find the best candidate at j.

    // swapped_index < N - 1 means that one element has
    // just been swapped to swapped_index, we should re-calculate
    // winning_indices from swapped_index to j;
    if (swapped_index < N - 1) {
      // Set winning_index as the winning_indices at swapped_index + 1, since
      // ids from ids.back() to ids[swapped_index + 1] are not
      // changed.
      winning_index = winning_indices[swapped_index + 1];
    }

    for (int i = swapped_index; i >= j; --i) {
      // Compare ids[i] with ids[winning_index]; inference score > 0 means
      // that ids[i] is more likely to be reactivated, so we should prefer
      // ids[i] as new winning_index.
      float score = 0.0f;
      const TabRankerResult result = ScoreTabsPairs(
          tabs.at(ids[i]).value(), tabs.at(ids[winning_index]).value(), &score);
      if (result == TabRankerResult::kSuccess && score > 0.0f) {
        winning_index = i;
      }

      // Always update winning_indices.
      winning_indices[i] = winning_index;
    }

    // swap winning_index with j;
    std::swap(ids[winning_index], ids[j]);
    swapped_index = winning_index;

    // Find the best candidate for position j, set the score for ids[j].
    reactivation_scores[ids[j]] = N - j;
  }
  return reactivation_scores;
}
void TabScorePredictor::LazyInitialize() {
  // Load correct config and alloc based on type_.
  if (type_ == kMLScorer) {
    if (!preprocessor_config_)
      preprocessor_config_ = LoadExamplePreprocessorConfig(
          IDR_TAB_RANKER_EXAMPLE_PREPROCESSOR_CONFIG_PB);
    if (!tfnative_alloc_)
      tfnative_alloc_ = std::make_unique<tfnative_model::FixedAllocations>();
    DCHECK(preprocessor_config_);
    DCHECK_EQ(preprocessor_config_->feature_indices().size(),
              static_cast<std::size_t>(tfnative_model::FEATURES_SIZE));
  }

  if (type_ == kPairwiseScorer) {
    if (!preprocessor_config_)
      preprocessor_config_ = LoadExamplePreprocessorConfig(
          IDR_TAB_RANKER_PAIRWISE_EXAMPLE_PREPROCESSOR_CONFIG_PB);
    if (!pairwise_alloc_)
      pairwise_alloc_ = std::make_unique<pairwise_model::FixedAllocations>();
    DCHECK(preprocessor_config_);
    DCHECK_EQ(preprocessor_config_->feature_indices().size(),
              static_cast<std::size_t>(pairwise_model::FEATURES_SIZE));
  }
}

// Simply returns the frecency_score in the TabFeatures.
TabRankerResult TabScorePredictor::ScoreTabWithFrecencyScorer(
    const TabFeatures& tab,
    float* score) {
  *score = tab.frecency_score;
  return TabRankerResult::kSuccess;
}

}  // namespace tab_ranker
