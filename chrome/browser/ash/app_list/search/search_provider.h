// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/callback_list.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/types.h"

class ChromeSearchResult;

namespace ash {

enum class AppListSearchResultType;

}  // namespace ash

namespace app_list {

// This enum scopes search providers into logically connected groups that
// a client expect to search together. This category do not have to be unique
// for each provider.
enum class SearchCategory {
  kTest = 0,
  kApps = 1,
  kAppShortcuts = 2,
  kFiles = 3,
  kGames = 4,
  kHelp = 5,
  kImages = 6,
  kPlayStore = 7,
  kWeb = 8,
  kSettings = 9,
  kOmnibox = 10,
  kDesksAdmin = 11,
  kAssistant = 12,
  kSystemInfoCard = 13
};

class SearchProvider {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;
  using OnSearchResultsCallback =
      base::RepeatingCallback<void(ash::AppListSearchResultType, Results)>;

  // Each provider should assign its search category during construction to
  // indicate the type of search results.
  explicit SearchProvider(SearchCategory search_category);

  SearchProvider(const SearchProvider&) = delete;
  SearchProvider& operator=(const SearchProvider&) = delete;

  virtual ~SearchProvider();

  // Invoked to start a query search. |query| is guaranteed to be non-empty.
  virtual void Start(const std::u16string& query,
                     OnSearchResultsCallback on_search_done);

  // Called when search query is cleared. The provider should stop/cancel
  // any pending search query handling. This should not affect zero state
  // search.
  virtual void StopQuery() {}

  // Invoked to start a zero-state search.
  virtual void StartZeroState(OnSearchResultsCallback on_search_done);

  // Invoked to cancel zero-state search - called when app list view gets
  // hidden.
  virtual void StopZeroState() {}

  // Handles training signals if necessary. A given |SearchProvider| may receive
  // training signals for results of any |AppListSearchResultType|, so it is the
  // |SearchProvider|'s responsibility to check |type| and ignore if necessary.
  virtual void Train(const std::string& id, ash::AppListSearchResultType type) {
  }
  // Returns the main result type created by this provider.
  virtual ash::AppListSearchResultType ResultType() const = 0;

  // Returns the launcher search category of this provider.
  SearchCategory search_category() const { return search_category_; }

 protected:
  // Swaps the internal results with |new_results|.
  // This is useful when multiple results will be added, and the notification is
  // desired to be done only once when all results are added.
  // TODO(b/315709613): Deprecated. To be removed. Use `on_search_done_`
  // directly.
  void SwapResults(Results* new_results);

  // A callback to be called when a search is done.
  OnSearchResultsCallback on_search_done_;

 private:
  // TODO(b/315709613): Deprecated. To be removed.
  virtual void Start(const std::u16string& query) {}

  // TODO(b/315709613): Deprecated. To be removed.
  virtual void StartZeroState() {}

  // Indicates what kind of search the provider does.
  const SearchCategory search_category_;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
