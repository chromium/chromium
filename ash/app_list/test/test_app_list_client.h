// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_TEST_TEST_APP_LIST_CLIENT_H_
#define ASH_APP_LIST_TEST_TEST_APP_LIST_CLIENT_H_

#include <memory>
#include <string>
#include <utility>
#include <vector>

#include "ash/public/cpp/app_list/app_list_client.h"
#include "base/macros.h"

namespace ash {

// A test implementation of AppListClient that records function call counts.
// Registers itself as the presenter for the app list on construction.
class TestAppListClient : public AppListClient {
 public:
  TestAppListClient();
  ~TestAppListClient() override;

  // AppListClient:
  void OnAppListControllerDestroyed() override {}
  void StartSearch(const base::string16& trimmed_query) override {}
  void OpenSearchResult(const std::string& result_id,
                        int event_flags,
                        ash::AppListLaunchedFrom launched_from,
                        ash::AppListLaunchType launch_type,
                        int suggestion_index,
                        bool launch_as_default) override {}
  void InvokeSearchResultAction(const std::string& result_id,
                                int action_index,
                                int event_flags) override {}
  void GetSearchResultContextMenuModel(
      const std::string& result_id,
      GetContextMenuModelCallback callback) override;
  void ViewClosing() override {}
  void ViewShown(int64_t display_id) override {}
  void ActivateItem(int profile_id,
                    const std::string& id,
                    int event_flags) override {}
  void GetContextMenuModel(int profile_id,
                           const std::string& id,
                           GetContextMenuModelCallback callback) override;
  void OnAppListVisibilityWillChange(bool visible) override {}
  void OnAppListVisibilityChanged(bool visible) override {}
  void OnFolderCreated(int profile_id,
                       std::unique_ptr<AppListItemMetadata> item) override {}
  void OnFolderDeleted(int profile_id,
                       std::unique_ptr<AppListItemMetadata> item) override {}
  void OnItemUpdated(int profile_id,
                     std::unique_ptr<AppListItemMetadata> item) override {}
  void OnPageBreakItemAdded(int profile_id,
                            const std::string& id,
                            const syncer::StringOrdinal& position) override {}
  void OnPageBreakItemDeleted(int profile_id, const std::string& id) override {}
  void GetNavigableContentsFactory(
      mojo::PendingReceiver<content::mojom::NavigableContentsFactory> receiver)
      override {}
  void OnSearchResultVisibilityChanged(const std::string& id,
                                       bool visibility) override {}
  void NotifySearchResultsForLogging(
      const base::string16& trimmed_query,
      const ash::SearchResultIdWithPositionIndices& results,
      int position_index) override {}

 private:
  DISALLOW_COPY_AND_ASSIGN(TestAppListClient);
};

}  // namespace ash

#endif  // ASH_APP_LIST_TEST_TEST_APP_LIST_CLIENT_H_
