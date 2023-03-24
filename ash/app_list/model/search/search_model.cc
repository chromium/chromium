// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/search/search_model.h"

#include <map>
#include <string>
#include <utility>

#include "base/functional/bind.h"

namespace ash {

SearchModel::SearchModel()
    : search_box_(std::make_unique<SearchBoxModel>()),
      results_(std::make_unique<SearchResults>()),
      ordered_categories_(std::vector<ash::AppListSearchResultCategory>()) {}

SearchModel::~SearchModel() {}

void SearchModel::SetSearchEngineIsGoogle(bool is_google) {
  search_box_->SetSearchEngineIsGoogle(is_google);
}

void SearchModel::SetWouldTriggerLauncherSearchIph(bool would_trigger) {
  search_box_->SetWouldTriggerIph(would_trigger);
}

std::vector<SearchResult*> SearchModel::FilterSearchResultsByFunction(
    SearchResults* results,
    const base::RepeatingCallback<bool(const SearchResult&)>& result_filter,
    size_t max_results) {
  std::vector<SearchResult*> matches;
  for (size_t i = 0; i < results->item_count(); ++i) {
    if (matches.size() == max_results)
      break;
    SearchResult* item = results->GetItemAt(i);
    if (result_filter.Run(*item))
      matches.push_back(item);
  }
  return matches;
}

void SearchModel::PublishResults(
    std::vector<std::unique_ptr<SearchResult>> new_results,
    const std::vector<ash::AppListSearchResultCategory>& categories) {
  ordered_categories_ = categories;

  // The following algorithm is used:
  // 1. Transform the |results_| list into an unordered map from result ID
  // to item.
  // 2. Use the order of items in |new_results| to build an ordered list. If the
  // result IDs exist in the map, update and use the item in the map and delete
  // it from the map afterwards. Otherwise, clone new items from |new_results|.
  // 3. Delete the objects remaining in the map as they are unused.

  // We have to erase all results at once so that observers are notified with
  // meaningful indexes.
  auto current_results = results_->RemoveAll();
  std::map<std::string, std::unique_ptr<SearchResult>> results_map;
  for (auto& ui_result : current_results)
    results_map[ui_result->id()] = std::move(ui_result);

  // Add items back to |results_| in the order of |new_results|.
  for (auto&& new_result : new_results) {
    auto ui_result_it = results_map.find(new_result->id());
    if (ui_result_it != results_map.end()) {
      // Update and use the old result if it exists.
      std::unique_ptr<SearchResult> ui_result = std::move(ui_result_it->second);
      ui_result->SetMetadata(new_result->TakeMetadata());

      results_->Add(std::move(ui_result));
      // Remove the item from the map so that it ends up only with unused
      // results.
      results_map.erase(ui_result_it);
    } else {
      // Copy the result from |new_results| otherwise.
      results_->Add(std::move(new_result));
    }
  }

  // Any remaining results in |results_map| will be automatically deleted.
}

SearchResult* SearchModel::FindSearchResult(const std::string& id) {
  for (const auto& result : *results_) {
    if (result->id() == id)
      return result.get();
  }
  return nullptr;
}

void SearchModel::DeleteAllResults() {
  PublishResults(std::vector<std::unique_ptr<SearchResult>>(),
                 std::vector<ash::AppListSearchResultCategory>());
}

}  // namespace ash
