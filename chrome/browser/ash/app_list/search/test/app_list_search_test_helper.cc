// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/test/app_list_search_test_helper.h"
#include "base/memory/raw_ptr.h"

#include "base/run_loop.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/search/test/search_results_changed_waiter.h"

namespace app_list {

ResultsWaiter::ResultsWaiter(const std::u16string& query) : query_(query) {
  observer_.Observe(::test::GetAppListClient()->search_controller());
}

ResultsWaiter::~ResultsWaiter() = default;

void ResultsWaiter::OnResultsAdded(
    const std::u16string& query,
    const std::vector<KeywordInfo>& extracted_keyword_info,
    const std::vector<const ChromeSearchResult*>& results) {
  if (query != query_)
    return;
  observer_.Reset();
  run_loop_.Quit();
}

void ResultsWaiter::Wait() {
  run_loop_.Run();
}

AppListSearchBrowserTest::AppListSearchBrowserTest() {
  scoped_feature_list_.InitWithFeatures({ash::features::kHelpAppLauncherSearch},
                                        {});
}

void AppListSearchBrowserTest::SetUpOnMainThread() {
  InProcessBrowserTest::SetUpOnMainThread();
}

// The AppListClient is in charge of communication from ash to chrome, so can
// be used to mimic UI actions. Examples include starting a search, launching
// a result, or possibly activating a particular view.
AppListClientImpl* AppListSearchBrowserTest::GetClient() {
  auto* client = ::test::GetAppListClient();
  CHECK(client);
  return client;
}

void AppListSearchBrowserTest::StartSearch(const std::string& query) {
  GetClient()->StartSearch(base::ASCIIToUTF16(query));
}

void AppListSearchBrowserTest::SearchAndWaitForProviders(
    const std::string& query,
    const std::set<ResultType> providers) {
  // The waiter should be created before starting the search request, otherwise
  // it may miss synchronous result changes.
  SearchResultsChangedWaiter results_changed_waiter(
      GetClient()->search_controller(), providers);
  ResultsWaiter results_waiter(base::ASCIIToUTF16(query));
  StartSearch(query);
  results_changed_waiter.Wait();
  // Wait for some results to get published for the query - result publishing
  // may get delayed due to a burn in period.
  results_waiter.Wait();
}

std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
AppListSearchBrowserTest::PublishedResults() {
  return GetClient()
      ->GetModelUpdaterForTest()
      ->GetPublishedSearchResultsForTest();
}

std::vector<ChromeSearchResult*>
AppListSearchBrowserTest::PublishedResultsForProvider(
    const ResultType provider) {
  std::vector<ChromeSearchResult*> results;
  for (ChromeSearchResult* result : PublishedResults()) {
    if (result->result_type() == provider)
      results.push_back(result);
  }
  return results;
}

// Returns a search result for the given |id|, or nullptr if no matching
// search result exists.
ChromeSearchResult* AppListSearchBrowserTest::FindResult(
    const std::string& id) {
  for (ChromeSearchResult* result : PublishedResults()) {
    if (result->id() == id)
      return result;
  }
  return nullptr;
}

Profile* AppListSearchBrowserTest::GetProfile() {
  return browser()->profile();
}

}  // namespace app_list
