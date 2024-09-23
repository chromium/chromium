// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_ranker_manager.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

TestRankerManager::TestRankerManager(Profile* profile)
    : RankerManager(profile) {}

TestRankerManager::~TestRankerManager() = default;

void TestRankerManager::SetCategoryRanks(
    base::flat_map<ash::AppListSearchResultCategory, double> category_ranks) {
  category_ranks_ = category_ranks;
}

void TestRankerManager::SetBestMatchString(
    const std::u16string& best_match_string) {
  best_match_string_ = best_match_string;
}

// Ranker:
void TestRankerManager::UpdateResultRanks(ResultsMap& results,
                                          ProviderType provider) {
  if (best_match_string_.empty()) {
    return;
  }
  for (auto& result : results[provider]) {
    if (result->title().find(best_match_string_) != std::u16string::npos) {
      result->SetBestMatch(true);
    }
  }
}

// Ranker:
void TestRankerManager::UpdateCategoryRanks(const ResultsMap& results,
                                            CategoriesList& categories,
                                            ProviderType provider) {
  for (auto& category : categories) {
    const auto it = category_ranks_.find(category.category);
    if (it != category_ranks_.end())
      category.score = it->second;
  }
}

void TestRankerManager::Train(const LaunchData& launch) {
  did_train_ = true;
}

}  // namespace app_list
