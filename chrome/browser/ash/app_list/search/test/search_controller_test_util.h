// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_CONTROLLER_TEST_UTIL_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_CONTROLLER_TEST_UTIL_H_

#include <memory>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "testing/gmock/include/gmock/gmock.h"

namespace app_list {

std::vector<std::unique_ptr<ChromeSearchResult>> MakeResults(
    const std::vector<std::string>& ids,
    const std::vector<ash::SearchResultDisplayType>& display_types,
    const std::vector<ash::AppListSearchResultCategory>& categories,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores);

std::vector<std::unique_ptr<ChromeSearchResult>> MakeFileResults(
    const std::vector<std::string>& ids,
    const std::vector<std::string>& fileNames,
    const std::vector<std::string>& dirs,
    const std::vector<ash::SearchResultDisplayType>& display_types,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores);

std::vector<std::unique_ptr<ChromeSearchResult>> MakeListResults(
    const std::vector<std::string>& ids,
    const std::vector<ash::AppListSearchResultCategory>& categories,
    const std::vector<int>& best_match_ranks,
    const std::vector<double>& scores);

// Returns a pointer to a search provider. Only valid until the next call to
// SimpleProvider.
static std::unique_ptr<SearchProvider> kProvider;
SearchProvider* SimpleProvider(ash::AppListSearchResultType result_type);

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_SEARCH_CONTROLLER_TEST_UTIL_H_
