// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_
#define ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_

#include <memory>
#include <set>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_model_export.h"
#include "ash/app_list/model/search/search_box_model.h"
#include "ash/app_list/model/search/search_result.h"
#include "base/functional/callback.h"
#include "ui/base/models/list_model.h"

namespace ash {

class SearchBoxModel;

// A model of app list that holds two search related sub models:
// - SearchBoxModel: the model for SearchBoxView.
// - SearchResults: owning a list of SearchResult.
class APP_LIST_MODEL_EXPORT SearchModel {
 public:
  using SearchResults = ui::ListModel<SearchResult>;

  SearchModel();

  SearchModel(const SearchModel&) = delete;
  SearchModel& operator=(const SearchModel&) = delete;

  ~SearchModel();

  void SetSearchEngineIsGoogle(bool is_google);
  bool search_engine_is_google() const {
    return search_box_->search_engine_is_google();
  }

  void SetWouldTriggerLauncherSearchIph(bool would_trigger);
  bool would_trigger_iph() const { return search_box_->would_trigger_iph(); }

  // Filter the given |results| by those which |result_filter| returns true for.
  // The returned list is truncated to |max_results|.
  static std::vector<SearchResult*> FilterSearchResultsByFunction(
      SearchResults* results,
      const base::RepeatingCallback<bool(const SearchResult&)>& result_filter,
      size_t max_results);

  SearchBoxModel* search_box() { return search_box_.get(); }
  SearchResults* results() { return results_.get(); }
  std::vector<ash::AppListSearchResultCategory>* ordered_categories() {
    return &ordered_categories_;
  }

  void PublishResults(
      std::vector<std::unique_ptr<SearchResult>> new_results,
      const std::vector<ash::AppListSearchResultCategory>& categories);

  SearchResult* FindSearchResult(const std::string& id);

  // Deletes all search results. This is used when moving from zero-state to a
  // search query.
  void DeleteAllResults();

 private:
  std::unique_ptr<SearchBoxModel> search_box_;
  std::unique_ptr<SearchResults> results_;
  std::vector<ash::AppListSearchResultCategory> ordered_categories_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_SEARCH_SEARCH_MODEL_H_
