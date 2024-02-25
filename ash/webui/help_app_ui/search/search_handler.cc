// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/help_app_ui/search/search_handler.h"

#include <algorithm>
#include <memory>
#include <utility>

#include "base/functional/bind.h"
#include "base/functional/callback.h"
#include "base/functional/callback_helpers.h"
#include "base/metrics/histogram_functions.h"
#include "base/strings/string_number_conversions.h"
#include "base/strings/utf_string_conversions.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"

namespace ash::help_app {
namespace {

// The path to save the help app persistence within the user's cryptohome.
constexpr char kHelpAppDir[] = "help_app/";

// The end result of a search. Logged once per time a search finishes.
// Not logged if the search is canceled by a new search starting. These values
// persist to logs. Entries should not be renumbered and numeric values should
// never be reused.
enum class SearchResultStatus {
  // There is no cache update, and the index is still empty, so the Search
  // Handler is not ready to handle searches.
  kNotReadyAndEmptyIndex = 0,
  // Not ready and the cache status is updating. This should be far less common
  // than kNotReadyAndEmptyIndex.
  kNotReadyAndUpdating = 1,
  // Ready and the LSS response status is Success.
  kReadyAndSuccess = 2,
  // Ready and the LSS response status is EmptyIndex. This can happen for
  // languages with no localized content to add to the search index.
  kReadyAndEmptyIndex = 3,
  // Ready and the LSS response status is something other than Success or
  // EmptyIndex.
  kReadyAndOtherStatus = 4,
  kMaxValue = kReadyAndOtherStatus,
};

// The current cache status of the search handler. We should only load from disk
// if the cache is empty, and should only provide search service if the cache is
// ready.
enum class CacheStatus {
  // The cache is empty.
  kEmpty = 0,
  // The cache is updating.
  kUpdating = 1,
  // The cache is updated and ready for search.
  kReady = 2,
};

// Use this in OnFindComplete.
void LogSearchResultStatus(SearchResultStatus state) {
  base::UmaHistogramEnumeration("Discover.SearchHandler.SearchResultStatus",
                                state);
}

// Order search results by relevance score. Higher relevance first.
bool CompareSearchResults(const mojom::SearchResultPtr& first,
                          const mojom::SearchResultPtr& second) {
  return first->relevance_score > second->relevance_score;
}

}  // namespace

SearchHandler::SearchHandler(
    SearchTagRegistry* search_tag_registry,
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy)
    : search_tag_registry_(search_tag_registry),
      cache_status_(CacheStatus::kEmpty),
      construction_time_(base::TimeTicks::Now()) {
  local_search_service_proxy->GetIndex(
      local_search_service::IndexId::kHelpAppLauncher,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());

  search_tag_registry_->AddObserver(this);

  // Set the search params to make fuzzy and prefix matching stricter.
  // This reduces the number of irrelevant search results.
  index_remote_->SetSearchParams(
      {
          /*relevance_threshold=*/0.73,  // The threshold used by linear map.
          /*prefix_threshold=*/0.8,
          /*fuzzy_threshold=*/0.85,
      },
      base::OnceCallback<void()>());
}

SearchHandler::~SearchHandler() {
  search_tag_registry_->RemoveObserver(this);
}

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const std::u16string& query,
                           uint32_t max_num_results,
                           SearchCallback callback) {
  // Search for 5x the maximum set of results. If there are many matches for
  // a query, it may be the case that |index_| returns some matches with higher
  // SearchResultDefaultRank values later in the list. Requesting up to 5x the
  // maximum number ensures that such results will be returned and can be ranked
  // accordingly when sorted.
  uint32_t max_local_search_service_results = 5 * max_num_results;

  // Reject the search request if the cache is not ready yet.
  if (cache_status_ != CacheStatus::kReady) {
    LogSearchResultStatus(cache_status_ == CacheStatus::kEmpty
                              ? SearchResultStatus::kNotReadyAndEmptyIndex
                              : SearchResultStatus::kNotReadyAndUpdating);
    std::move(callback).Run({});
    return;
  }
  index_remote_->Find(query, max_local_search_service_results,
                      base::BindOnce(&SearchHandler::OnFindComplete,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback), max_num_results));
}

