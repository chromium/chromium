// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/ranking/ranker.h"

#include "base/notreached.h"

namespace app_list {

void Ranker::Start(const std::u16string& query,
                   const CategoriesList& categories) {}

// Ranks search results. Should return a vector of scores that is the same
// length as |results|.
std::vector<double> Ranker::GetResultRanks(const ResultsMap& results,
                                           ProviderType provider) {
  NOTREACHED_IN_MIGRATION()
      << "This function should be overridden by its child ranker.";
  return std::vector<double>(results.size(), 0.0);
}

// Ranks search results. Implementations should modify the scoring structs of
// |results|, but not modify the ordering of the vector itself.
void Ranker::UpdateResultRanks(ResultsMap& results, ProviderType provider) {}

// Ranks categories. Should return a vector of scores that is the same
// length as |categories|.
std::vector<double> Ranker::GetCategoryRanks(const ResultsMap& results,
                                             const CategoriesList& categories,
                                             ProviderType provider) {
  NOTREACHED_IN_MIGRATION()
      << "This function should be overridden by its child ranker.";
  return std::vector<double>(categories.size(), 0.0);
}

// Ranks categories. Implementations should modify the scoring members of
// structs in |categories|, but not modify the ordering of the vector itself.
void Ranker::UpdateCategoryRanks(const ResultsMap& results,
                                 CategoriesList& categories,
                                 ProviderType provider) {}

void Ranker::Train(const LaunchData& launch) {}

void Ranker::Remove(ChromeSearchResult* result) {}

void Ranker::OnBurnInPeriodElapsed() {}

}  // namespace app_list
