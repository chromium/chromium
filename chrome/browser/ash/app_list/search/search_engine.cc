// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_engine.h"

#include <algorithm>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

SearchEngine::SearchEngine(Profile* profile) : profile_(profile) {}

SearchEngine::~SearchEngine() = default;

void SearchEngine::AddProvider(std::unique_ptr<SearchProvider> provider) {
  providers_.emplace_back(std::move(provider));
}

void SearchEngine::StartSearch(const std::u16string& query,
                               SearchResultsCallback callback) {
  on_search_done_ = std::move(callback);

  auto on_provider_results = base::BindRepeating(
      &SearchEngine::OnProviderResults, base::Unretained(this));
  for (const auto& provider : providers_) {
    // Does not start the search of a provider if its control category is
    // disabled.
    // TODO(b/315709613): make it as a search option and move the logic back to
    // the SC.
    if (ash::features::IsLauncherSearchControlEnabled() &&
        !IsControlCategoryEnabled(profile_, provider->control_category())) {
      continue;
    }
    provider->Start(query, on_provider_results);
  }
}

void SearchEngine::OnProviderResults(
    ash::AppListSearchResultType result_type,
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  on_search_done_.Run(result_type, std::move(results));
}

void SearchEngine::StopQuery() {
  std::for_each(providers_.begin(), providers_.end(),
                [](const auto& provider) { provider->StopQuery(); });
}

void SearchEngine::StartZeroState(SearchResultsCallback callback) {
  on_search_done_ = std::move(callback);

  auto on_provider_results = base::BindRepeating(
      &SearchEngine::OnProviderResults, base::Unretained(this));
  std::for_each(providers_.begin(), providers_.end(),
                [&](const auto& provider) {
                  provider->StartZeroState(on_provider_results);
                });
}

void SearchEngine::StopZeroState() {
  std::for_each(providers_.begin(), providers_.end(),
                [](const auto& provider) { provider->StopZeroState(); });
}

// TODO(b/315709613): Remove from providers and move the logic back to the SC.
std::vector<ash::AppListSearchControlCategory>
SearchEngine::GetToggleableCategories() const {
  // Use a set to deduplicate and sort the elements in order.
  std::set<ash::AppListSearchControlCategory> category_set;
  for (auto& provider : providers_) {
    // Cannot toggle is not an actual search category.
    if (provider->control_category() ==
        ash::AppListSearchControlCategory::kCannotToggle) {
      continue;
    }

    category_set.insert(provider->control_category());
  }
  return std::vector<ash::AppListSearchControlCategory>(category_set.begin(),
                                                        category_set.end());
}

size_t SearchEngine::ReplaceProvidersForResultTypeForTest(
    ash::AppListSearchResultType result_type,
    std::unique_ptr<SearchProvider> new_provider) {
  DCHECK_EQ(result_type, new_provider->ResultType());

  size_t removed_providers = base::EraseIf(
      providers_, [&](const std::unique_ptr<SearchProvider>& provider) {
        return provider->ResultType() == result_type;
      });
  if (!removed_providers) {
    return 0u;
  }
  DCHECK_EQ(1u, removed_providers);

  AddProvider(std::move(new_provider));
  return removed_providers;
}

}  // namespace app_list
