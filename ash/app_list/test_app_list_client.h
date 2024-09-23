// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_APP_LIST_CLIENT_H_
#define ASH_APP_LIST_TEST_APP_LIST_CLIENT_H_

#include <map>
#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/weak_ptr.h"
#include "components/account_id/account_id.h"

namespace ash {

// A test implementation of AppListClient that records function call counts.
class TestAppListClient : public AppListClient {
 public:
  TestAppListClient();

  TestAppListClient(const TestAppListClient&) = delete;
  TestAppListClient& operator=(const TestAppListClient&) = delete;

  ~TestAppListClient() override;

  // AppListClient:
  void OnAppListControllerDestroyed() override {}
  std::vector<AppListSearchControlCategory> GetToggleableCategories()
      const override;
  void StartZeroStateSearch(base::OnceClosure on_done,
                            base::TimeDelta timeout) override;
  void StartSearch(const std::u16string& trimmed_query) override;
  void OpenSearchResult(int profile_id,
                        const std::string& result_id,
                        int event_flags,
                        AppListLaunchedFrom launched_from,
                        AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override;
  void InvokeSearchResultAction(const std::string& result_id,
                                SearchResultActionType action) override;
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags,
                    ash::AppListLaunchedFrom launched_from,
                    bool is_above_the_fold) override;
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           AppListItemContext item_context,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override {}
  void OnAppListVisibilityChanged(bool visible) override {}
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override {}
  void OnQuickSettingsChanged(
      const std::string& setting_name,
      const std::map<std::string, int>& values) override {}
  AppListNotifier* GetNotifier() override;
  void RecalculateWouldTriggerLauncherSearchIph() override;
  std::unique_ptr<ScopedIphSession> CreateLauncherSearchIphSession() override;
  void LoadIcon(int profile_id, const std::string& app_id) override;
  ash::AppListSortOrder GetPermanentSortingOrder() const override;
  std::optional<bool> IsNewUser(const AccountId& account_id) const override;
  void RecordAppsDefaultVisibility(
      const std::vector<std::string>& apps_above_the_fold,
      const std::vector<std::string>& apps_below_the_fold,
      bool is_apps_collections_page) override;
  bool HasReordered() override;

  int start_zero_state_search_count() const {
    return start_zero_state_search_count_;
  }
  void set_run_zero_state_callback_immediately(bool value) {
    run_zero_state_callback_immediately_ = value;
  }
  int zero_state_search_done_count() const {
    return zero_state_search_done_count_;
  }
  void set_available_categories_for_test(
      const std::vector<AppListSearchControlCategory>& categories) {
    toggleable_categories_for_test_ = categories;
  }

  // Returns the number of AppItems that have been activated. These items could
  // live in search, RecentAppsView, or ScrollableAppsGridView.
  int activate_item_count() const { return activate_item_count_; }
  int activate_item_above_the_fold() const {
    return activate_item_above_the_fold_;
  }
  int activate_item_below_the_fold() const {
    return activate_item_below_the_fold_;
  }
  int items_above_the_fold_count() const { return items_above_the_fold_count_; }
  int items_below_the_fold_count() const { return items_below_the_fold_count_; }

  // Returns the ID of the last activated AppItem.
  std::string activate_item_last_id() const { return activate_item_last_id_; }

  // Returns the ID of the last opened SearchResult.
  std::string last_opened_search_result() const {
    return last_opened_search_result_;
  }

  std::vector<std::string> load_icon_app_ids() const {
    return loaded_icon_app_ids_;
  }

  using SearchResultActionId = std::pair<std::string, int>;
  std::vector<SearchResultActionId> GetAndResetInvokedResultActions();

  // Returns the list of search queries that were requested.
  // This clears the list of tracked queries - if the method gets called
  // consecutively, the second call will not return queries returned returned by
  // the first call.
  std::vector<std::u16string> GetAndResetPastSearchQueries();

  using SearchCallback =
      base::RepeatingCallback<void(const std::u16string& query)>;
  void set_search_callback(SearchCallback callback) {
    search_callback_ = std::move(callback);
  }

  void set_is_new_user(std::optional<bool> is_new_user) {
    is_new_user_ = is_new_user;
  }

 private:
  // Called in response to StartZeroStateSearch() when
  // `run_zero_state_callback_immediately_` is false. Counts calls via
  // `zero_state_done_count_` then invokes `on_done`.
  void OnZeroStateSearchDone(base::OnceClosure on_done);

  int start_zero_state_search_count_ = 0;
  bool run_zero_state_callback_immediately_ = true;
  int zero_state_search_done_count_ = 0;
  std::vector<std::u16string> search_queries_;
  std::vector<SearchResultActionId> invoked_result_actions_;
  int activate_item_count_ = 0;
  int activate_item_above_the_fold_ = 0;
  int items_above_the_fold_count_ = 0;
  int activate_item_below_the_fold_ = 0;
  int items_below_the_fold_count_ = 0;
  std::string activate_item_last_id_;
  std::string last_opened_search_result_;
  std::vector<std::string> loaded_icon_app_ids_;

  std::vector<AppListSearchControlCategory> toggleable_categories_for_test_;
  std::optional<bool> is_new_user_;

  // If not null, callback that will be run on each search request. It can be
  // used by tests to inject results to search model in response to search
  // queries.
  SearchCallback search_callback_;

  base::WeakPtrFactory<TestAppListClient> weak_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_CLIENT_H_
