// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/mrfu_ranker.h"

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/ranking/util.h"
#include "chrome/browser/ash/app_list/search/types.h"
#include "chrome/browser/ash/app_list/search/util/ftrl_optimizer.h"

namespace app_list {

// MrfuResultRanker -----------------------------------------------------------

MrfuResultRanker::MrfuResultRanker(MrfuCache::Params params,
                                   MrfuCache::Proto proto)
    : mrfu_(std::make_unique<MrfuCache>(std::move(proto), params)) {}

MrfuResultRanker::~MrfuResultRanker() = default;

std::vector<double> MrfuResultRanker::GetResultRanks(const ResultsMap& results,
                                                     ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end())
    return {};

  std::vector<double> scores;
  for (const auto& result : it->second)
    scores.push_back(mrfu_->Get(result->id()));
  return scores;
}

void MrfuResultRanker::UpdateResultRanks(ResultsMap& results,
                                         ProviderType provider) {
  const auto it = results.find(provider);
  if (it == results.end())
    return;

  for (auto& result : it->second)
    result->scoring().set_mrfu_result_score(mrfu_->Get(result->id()));
}

void MrfuResultRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }
  mrfu_->Use(launch.id);
}

// MrfuCategoryRanker ----------------------------------------------------------

MrfuCategoryRanker::MrfuCategoryRanker(
    MrfuCache::Params params,
    ash::PersistentProto<MrfuCacheProto> proto)
    : mrfu_(std::make_unique<MrfuCache>(std::move(proto), params)) {}

MrfuCategoryRanker::~MrfuCategoryRanker() = default;

void MrfuCategoryRanker::Start(const std::u16string& query,
                               const CategoriesList& categories) {
  if (mrfu_->initialized() && mrfu_->empty())
    SetDefaultCategoryScores();
}

std::vector<double> MrfuCategoryRanker::GetCategoryRanks(
    const ResultsMap& results,
    const CategoriesList& categories,
    ProviderType provider) {
  if (!mrfu_->initialized())
    return std::vector<double>(categories.size(), 0.0);

  // Build a map of the MRFU category scores, with 0.0 for unseen categories.
  base::flat_map<Category, double> scores_map;
  for (const auto& id_score : mrfu_->GetAllNormalized())
    scores_map[StringToCategory(id_score.first)] = id_score.second;

  std::vector<double> scores;
  for (const auto& category : categories) {
    const auto it = scores_map.find(category.category);
    scores.push_back(it != scores_map.end() ? it->second : 0.0);
  }
  DCHECK_EQ(scores.size(), categories.size());
  return scores;
}

void MrfuCategoryRanker::UpdateCategoryRanks(const ResultsMap& results,
                                             CategoriesList& categories,
                                             ProviderType provider) {
  const auto& scores = GetCategoryRanks(results, categories, provider);
  DCHECK_EQ(scores.size(), categories.size());
  if (scores.size() != categories.size())
    return;
  for (size_t i = 0; i < categories.size(); ++i)
    categories[i].score = scores[i];
}

void MrfuCategoryRanker::Train(const LaunchData& launch) {
  if (launch.launched_from !=
      ash::AppListLaunchedFrom::kLaunchedFromSearchBox) {
    return;
  }
  mrfu_->Use(CategoryToString(launch.category));
}

void MrfuCategoryRanker::SetDefaultCategoryScores() {
  // Default category prioritization:
  //
  //   P1: Apps
  //       Play Store
  //       Settings
  //   P2: Web
  //       Files (local and Drive)
  //       Shortcuts (which are under the Help category)
  //   P3: Everything else
  //
  // Achieve this by training once each, in reverse order.
  mrfu_->Use(CategoryToString(Category::kHelp));
  mrfu_->Use(CategoryToString(Category::kFiles));
  mrfu_->Use(CategoryToString(Category::kWeb));
  mrfu_->Use(CategoryToString(Category::kSettings));
  mrfu_->Use(CategoryToString(Category::kPlayStore));
  mrfu_->Use(CategoryToString(Category::kApps));
}

}  // namespace app_list