void SearchHandler::Update(std::vector<mojom::SearchConceptPtr> concepts,
                           UpdateCallback callback) {
  // Temporarily disable the search before the update is complete to avoid
  // data inconsistency.
  cache_status_ = CacheStatus::kUpdating;
  // Update the persistence if the path is available.
  if (persistence_) {
    persistence_->UpdateSearchConcepts(concepts);
  }

  if (concepts.empty()) {
    // Trying to update with an empty list causes an error in the LSS.
    cache_status_ = CacheStatus::kReady;
    std::move(callback).Run();
    return;
  }

  search_tag_registry_->ClearAndUpdate(std::move(concepts),
                                       std::move(callback));
}

void SearchHandler::Observe(
    mojo::PendingRemote<mojom::SearchResultsObserver> observer) {
  observers_.Add(std::move(observer));
}

void SearchHandler::OnProfileDirAvailable(const base::FilePath& profile_dir) {
  persistence_ = std::make_unique<SearchConcept>(
      profile_dir.AppendASCII(kHelpAppDir).AppendASCII("persistence.pb"));

  // Attempt to read from persistence.
  persistence_->GetSearchConcepts(
      base::BindOnce(&SearchHandler::OnPersistenceReadComplete,
                     weak_ptr_factory_.GetWeakPtr()));
}

void SearchHandler::OnRegistryUpdated() {
  cache_status_ = CacheStatus::kReady;
  if (!construction_time_.is_null()) {
    base::UmaHistogramTimes("Discover.SearchHandler.SearchAvailableTime",
                            base::TimeTicks::Now() - construction_time_);
    // Reset as we only care about the first time the search is available.
    construction_time_ = base::TimeTicks();
  }
  for (auto& observer : observers_)
    observer->OnSearchResultAvailabilityChanged();
}

std::vector<mojom::SearchResultPtr> SearchHandler::GenerateSearchResultsArray(
    const std::vector<local_search_service::Result>&
        local_search_service_results,
    uint32_t max_num_results) const {
  std::vector<mojom::SearchResultPtr> search_results;
  for (const auto& result : local_search_service_results) {
    if (search_results.size() == max_num_results) {
      break;
    }
    mojom::SearchResultPtr result_ptr = ResultToSearchResult(result);
    if (result_ptr)
      search_results.push_back(std::move(result_ptr));
  }
  std::sort(search_results.begin(), search_results.end(), CompareSearchResults);

  return search_results;
}

void SearchHandler::OnFindComplete(
    SearchCallback callback,
    uint32_t max_num_results,
    local_search_service::ResponseStatus response_status,
    const std::optional<std::vector<local_search_service::Result>>&
        local_search_service_results) {
  if (response_status != local_search_service::ResponseStatus::kSuccess) {
    LogSearchResultStatus(
        response_status == local_search_service::ResponseStatus::kEmptyIndex
            ? SearchResultStatus::kReadyAndEmptyIndex
            : SearchResultStatus::kReadyAndOtherStatus);
    std::move(callback).Run({});
    return;
  }
  LogSearchResultStatus(SearchResultStatus::kReadyAndSuccess);

  std::move(callback).Run(GenerateSearchResultsArray(
      local_search_service_results.value(), max_num_results));
}

void SearchHandler::OnPersistenceReadComplete(
    std::vector<mojom::SearchConceptPtr> concepts) {
  // Only update from persistence if the cache is empty.
  if (cache_status_ != CacheStatus::kEmpty) {
    return;
  }
  cache_status_ = CacheStatus::kUpdating;
  if (concepts.empty()) {
    // Trying to update with an empty list causes an error in the LSS.
    cache_status_ = CacheStatus::kReady;
    return;
  }

  search_tag_registry_->Update(concepts, base::DoNothing());
}

mojom::SearchResultPtr SearchHandler::ResultToSearchResult(
    const local_search_service::Result& result) const {
  const auto& metadata = search_tag_registry_->GetTagMetadata(result.id);
  // This should not happen because there isn't a way to remove metadata.
  if (&metadata == &SearchTagRegistry::not_found_) {
    return nullptr;
  }

  // Empty locale because we assume the locale always matches the system locale.
  return mojom::SearchResult::New(
      /*id=*/result.id,
      /*title=*/metadata.title,
      /*main_category=*/metadata.main_category,
      /*url_path_with_parameters=*/metadata.url_path_with_parameters,
      /*locale=*/"",
      /*relevance_score=*/result.score);
}

}  // namespace ash::help_app
