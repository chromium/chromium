// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/search_controller_test_util.h"

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/time/time.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/ash/app_list/search/test/test_result.h"
#include "test_search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

std::vector<std::unique_ptr<ChromeSearchResult>> MakeResults(
    const std::vector<std::string>& ids,
    const std::vector<ash::SearchResultDisplayType>& display_types,
    const std::vector<ash::AppListSearchResultCategory>& categories,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores) {
  std::vector<std::unique_ptr<ChromeSearchResult>> results;
  for (size_t i = 0; i < ids.size(); ++i) {
    results.emplace_back(std::make_unique<TestResult>(
        ids[i], display_types[i], categories[i], best_match_ranks[i],
        /*relevance=*/scores[i], /*ftrl_result_score=*/scores[i]));
  }
  return results;
}

std::vector<std::unique_ptr<ChromeSearchResult>> MakeFileResults(
    const std::vector<std::string>& ids,
    const std::vector<std::string>& fileNames,
    const std::vector<std::string>& dirs,
    const std::vector<ash::SearchResultDisplayType>& display_types,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores) {
  std::vector<std::unique_ptr<ChromeSearchResult>> results;
  for (size_t i = 0; i < ids.size(); ++i) {
    results.emplace_back(std::make_unique<TestResult>(
        ids[i], display_types[i], Category::kFiles, fileNames[i],
        dirs[i] + fileNames[i], best_match_ranks[i],
        /*relevance=*/scores[i], /*ftrl_result_score=*/scores[i]));
  }
  return results;
}

std::vector<std::unique_ptr<ChromeSearchResult>> MakeListResults(
    const std::vector<std::string>& ids,
    const std::vector<ash::AppListSearchResultCategory>& categories,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores) {
  std::vector<ash::SearchResultDisplayType> display_types(
      ids.size(), ash::SearchResultDisplayType::kList);
  return MakeResults(ids, display_types, categories, best_match_ranks, scores);
}

SearchProvider* SimpleProvider(ash::AppListSearchResultType result_type) {
  kProvider =
      std::make_unique<TestSearchProvider>(result_type, base::Seconds(0));
  return kProvider.get();
}

}  // namespace app_list
