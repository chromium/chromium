// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/functional/bind.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

TestSearchController::TestSearchController()
    : SearchController(nullptr, nullptr, nullptr, nullptr, nullptr) {}

TestSearchController::~TestSearchController() = default;

void TestSearchController::ClearSearch() {
  if (!ash::IsZeroStateResultType(provider_->ResultType())) {
    last_results_.clear();
  }
  provider_->StopQuery();
}

void TestSearchController::StartSearch(const std::u16string& query) {
  // The search controller used when categorical search is enabled clears all
  // results when starting another search query - simulate this behavior in
  // tests when categorical search is enabled.
  if (!ash::IsZeroStateResultType(provider_->ResultType())) {
    last_results_.clear();
  }
  provider_->Start(query, base::BindRepeating(&TestSearchController::SetResults,
                                              base::Unretained(this)));
}

void TestSearchController::StartZeroState(base::OnceClosure on_done,
                                          base::TimeDelta timeout) {
  last_results_.clear();
  provider_->StartZeroState(base::BindRepeating(
      &TestSearchController::SetResults, base::Unretained(this)));
}

void TestSearchController::AddProvider(
    std::unique_ptr<SearchProvider> provider) {
  DCHECK(!provider_);
  provider_ = std::move(provider);
}

void TestSearchController::SetResults(ash::AppListSearchResultType result_type,
                                      Results results) {
  last_results_ = std::move(results);
  if (results_changed_callback_) {
    results_changed_callback_.Run(result_type);
  }
}

void TestSearchController::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  results_changed_callback_ = callback;
}

void TestSearchController::WaitForZeroStateCompletionForTest(
    base::OnceClosure callback) {
  std::move(callback).Run();
}

}  // namespace app_list
