// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/ftrl_ranker.h"

#include "base/strings/string_number_conversions.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"

namespace app_list {

// FtrlRanker ------------------------------------------------------------

FtrlRanker::FtrlRanker(FtrlRanker::RankingKind kind,
                       FtrlOptimizer::Params params,
                       FtrlOptimizer::Proto proto)
    : kind_(kind),
      ftrl_(std::make_unique<FtrlOptimizer>(std::move(proto), params)) {}

FtrlRanker::~FtrlRanker() = default;

void FtrlRanker::AddExpert(std::unique_ptr<Ranker> ranker) {
  rankers_.push_back(std::move(ranker));
}

void FtrlRanker::Start(const std::u16string& query,
                       const CategoriesList& categories) {
  for (auto& ranker : rankers_)
    ranker->Start(query, categories);

  ftrl_->Clear();
}

void FtrlRanker::Train(const LaunchData& launch) {
  for (auto& ranker : rankers_)
    ranker->Train(launch);

  switch (kind_) {
    case FtrlRanker::RankingKind::kResults:
      ftrl_->Train(launch.id);
      break;
    case FtrlRanker::RankingKind::kCategories:
      ftrl_->Train(CategoryToString(launch.category));
      break;
  }
}

void FtrlRanker::UpdateResultRanks(ResultsMap& results, ProviderType provider) {
  if (kind_ != FtrlRanker::RankingKind::kResults)
    return;

  const auto it = results.find(provider);
  DCHECK(it != results.end());
  const auto& new_results = it->second;

  // Create a vector of result ids.
  std::vector<std::string> ids;
  for (const auto& result : new_results)
    ids.push_back(result->id());

  // Create a vector of vectors for each expert's scores.
  std::vector<std::vector<double>> expert_scores;
  for (auto& ranker : rankers_)
    expert_scores.push_back(ranker->GetResultRanks(results, provider));

  // Get the final scores from the FTRL optimizer set them on the results.
  std::vector<double> result_scores =
      ftrl_->Score(std::move(ids), std::move(expert_scores));
  DCHECK_EQ(new_results.size(), result_scores.size());
  for (size_t i = 0; i < new_results.size(); ++i)
    new_results[i]->scoring().set_ftrl_result_score(result_scores[i]);
}

void FtrlRanker::UpdateCategoryRanks(const ResultsMap& results,
                                     CategoriesList& categories,
                                     ProviderType provider) {
  if (kind_ != FtrlRanker::RankingKind::kCategories)
    return;

  // Create a vector of category strings.
  std::vector<std::string> category_strings;
  for (const auto& category : categories)
    category_strings.push_back(CategoryToString(category.category));

  // Create a vector of vectors for each expert's scores.
  std::vector<std::vector<double>> expert_scores;
  for (auto& ranker : rankers_) {
    expert_scores.push_back(
        ranker->GetCategoryRanks(results, categories, provider));
  }

  // Get the final scores from the FTRL optimizer set them on the categories.
  std::vector<double> category_scores =
      ftrl_->Score(std::move(category_strings), std::move(expert_scores));
  DCHECK_EQ(categories.size(), category_scores.size());
  for (size_t i = 0; i < categories.size(); ++i)
    categories[i].score = category_scores[i];
}

// ResultScoringShim ----------------------------------------------------------

ResultScoringShim::ResultScoringShim(ResultScoringShim::ScoringMember member)
    : member_(member) {}

std::vector<double> ResultScoringShim::GetResultRanks(const ResultsMap& results,
                                                      ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end())
    return {};

  std::vector<double> scores;
  for (const auto& result : it->second) {
    double score;
    switch (member_) {
      case ResultScoringShim::ScoringMember::kNormalizedRelevance:
        score = result->scoring().normalized_relevance();
        break;
      case ResultScoringShim::ScoringMember::kMrfuResultScore:
        score = result->scoring().mrfu_result_score();
        break;
    }
    scores.push_back(score);
  }
  return scores;
}

// BestResultCategoryRanker ----------------------------------------------------

BestResultCategoryRanker::BestResultCategoryRanker() = default;
BestResultCategoryRanker::~BestResultCategoryRanker() = default;

void BestResultCategoryRanker::Start(const std::u16string& query,
                                     const CategoriesList& categories) {
  current_category_scores_.clear();
  for (const auto& category : categories)
    current_category_scores_[category.category] = 0.0;
}

std::vector<double> BestResultCategoryRanker::GetCategoryRanks(
    const ResultsMap& results,
    const CategoriesList& categories,
    ProviderType provider) {
  const auto it = results.find(provider);
  DCHECK(it != results.end());

  for (const auto& result : it->second) {
    // Ignore best match, answer card results, and filtered results for the
    // purposes of deciding category scores, because they are not displayed in
    // their category.
    if (result->best_match() || result->scoring().filtered() ||
        result->display_type() ==
            ChromeSearchResult::DisplayType::kAnswerCard) {
      continue;
    }
    current_category_scores_[result->category()] =
        std::max(result->scoring().normalized_relevance(),
                 current_category_scores_[result->category()]);
  }

  // Collect those scores into a vector with the same ordering as |categories|.
  // For categories with no results in them, defaults to 0.0.
  std::vector<double> result;
  for (const auto& category : categories)
    result.push_back(current_category_scores_[category.category]);
  return result;
}

}  // namespace app_list
