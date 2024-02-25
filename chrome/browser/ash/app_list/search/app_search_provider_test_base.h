// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_TEST_BASE_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_TEST_BASE_H_

#include <memory>
#include <string>

#include "base/files/scoped_temp_dir.h"
#include "base/memory/raw_ptr.h"
#include "base/test/simple_test_clock.h"
#include "chrome/browser/ash/app_list/app_list_test_util.h"
#include "chrome/browser/ash/app_list/arc/arc_app_test.h"
#include "chrome/browser/ash/app_list/search/chrome_search_result.h"
#include "extensions/common/mojom/manifest.mojom.h"

namespace test {
class TestAppListControllerDelegate;
}

namespace app_list {

class AppSearchDataSource;
class SearchProvider;
class TestSearchController;

// Base class for app search and app zero state provider unit tests.
class AppSearchProviderTestBase : public AppListTestBase {
 public:
  explicit AppSearchProviderTestBase(bool zero_state_provider);

  AppSearchProviderTestBase(const AppSearchProviderTestBase&) = delete;
  AppSearchProviderTestBase& operator=(const AppSearchProviderTestBase&) =
      delete;

  ~AppSearchProviderTestBase() override;

  // AppListTestBase overrides:
  void SetUp() override;

  //  Sets up app search provider to be used in the test.
  void InitializeSearchProvider();

  // Runs a search for using `query`.
  std::string RunQuery(const std::string& query);

  // Runs zero state search.
  std::string RunZeroStateSearch();

  // Clears search state for the last query.
  void ClearSearch();

  // Returns list of result IDs sorted by their relevance.
  std::string GetSortedResultsString();

  // Returns list of results.
  std::vector<ChromeSearchResult*> GetLastResults();

  // Installs a test ARC app.
  std::string AddArcApp(const std::string& name,
                        const std::string& package,
                        const std::string& activity,
                        bool sticky = false);

  // Installs a test extension.
  void AddExtension(const std::string& id,
                    const std::string& name,
                    extensions::mojom::ManifestLocation location,
                    int init_from_value_flags,
                    bool display_in_launcher = true);

  // Notifies search providers that app list view is closing (simulating a call
  // that happens when app list closes, or gets hidden in tablet mode).
  void CallViewClosing();

  // Waits for base::Time::Now() is updated.
  void WaitTimeUpdated();

  ArcAppTest& arc_test() { return arc_test_; }

 private:
  // Whether the test is testing zero state, or queried apps search provider.
  const bool zero_state_provider_;

  base::SimpleTestClock clock_;
  base::ScopedTempDir temp_dir_;
  std::unique_ptr<TestSearchController> search_controller_;
  std::unique_ptr<AppSearchDataSource> data_source_;
  raw_ptr<SearchProvider, DanglingUntriaged> app_search_ = nullptr;
  std::unique_ptr<::test::TestAppListControllerDelegate> controller_;
  ArcAppTest arc_test_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_APP_SEARCH_PROVIDER_TEST_BASE_H_
