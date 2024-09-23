// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/shortcut_customization_ui/backend/search/search_handler.h"

#include <optional>
#include <vector>

#include "ash/public/mojom/accelerator_info.mojom-forward.h"
#include "ash/public/mojom/accelerator_info.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search.mojom.h"
#include "ash/webui/shortcut_customization_ui/backend/search/search_concept.h"
#include "base/check.h"

// Sets the relevance_threshold to be low enough for single-character queries
// to produce results, but high enough to avoid too many irrelevant results.
// The default value is 0.64, at which we observed single-character queries
// produced no or few results. In testing, 0.4 was discovered to be too low of
// a threshold and reduced the quality of search results. We arrived at the
// current value by testing various combinations of queries. This value may
// need to be amended in the future.
const double search_service_relevance_threshold = 0.52;

namespace ash::shortcut_ui {

SearchHandler::SearchHandler(
    SearchConceptRegistry* search_concept_registry,
    local_search_service::LocalSearchServiceProxy* local_search_service_proxy)
    : search_concept_registry_(search_concept_registry) {
  CHECK(local_search_service_proxy);
  local_search_service_proxy->GetIndex(
      local_search_service::IndexId::kShortcutsApp,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  CHECK(index_remote_.is_bound());
  CHECK(search_concept_registry_);

  search_concept_registry_->AddObserver(this);

  index_remote_->SetSearchParams(
      {/*relevance_threshold=*/search_service_relevance_threshold},
      base::OnceCallback<void()>());
}

SearchHandler::~SearchHandler() {
  if (search_concept_registry_) {
    search_concept_registry_->RemoveObserver(this);
  }
}

void SearchHandler::BindInterface(
    mojo::PendingReceiver<shortcut_customization::mojom::SearchHandler>
        pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::OnRegistryUpdated() {
  for (auto& observer : observers_) {
    observer->OnSearchResultsAvailabilityChanged();
  }
}

void SearchHandler::AddSearchResultsAvailabilityObserver(
    mojo::PendingRemote<
        shortcut_customization::mojom::SearchResultsAvailabilityObserver>
        observer) {
  observers_.Add(std::move(observer));
}

void SearchHandler::Search(const std::u16string& query,
                           uint32_t max_num_results,
                           SearchCallback callback) {
  index_remote_->Find(
      query, max_num_results,
      base::BindOnce(&SearchHandler::OnFindComplete,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void SearchHandler::OnFindComplete(
    SearchCallback callback,
    local_search_service::ResponseStatus response_status,
    const std::optional<std::vector<local_search_service::Result>>&
        local_search_service_results) {
  if (response_status != local_search_service::ResponseStatus::kSuccess) {
    LOG(ERROR) << "Cannot search; LocalSearchService returned "
               << static_cast<int>(response_status)
               << ". Returning empty results array.";
    std::move(callback).Run({});
    return;
  }

  std::move(callback).Run(
      GenerateSearchResultsArray(local_search_service_results.value()));
}

std::vector<shortcut_customization::mojom::SearchResultPtr>
SearchHandler::GenerateSearchResultsArray(
    const std::vector<local_search_service::Result>&
        local_search_service_results) const {
  std::vector<shortcut_customization::mojom::SearchResultPtr> search_results;
  for (const auto& result : local_search_service_results) {
    shortcut_customization::mojom::SearchResultPtr result_ptr =
        ResultToSearchResult(result);
    if (result_ptr) {
      search_results.push_back(std::move(result_ptr));
    }
  }

  std::sort(search_results.begin(), search_results.end(), CompareSearchResults);

  return search_results;
}

shortcut_customization::mojom::SearchResultPtr
SearchHandler::ResultToSearchResult(
    const local_search_service::Result& result) const {
  const SearchConcept* search_concept =
      search_concept_registry_->GetSearchConceptById(result.id);

  // If the concept was not registered, no metadata is available. This can
  // occur if the search concept was dynamically unregistered during the
  // asynchronous Find() call.
  if (!search_concept) {
    return nullptr;
  }

  return shortcut_customization::mojom::SearchResult::New(
      /*accelerator_layout_info=*/search_concept->accelerator_layout_info
          ->Clone(),
      /*accelerator_infos=*/mojo::Clone(search_concept->accelerator_infos),
      /*relevance_score=*/result.score);
}

bool SearchHandler::CompareSearchResults(
    const shortcut_customization::mojom::SearchResultPtr& first,
    const shortcut_customization::mojom::SearchResultPtr& second) {
  return first->relevance_score > second->relevance_score;
}

}  // namespace ash::shortcut_ui