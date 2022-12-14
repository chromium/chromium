// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/test_search_controller.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "chrome/browser/ash/app_list/search/search_provider.h"

namespace app_list {

TestSearchController::TestSearchController() = default;
TestSearchController::~TestSearchController() = default;

void TestSearchController::ClearSearch() {
  if (!ash::IsZeroStateResultType(provider_->ResultType()))
    last_results_.clear();
  provider_->StopQuery();
}

void TestSearchController::StartSearch(const std::u16string& query) {
  // The search controller used when categorical search is enabled clears all
  // results when starging another search query - simulate this behavior in
  // tests when categorical search is enabled.
  if (!ash::IsZeroStateResultType(provider_->ResultType()))
    last_results_.clear();
  provider_->Start(query);
}

void TestSearchController::StartZeroState(base::OnceClosure on_done,
                                          base::TimeDelta timeout) {
  last_results_.clear();
  provider_->StartZeroState();
}

void TestSearchController::AppListClosing() {}

void TestSearchController::OpenResult(ChromeSearchResult* result,
                                      int event_flags) {}

void TestSearchController::InvokeResultAction(
    ChromeSearchResult* result,
    ash::SearchResultActionType action) {}

AppSearchDataSource* TestSearchController::GetAppSearchDataSource() {
  return nullptr;
}

void TestSearchController::AddProvider(
    std::unique_ptr<SearchProvider> provider) {
  DCHECK(!provider_);
  provider_ = std::move(provider);
  provider_->set_controller(this);
}

size_t TestSearchController::ReplaceProvidersForResultTypeForTest(
    ash::AppListSearchResultType result_type,
    std::unique_ptr<SearchProvider> provider) {
  NOTREACHED();
  return 0u;
}

void TestSearchController::SetResults(const SearchProvider* provider,
                                      Results results) {
  last_results_ = std::move(results);
  if (results_changed_callback_)
    results_changed_callback_.Run(provider->ResultType());
}

void TestSearchController::Publish() {}

ChromeSearchResult* TestSearchController::FindSearchResult(
    const std::string& result_id) {
  return nullptr;
}

ChromeSearchResult* TestSearchController::GetResultByTitleForTest(
    const std::string& title) {
  return nullptr;
}

void TestSearchController::Train(LaunchData&& launch_data) {}

void TestSearchController::AddObserver(Observer* observer) {}

void TestSearchController::RemoveObserver(Observer* observer) {}

std::u16string TestSearchController::get_query() {
  return u"";
}

base::Time TestSearchController::session_start() {
  return base::Time::Now();
}

void TestSearchController::set_results_changed_callback_for_test(
    ResultsChangedCallback callback) {
  results_changed_callback_ = callback;
}

void TestSearchController::disable_ranking_for_test() {}

void TestSearchController::WaitForZeroStateCompletionForTest(
    base::OnceClosure callback) {
  std::move(callback).Run();
}
}  // namespace app_list
