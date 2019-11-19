// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_
#define ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item_list.h"
#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model_export.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/macros.h"
#include "base/observer_list.h"

namespace ash {

class AppListFolderItem;
class AppListItem;
class AppListItemList;
class AppListModelObserver;

// Master model of app list that holds AppListItemList, which owns a list
// of AppListItems and is displayed in the grid view.
// NOTE: Currently this class observes |top_level_item_list_|. The View code may
// move entries in the item list directly (but can not add or remove them) and
// the model needs to notify its observers when this occurs.
class APP_LIST_MODEL_EXPORT AppListModel : public AppListItemListObserver {
 public:
  AppListModel();
  ~AppListModel() override;

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(ash::AppListModelStatus status);

  void SetState(ash::AppListState state);
  ash::AppListState state() const { return state_; }

  // The current state of the AppListView. Controlled by AppListView.
  void SetStateFullscreen(ash::AppListViewState state);
  ash::AppListViewState state_fullscreen() const { return state_fullscreen_; }

  // Finds the item matching |id|.
  AppListItem* FindItem(const std::string& id);

  // Find a folder item matching |id|.
  AppListFolderItem* FindFolderItem(const std::string& id);

  // Adds |item| to the model. The model takes ownership of |item|. Returns a
  // pointer to the item that is safe to use (e.g. after passing ownership).
  AppListItem* AddItem(std::unique_ptr<AppListItem> item);

  // Adds |item| to an existing folder or creates a new folder. If |folder_id|
  // is empty, adds the item to the top level model instead. The model takes
  // ownership of |item|. Returns a pointer to the item that is safe to use.
  AppListItem* AddItemToFolder(std::unique_ptr<AppListItem> item,
                               const std::string& folder_id);

  // Add a "page break" item right after the specified item in item list.
  void AddPageBreakItemAfter(const AppListItem* previous_item);

  // Merges two items. If the target item is a folder, the source item is
  // added to the end of the target folder. Otherwise a new folder is created
  // in the same position as the target item with the target item as the first
  // item in the new folder and the source item as the second item. Returns
  // the id of the target folder, or an empty string if the merge failed. The
  // source item may already be in a folder. See also the comment for
  // RemoveItemFromFolder. NOTE: This should only be called by the View code
  // (not the sync code); it enforces folder restrictions (e.g. the target can
  // not be an OEM folder).
  const std::string MergeItems(const std::string& target_item_id,
                               const std::string& source_item_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. |item|->position will determine where the item
  // is positioned. See also the comment for RemoveItemFromFolder.
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Move |item| to the folder matching |folder_id| or to the top level if
  // |folder_id| is empty. The item will be inserted before |position| or at
  // the end of the list if |position| is invalid. Note: |position| is copied
  // in case it refers to the containing folder which may get deleted. See
  // also the comment for RemoveItemFromFolder. Returns true if the item was
  // moved. NOTE: This should only be called by the View code (not the sync
  // code); it enforces folder restrictions (e.g. the source folder can not be
  // type OEM).
  bool MoveItemToFolderAt(AppListItem* item,
                          const std::string& folder_id,
                          syncer::StringOrdinal position);

  // Sets the position of |item| either in |top_level_item_list_| or the
  // folder specified by |item|->folder_id(). If |new_position| is invalid,
  // move the item to the end of the list.
  void SetItemPosition(AppListItem* item,
                       const syncer::StringOrdinal& new_position);

  // Sets the name of |item| and notifies observers.
  void SetItemName(AppListItem* item, const std::string& name);

  // Sets the name and short name of |item| and notifies observers.
  void SetItemNameAndShortName(AppListItem* item,
                               const std::string& name,
                               const std::string& short_name);

  // Deletes the item matching |id| from |top_level_item_list_| or from the
  // appropriate folder.
  void DeleteItem(const std::string& id);

  // Wrapper around DeleteItem() which will also clean up if its parent folder
  // has a single child left.
  void DeleteUninstalledItem(const std::string& id);

  // Deletes all items. This is used in profile switches.
  void DeleteAllItems();

  AppListItemList* top_level_item_list() { return top_level_item_list_.get(); }

  ash::AppListModelStatus status() const { return status_; }

 private:
  // AppListItemListObserver
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

  // Returns an existing folder matching |folder_id| or creates a new folder.
  AppListFolderItem* FindOrCreateFolderItem(const std::string& folder_id);

  // Adds |item_ptr| to |top_level_item_list_| and notifies observers.
  AppListItem* AddItemToItemListAndNotify(
      std::unique_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |top_level_item_list_| and notifies observers that an
  // Update occured (e.g. item moved from a folder).
  AppListItem* AddItemToItemListAndNotifyUpdate(
      std::unique_ptr<AppListItem> item_ptr);

  // Adds |item_ptr| to |folder| and notifies observers.
  AppListItem* AddItemToFolderItemAndNotify(
      AppListFolderItem* folder,
      std::unique_ptr<AppListItem> item_ptr);

  // Removes |item| from |top_level_item_list_| or calls RemoveItemFromFolder
  // if |item|->folder_id is set.
  std::unique_ptr<AppListItem> RemoveItem(AppListItem* item);

  // Removes |item| from |folder|. If |folder| becomes empty, deletes |folder|
  // from |top_level_item_list_|. Does NOT trigger observers, calling function
  // must do so.
  std::unique_ptr<AppListItem> RemoveItemFromFolder(AppListFolderItem* folder,
                                                    AppListItem* item);

  std::unique_ptr<AppListItemList> top_level_item_list_;

  ash::AppListModelStatus status_ = ash::AppListModelStatus::kStatusNormal;
  ash::AppListState state_ = ash::AppListState::kInvalidState;
  // The AppListView state. Controlled by the AppListView.
  ash::AppListViewState state_fullscreen_ = ash::AppListViewState::kClosed;
  base::ObserverList<AppListModelObserver, true>::Unchecked observers_;

  DISALLOW_COPY_AND_ASSIGN(AppListModel);
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_
