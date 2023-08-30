// Copyright 2014 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
#define CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_

#include <memory>
#include <string>
#include <vector>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "chrome/browser/ash/app_list/search/types.h"

class ChromeSearchResult;

namespace ash {

enum class AppListSearchResultType;

}  // namespace ash

namespace app_list {

class SearchController;

class SearchProvider {
 public:
  using Results = std::vector<std::unique_ptr<ChromeSearchResult>>;

  // Each provider should assign its control category during construction to
  // indicate whether or not they need a control to disable themselves. The
  // default value `kCannotToggle` means it is non-toggleable and should always
  // provide results for search.
  explicit SearchProvider(
      ControlCategory control_category = ControlCategory::kCannotToggle);

  SearchProvider(const SearchProvider&) = delete;
  SearchProvider& operator=(const SearchProvider&) = delete;

  virtual ~SearchProvider();

  // Invoked to start a query search. |query| is guaranteed to be non-empty.
  virtual void Start(const std::u16string& query) {}

  // Called when search query is cleared. The provider should stop/cancel
  // any pending search query handling. This should not affect zero state
  // search.
  virtual void StopQuery() {}

  // Invoked to start a zero-state search.
  virtual void StartZeroState() {}

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

  void set_controller(SearchController* controller) {
    search_controller_ = controller;
  }

  // Returns the launcher search control category of this provider.
  ControlCategory control_category() const { return control_category_; }

 protected:
  // Swaps the internal results with |new_results|.
  // This is useful when multiple results will be added, and the notification is
  // desired to be done only once when all results are added.
  void SwapResults(Results* new_results);

  // The control category setters should be called in derived class constructor
  // only.
  void set_control_category(ControlCategory control_category) {
    control_category_ = control_category;
  }

 private:
  raw_ptr<SearchController, ExperimentalAsh> search_controller_ = nullptr;
  // The launcher search control category of the provider. Each provider is
  // enabled by default.
  ControlCategory control_category_ = ControlCategory::kCannotToggle;
};

}  // namespace app_list

#endif  // CHROME_BROWSER_ASH_APP_LIST_SEARCH_SEARCH_PROVIDER_H_
