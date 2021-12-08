// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_
#define ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_

#include <string>
#include <vector>

#include "ash/ash_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"

namespace views {
class View;
}

namespace ash {
class AppsGridView;
class AppListModel;
class PaginationModel;

// Accesses ash data for app list view testing.
class ASH_EXPORT AppListTestApi {
 public:
  AppListTestApi();
  ~AppListTestApi();
  AppListTestApi(const AppListTestApi& other) = delete;
  AppListTestApi& operator=(const AppListTestApi& other) = delete;

  // Returns the active app list model.
  AppListModel* GetAppListModel();

  // Waits for the bubble launcher window to open on the primary display.
  // `wait_for_opening_animation` indicates whether to wait for the window
  // opening animation. See AppListBubblePresenter::Show(). Only used with
  // productivity launcher in clamshell mode.
  void WaitForBubbleWindow(bool wait_for_opening_animation);

  // Waits until all the animations on the app list widget end. No operations
  // if the app list widget is already idle.
  void WaitUntilAppListAnimationIdle();

  // Returns whether there is an item for |app_id|.
  bool HasApp(const std::string& app_id);

  // Returns ids of the items in top level app list view.
  std::vector<std::string> GetTopLevelViewIdList();

  // Creates a folder and moves all the apps in |apps| into that folder. Returns
  // the created folder id or empty string on error. Note that |apps| should
  // contains at least two items.
  std::string CreateFolderWithApps(const std::vector<std::string>& apps);

  // Returns the folder id that the app with |app_id| belongs to. Returns empty
  // string if the app is not in a folder.
  std::string GetFolderId(const std::string& app_id);

  // Returns IDs of all apps that belong to the folder with |folder_id|.
  std::vector<std::string> GetAppIdsInFolder(const std::string& folder_id);

  // Moves an item to position |to_index| within the item's item list. The item
  // can be a folder.
  void MoveItemToPosition(const std::string& item_id, const size_t to_index);

  // Adds one page break item after the item specified by `item_id`.
  void AddPageBreakItemAfterId(const std::string& item_id);

  // Returns the item count of the top list.
  int GetTopListItemCount();

  // Returns the last app list item view in the top level apps grid. Requires
  // the app list UI to be shown.
  views::View* GetLastItemInAppsGridView();

  // Returns the pagination model.
  PaginationModel* GetPaginationModel();

  // Updates the paged view structure.
  void UpdatePagedViewStructure();

  // Returns the top level apps grid view. Could be ScrollableAppsGridView if
  // bubble launcher is enabled or PagedAppsGridView otherwise.
  AppsGridView* GetTopLevelAppsGridView();

  // Returns the app list bubble's undo button that reverts the temporary
  // sorting order when triggered.
  views::View* GetBubbleReorderUndoButton();

  // Returns the visibility of the app list bubble's undo toast where the undo
  // button is located.
  bool GetBubbleReorderUndoToastVisibility() const;
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_
