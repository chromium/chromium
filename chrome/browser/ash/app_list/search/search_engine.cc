// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_engine.h"

#include <algorithm>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_controller.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/profiles/profile.h"

namespace app_list {

SearchOptions::SearchOptions() = default;

SearchOptions::~SearchOptions() = default;

SearchOptions::SearchOptions(const SearchOptions&) = default;

SearchOptions& SearchOptions::operator=(const SearchOptions&) = default;

SearchEngine::SearchEngine(Profile* profile) : profile_(profile) {}

SearchEngine::~SearchEngine() = default;

void SearchEngine::AddProvider(std::unique_ptr<SearchProvider> provider) {
  providers_[provider->search_category()].push_back(std::move(provider));
}

void SearchEngine::StartSearch(const std::u16string& query,
                               SearchOptions search_options,
                               SearchResultsCallback callback) {
  StopQuery();
  on_search_done_ = std::move(callback);

  auto on_provider_results = base::BindRepeating(
      &SearchEngine::OnProviderResults, base::Unretained(this));

  for (const auto& category :
       search_options.search_categories.value_or(GetAllSearchCategories())) {
    for (const auto& provider : providers_[category]) {
      provider->Start(query, on_provider_results);
    }
  }
}

void SearchEngine::OnProviderResults(
    ash::AppListSearchResultType result_type,
    std::vector<std::unique_ptr<ChromeSearchResult>> results) {
  on_search_done_.Run(result_type, std::move(results));
}

void SearchEngine::StopQuery() {
  for (const auto& [category, providers] : providers_) {
    for (const auto& provider : providers) {
      provider->StopQuery();
    }
  }
}

void SearchEngine::StartZeroState(SearchResultsCallback callback) {
  StopZeroState();
  on_search_done_ = std::move(callback);

  auto on_provider_results = base::BindRepeating(
      &SearchEngine::OnProviderResults, base::Unretained(this));

  for (const auto& [category, providers] : providers_) {
    for (const auto& provider : providers) {
      provider->StartZeroState(on_provider_results);
    }
  }
}

void SearchEngine::StopZeroState() {
  for (const auto& [category, providers] : providers_) {
    for (const auto& provider : providers) {
      provider->StopZeroState();
    }
  }
}

std::vector<SearchCategory> SearchEngine::GetAllSearchCategories() const {
  std::vector<SearchCategory> categories;
  for (const auto& [category, _] : providers_) {
    categories.push_back(category);
  }

  return categories;
}

size_t SearchEngine::ReplaceProvidersForResultTypeForTest(
    ash::AppListSearchResultType result_type,
    std::unique_ptr<SearchProvider> new_provider) {
  DCHECK_EQ(result_type, new_provider->ResultType());

  size_t removed_providers = 0;
  for (auto& [category, providers] : providers_) {
    removed_providers += std::erase_if(
        providers, [&](const std::unique_ptr<SearchProvider>& provider) {
          return provider->ResultType() == result_type;
        });
  }
  if (!removed_providers) {
    return 0u;
  }
  DCHECK_EQ(1u, removed_providers);

  AddProvider(std::move(new_provider));
  return removed_providers;
}

}  // namespace app_list
