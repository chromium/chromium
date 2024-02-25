// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/search_provider.h"

#include <utility>

#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"

namespace app_list {

SearchProvider::SearchProvider(SearchCategory search_category)
    : search_category_(search_category) {}
SearchProvider::~SearchProvider() = default;

void SearchProvider::Start(const std::u16string& query,
                           OnSearchResultsCallback on_search_done) {
  on_search_done_ = std::move(on_search_done);
  Start(query);
}

void SearchProvider::StartZeroState(OnSearchResultsCallback on_search_done) {
  on_search_done_ = std::move(on_search_done);
  StartZeroState();
}

void SearchProvider::SwapResults(Results* new_results) {
  Results results;
  results.swap(*new_results);
  if (on_search_done_) {
    on_search_done_.Run(ResultType(), std::move(results));
  }
}

}  // namespace app_list
