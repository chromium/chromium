// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_ENGINE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_ENGINE_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback_list.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"

class ChromeSearchResult;
class Profile;

namespace app_list {

class SearchProvider;

class SearchEngine {
 public:
  using SearchResultsCallback = base::RepeatingCallback<void(
      ash::AppListSearchResultType result_type,
      std::vector<std::unique_ptr<ChromeSearchResult>> results)>;

  explicit SearchEngine(Profile* profile);
  ~SearchEngine();

  SearchEngine(const SearchEngine&) = delete;
  SearchEngine& operator=(const SearchEngine&) = delete;

  void AddProvider(std::unique_ptr<SearchProvider> provider);

  void StartSearch(const std::u16string& query, SearchResultsCallback callback);
  void StopQuery();

  void StartZeroState(SearchResultsCallback callback);
  void StopZeroState();

  std::vector<ash::AppListSearchControlCategory> GetToggleableCategories()
      const;

  size_t ReplaceProvidersForResultTypeForTest(
      ash::AppListSearchResultType result_type,
      std::unique_ptr<SearchProvider> new_provider);

 private:
  void OnProviderResults(
      ash::AppListSearchResultType result_type,
      std::vector<std::unique_ptr<ChromeSearchResult>> results);

  std::vector<std::unique_ptr<SearchProvider>> providers_;

  base::flat_map<ash::AppListSearchResultType,
                 std::vector<std::unique_ptr<ChromeSearchResult>>>
      results_;

  SearchResultsCallback on_search_done_;

  const raw_ptr<Profile> profile_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_ENGINE_H_
