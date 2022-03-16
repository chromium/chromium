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
  // `wait_for_opening_animation` indicates whether to wait for the bubble
  // launcher show animations (including the app list window animation, the
  // bubble apps page animation, the bubble view animation and apps grid
  // animation). Only used with productivity launcher in clamshell mode.
  void WaitForBubbleWindow(bool wait_for_opening_animation);

  // Waits until all the animations to show the app list become idle. No
  // operations if the app list is already idle.
  void WaitForAppListShowAnimation(bool is_bubble_window);

  // Returns whether there is an item for |app_id|.
  bool HasApp(const std::string& app_id);

  // Returns the name displayed in the launcher for the provided app list item.
  std::u16string GetAppListItemViewName(const std::string& item_id);

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
  const AppsGridView* GetTopLevelAppsGridView() const;

  // Returns the apps grid view in the folder.
  AppsGridView* GetFolderAppsGridView();

  // Returns whether the folder view is under animation.
  bool IsFolderViewAnimating() const;

  // Returns the app list bubble's undo button that reverts the temporary
  // sorting order when triggered.
  views::View* GetBubbleReorderUndoButton();

  // Returns the fullscreen app list's undo button that reverts the temporary
  // sorting order when triggered.
  views::View* GetFullscreenReorderUndoButton();

  // Returns the visibility of the app list bubble's undo toast where the undo
  // button is located.
  bool GetBubbleReorderUndoToastVisibility() const;

  // Returns the visibility of the fullscreen app list's undo toast where the
  // undo button is located.
  bool GetFullscreenReorderUndoToastVisibility() const;

  // Registers a callback that runs when all the animations scheduled to show or
  // hide the folder view complete.
  void SetFolderViewAnimationCallback(
      base::OnceClosure folder_animation_done_callback);

  // Returns the toast container view from either the bubble app list or the
  // fullscreen app list depending on which app list is being used. This method
  // assumes that the app list has been created.
  views::View* GetToastContainerView();

  // Adds a callback that runs at the end of the app list reorder. The callback
  // carries:
  // (1) A boolean parameter that is true if the reorder is aborted.
  // (2) An enum value that specifies the reorder animation status when the
  // callback runs.
  void AddReorderAnimationCallback(
      base::RepeatingCallback<void(bool, AppListReorderAnimationStatus)>
          callback);

  // Adds a callback that runs right after the app list fade out animation
  // triggered by reorder starts.
  void AddFadeOutAnimationStartClosure(base::OnceClosure closure);

  // Returns true if there is any waiting reorder animation test callback.
  bool HasAnyWaitingReorderDoneCallback() const;

  // Enables/Disables the app list nudge for testing.
  void DisableAppListNudge(bool disable);

  // Moves the app list item at `source_index` to `target_index` by
  // drag-and-drop. `source_index` and `target_index` are view indices in the
  // root apps grid.
  void ReorderItemInRootByDragAndDrop(int source_index, int target_index);
};

}  // namespace ash

#endif  // ASH_PUBLIC_CPP_TEST_APP_LIST_TEST_API_H_
