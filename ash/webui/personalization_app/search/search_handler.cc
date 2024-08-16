// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/webui/personalization_app/search/search_handler.h"

#include <algorithm>
#include <memory>
#include <optional>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/personalization_app/enterprise_policy_delegate.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/search/search.mojom-forward.h"
#include "ash/webui/personalization_app/search/search.mojom.h"
#include "ash/webui/personalization_app/search/search_concept.h"
#include "ash/webui/personalization_app/search/search_tag_registry.h"
#include "base/check.h"
#include "base/notreached.h"
#include "base/strings/string_number_conversions.h"
#include "chromeos/ash/components/local_search_service/public/cpp/local_search_service_proxy.h"
#include "chromeos/ash/components/local_search_service/shared_structs.h"
#include "chromeos/strings/grit/chromeos_strings.h"
#include "components/prefs/pref_service.h"
#include "mojo/public/cpp/bindings/pending_receiver.h"
#include "ui/base/l10n/l10n_util.h"

namespace ash::personalization_app {

namespace {

// Sorts search results descending.
bool CompareSearchResults(const mojom::SearchResultPtr& a,
                          const mojom::SearchResultPtr& b) {
  return a->relevance_score > b->relevance_score;
}

}  // namespace

SearchHandler::SearchHandler(
    local_search_service::LocalSearchServiceProxy& local_search_service_proxy,
    PrefService* pref_service,
    std::unique_ptr<EnterprisePolicyDelegate> enterprise_policy_delegate)
    : search_tag_registry_(std::make_unique<SearchTagRegistry>(
          local_search_service_proxy,
          pref_service,
          std::move(enterprise_policy_delegate))) {
  local_search_service_proxy.GetIndex(
      local_search_service::IndexId::kPersonalization,
      local_search_service::Backend::kLinearMap,
      index_remote_.BindNewPipeAndPassReceiver());
  DCHECK(index_remote_.is_bound());

  search_tag_registry_observer_.Observe(search_tag_registry_.get());
}

// For testing purposes only.
SearchHandler::SearchHandler() = default;

SearchHandler::~SearchHandler() = default;

void SearchHandler::BindInterface(
    mojo::PendingReceiver<mojom::SearchHandler> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void SearchHandler::Search(const std::u16string& query,
                           uint32_t max_num_results,
                           SearchCallback callback) {
  // Request more than the maximum, then sort and take the top
  // |max_num_results|. This helps the quality of search results.
  uint32_t max_local_search_service_results = 5 * max_num_results;
  index_remote_->Find(query, max_local_search_service_results,
                      base::BindOnce(&SearchHandler::OnLocalSearchDone,
                                     weak_ptr_factory_.GetWeakPtr(),
                                     std::move(callback), max_num_results));
}

void SearchHandler::AddObserver(
    mojo::PendingRemote<mojom::SearchResultsObserver> observer) {
  observers_.Add(std::move(observer));
}

void SearchHandler::OnRegistryUpdated() {
  for (auto& observer : observers_) {
    observer->OnSearchResultsChanged();
  }
}

void SearchHandler::OnLocalSearchDone(
    SearchCallback callback,
    uint32_t max_num_results,
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
  DCHECK(local_search_service_results.has_value());

  std::vector<mojom::SearchResultPtr> search_results;
  for (const auto& local_result : local_search_service_results.value()) {
    const auto* search_concept =
        search_tag_registry_->GetSearchConceptById(local_result.id);

    if (!search_concept) {
      continue;
    }

    int matching_content_id;
    if (!base::StringToInt(local_result.positions.front().content_id,
                           &matching_content_id)) {
      NOTREACHED() << "All content ids are expected to be a valid integer: "
                   << local_result.positions.front().content_id;
    }

    search_results.push_back(mojom::SearchResult::New(
        /*id=*/search_concept->id,
        /*text=*/SearchTagRegistry::MessageIdToString(matching_content_id),
        /*relative_url=*/search_concept->relative_url,
        /*relevance_score=*/local_result.score));
  }

  // Limit to top |max_num_results| results. Use partial_sort and then resize.
  std::partial_sort(
      search_results.begin(),
      std::min(search_results.begin() + max_num_results, search_results.end()),
      search_results.end(), CompareSearchResults);
  search_results.resize(
      std::min(static_cast<size_t>(max_num_results), search_results.size()));

  std::move(callback).Run(std::move(search_results));
}

}  // namespace ash::personalization_app
