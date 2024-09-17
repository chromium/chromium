// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/ranker_manager.h"

#include "chrome/browser/ash/app_list/search/ranking/answer_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/best_match_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/continue_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/filtering_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/ftrl_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/keyword_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/mrfu_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/query_highlighter.h"
#include "chrome/browser/ash/app_list/search/ranking/removed_results.pb.h"
#include "chrome/browser/ash/app_list/search/ranking/removed_results_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/score_normalizing_ranker.h"
#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/app_list/search/search_features.h"
#include "chrome/browser/ash/app_list/search/util/score_normalizer.h"
#include "chrome/browser/ash/app_list/search/util/score_normalizer.pb.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {
namespace {

// A standard write delay used for protos without time-sensitive writes. This is
// intended to be slightly longer than the longest conceivable latency for a
// search.
constexpr base::TimeDelta kStandardWriteDelay = base::Seconds(3);

}  // namespace

RankerManager::RankerManager(Profile* profile) {
  // Score normalization parameters:
  ScoreNormalizer::Params score_normalizer_params;
  // Change this version number when changing the number of bins below.
  score_normalizer_params.version = 1;
  // The maximum number of buckets the score normalizer discretizes result
  // scores into.
  score_normalizer_params.max_bins = 5;

  // Result ranking parameters.
  FtrlOptimizer::Params ftrl_result_params;
  ftrl_result_params.alpha = 0.1;
  ftrl_result_params.gamma = 0.1;
  ftrl_result_params.num_experts = 2u;

  MrfuCache::Params mrfu_result_params;
  mrfu_result_params.half_life = 30.0f;
  mrfu_result_params.boost_factor = 2.5f;
  mrfu_result_params.max_items = 200u;

  // Category ranking parameters.
  FtrlOptimizer::Params ftrl_category_params;
  ftrl_category_params.alpha = 0.1;
  ftrl_category_params.gamma = 0.1;
  ftrl_category_params.num_experts = 2u;

  MrfuCache::Params mrfu_category_params;
  mrfu_category_params.half_life = 20.0f;
  mrfu_category_params.boost_factor = 7.0f;
  mrfu_category_params.max_items = 20u;

  const auto state_dir = RankerStateDirectory(profile);

  // 1. Result pre-processing. These filter or modify search results but don't
  // change their scores.
  AddRanker(std::make_unique<QueryHighlighter>());
  AddRanker(std::make_unique<ContinueRanker>());
  AddRanker(std::make_unique<FilteringRanker>());
  AddRanker(std::make_unique<RemovedResultsRanker>(profile));

  // 2. Score normalization, a precursor to other ranking.
  AddRanker(std::make_unique<ScoreNormalizingRanker>(
      score_normalizer_params,
      ash::PersistentProto<ScoreNormalizerProto>(
          state_dir.AppendASCII("score_norm.pb"), kStandardWriteDelay)));

  // 3. Ranking for results.
  // 3a. Most-frequently-recently-used (MRFU) ranking.
  auto mrfu_ranker = std::make_unique<MrfuResultRanker>(
      mrfu_result_params,
      ash::PersistentProto<MrfuCacheProto>(
          state_dir.AppendASCII("mrfu_results.pb"), kStandardWriteDelay));
  AddRanker(std::move(mrfu_ranker));

  // 3b. Ensembling between MRFU and normalized score ranking.
  auto ftrl_ranker = std::make_unique<FtrlRanker>(
      FtrlRanker::RankingKind::kResults, ftrl_result_params,
      ash::PersistentProto<FtrlOptimizerProto>(
          state_dir.AppendASCII("ftrl_results.pb"), kStandardWriteDelay));
  ftrl_ranker->AddExpert(std::make_unique<ResultScoringShim>(
      ResultScoringShim::ScoringMember::kNormalizedRelevance));
  ftrl_ranker->AddExpert(std::make_unique<ResultScoringShim>(
      ResultScoringShim::ScoringMember::kMrfuResultScore));
  AddRanker(std::move(ftrl_ranker));

  // 4. Ranking for categories
  AddRanker(std::make_unique<MrfuCategoryRanker>(
      mrfu_category_params,
      ash::PersistentProto<MrfuCacheProto>(
          state_dir.AppendASCII("mrfu_categories.pb"), kStandardWriteDelay)));

  // TODO(b/274921356): Temporarily comment out the `KeywordRanker` construction
  // to avoid any possible crashes. Re-enable it when we make sure this problem
  // has been fixed.
  //
  // if (search_features::IsLauncherKeywordExtractionScoringEnabled()) {
  //   AddRanker(std::make_unique<KeywordRanker>());
  // }

  // 5. Result post-processing.
  // Nb. the best match ranker relies on score normalization, and the answer
  // ranker relies on the best match ranker.
  AddRanker(std::make_unique<BestMatchRanker>());
  AddRanker(std::make_unique<AnswerRanker>());
}

RankerManager::~RankerManager() {}

void RankerManager::Start(const std::u16string& query,
                          const CategoriesList& categories) {
  for (auto& ranker : rankers_) {
    ranker->Start(query, categories);
  }
}

void RankerManager::UpdateResultRanks(ResultsMap& results,
                                      ProviderType provider) {
  for (auto& ranker : rankers_) {
    ranker->UpdateResultRanks(results, provider);
  }
}

void RankerManager::UpdateCategoryRanks(const ResultsMap& results,
                                        CategoriesList& categories,
                                        ProviderType provider) {
  for (auto& ranker : rankers_) {
    ranker->UpdateCategoryRanks(results, categories, provider);
  }
}

void RankerManager::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_) {
    ranker->Train(launch);
  }
}

void RankerManager::Remove(ChromeSearchResult* result) {
  for (auto& ranker : rankers_) {
    ranker->Remove(result);
  }
}

void RankerManager::AddRanker(std::unique_ptr<Ranker> ranker) {
  rankers_.emplace_back(std::move(ranker));
}

void RankerManager::OnBurnInPeriodElapsed() {
  for (auto& ranker : rankers_) {
    ranker->OnBurnInPeriodElapsed();
  }
}

}  // namespace app_list
