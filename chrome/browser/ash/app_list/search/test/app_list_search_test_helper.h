// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_APP_LIST_SEARCH_TEST_HELPER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_APP_LIST_SEARCH_TEST_HELPER_H_

#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "ash/public/cpp/app_list/app_list_metrics.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/files/file_util.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/notreached.h"
#include "base/run_loop.h"
#include "base/strings/utf_string_conversions.h"
#include "base/test/bind.h"
#include "base/test/metrics/histogram_tester.h"
#include "base/test/scoped_feature_list.h"
#include "base/threading/thread_restrictions.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "chrome/browser/ash/app_list/search/search_controller.h"
#include "chrome/browser/ash/app_list/test/chrome_app_list_test_support.h"
#include "chrome/browser/ash/file_manager/path_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/browser/ui/browser_finder.h"
#include "chrome/common/pref_names.h"
#include "chrome/test/base/in_process_browser_test.h"
#include "components/prefs/pref_service.h"
#include "content/public/test/browser_test.h"
#include "content/public/test/test_navigation_observer.h"
#include "ui/base/l10n/l10n_util.h"
#include "ui/events/event_constants.h"

namespace app_list {

class ResultsWaiter : public SearchController::Observer {
 public:
  explicit ResultsWaiter(const std::u16string& query);
  ~ResultsWaiter() override;

  void OnResultsAdded(
      const std::u16string& query,
      const std::vector<KeywordInfo>& extracted_keyword_info,
      const std::vector<const ChromeSearchResult*>& results) override;
  void Wait();

 private:
  const std::u16string query_;
  base::RunLoop run_loop_;
  base::ScopedObservation<SearchController, SearchController::Observer>
      observer_{this};
};

// This contains almost end-to-end tests for the launcher search backend. It is
// set up to simulate user input by calls to the AppListClient, and observe the
// results that would be displayed via the AppListModelUpdater.
class AppListSearchBrowserTest : public InProcessBrowserTest {
 public:
  using ResultType = ash::AppListSearchResultType;
  using DisplayType = ash::SearchResultDisplayType;

  AppListSearchBrowserTest();
  ~AppListSearchBrowserTest() override = default;

  AppListSearchBrowserTest(const AppListSearchBrowserTest&) = delete;
  AppListSearchBrowserTest& operator=(const AppListSearchBrowserTest&) = delete;

  // InProcessBrowserTest:
  void SetUpOnMainThread() override;

  //---------------
  // Search helpers
  //---------------

  // The AppListClient is in charge of communication from ash to chrome, so can
  // be used to mimic UI actions. Examples include starting a search, launching
  // a result, or possibly activating a particular view.
  AppListClientImpl* GetClient();

  void StartSearch(const std::string& query);

  void SearchAndWaitForProviders(const std::string& query,
                                 const std::set<ResultType> providers);

  std::vector<raw_ptr<ChromeSearchResult, VectorExperimental>>
  PublishedResults();

  std::vector<ChromeSearchResult*> PublishedResultsForProvider(
      const ResultType provider);

  // Returns a search result for the given |id|, or nullptr if no matching
  // search result exists.
  ChromeSearchResult* FindResult(const std::string& id);

  //----------------
  // Session helpers
  //----------------

  Profile* GetProfile();

 private:
  base::test::ScopedFeatureList scoped_feature_list_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_TEST_APP_LIST_SEARCH_TEST_HELPER_H_
