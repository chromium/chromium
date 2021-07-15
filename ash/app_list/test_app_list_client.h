// Copyright 2018 The Chromium Authors. All rights reserved.
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
#include "base/macros.h"

namespace ash {

// A test implementation of AppListClient that records function call counts.
class TestAppListClient : public AppListClient {
 public:
  TestAppListClient();
  ~TestAppListClient() override;

  // AppListClient:
  void OnAppListControllerDestroyed() override {}
  void StartSearch(const std::u16string& trimmed_query) override;
  void OpenSearchResult(int profile_id,
                        const std::string& result_id,
                        AppListSearchResultType result_type,
                        int event_flags,
                        AppListLaunchedFrom launched_from,
                        AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override {}
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index) override;
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewClosing() override {}
  void ViewShown(int64_t display_id) override {}
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override;
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override {}
  void OnAppListVisibilityChanged(bool visible) override {}
  void OnItemAdded(int profile_id,
                   std::unique_ptr<AppListItemMetadata> item) override {}
  void OnItemUpdated(int profile_id,
                     std::unique_ptr<AppListItemMetadata> item) override {}
  void OnFolderDeleted(int profile_id,
                       std::unique_ptr<AppListItemMetadata> item) override {}
  void OnPageBreakItemDeleted(int profile_id, const std::string& id) override {}
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override {}
  void OnQuickSettingsChanged(
      const std::string& setting_name,
      const std::map<std::string, int>& values) override {}
  void NotifySearchResultsForLogging(
      const std::u16string& trimmed_query,
      const SearchResultIdWithPositionIndices& results,
      int position_index) override {}
  AppListNotifier* GetNotifier() override;

  std::u16string last_search_query() { return last_search_query_; }

  // Returns the number of AppItems that have been activated. These items could
  // live in search, RecentAppsView, or ScrollableAppsGridView.
  int activate_item_count() const { return activate_item_count_; }

  // Returns the ID of the last activated AppItem.
  std::string activate_item_last_id() const { return activate_item_last_id_; }

  using SearchResultActionId = std::pair<std::string, int>;
  std::vector<SearchResultActionId> GetAndClearInvokedResultActions();

 private:
  std::u16string last_search_query_;
  std::vector<SearchResultActionId> invoked_result_actions_;
  int activate_item_count_ = 0;
  std::string activate_item_last_id_;

  DISALLOW_COPY_AND_ASSIGN(TestAppListClient);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_APP_LIST_CLIENT_H_
