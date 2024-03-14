// Copyright 2012 The Chromium Authors
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
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/scoped_multi_source_observation.h"

namespace ash {

class AppListFolderItem;
class AppListItem;
class AppListItemList;
class AppListModelObserver;
class AppListModelDelegate;

// Main model for the app list. Holds AppListItemList, which owns a list of
// AppListItems and is displayed in the grid view.
// NOTE: Currently this class observes |top_level_item_list_|. The View code may
// move entries in the item list directly (but can not add or remove them) and
// the model needs to notify its observers when this occurs.
class APP_LIST_MODEL_EXPORT AppListModel : public AppListItemListObserver {
 public:
  explicit AppListModel(AppListModelDelegate* app_list_model_delegate);

  AppListModel(const AppListModel&) = delete;
  AppListModel& operator=(const AppListModel&) = delete;

  ~AppListModel() override;

  void AddObserver(AppListModelObserver* observer);
  void RemoveObserver(AppListModelObserver* observer);

  void SetStatus(AppListModelStatus status);

  // Finds the item matching |id|.
  AppListItem* FindItem(const std::string& id);

  // Find a folder item matching |id|.
  AppListFolderItem* FindFolderItem(const std::string& id);

  // Creates and adds an empty folder item with the provided ID.
  AppListFolderItem* CreateFolderItem(const std::string& folder_id);

  // Adds |item| to the model. The model takes ownership of |item|. Returns a
  // pointer to the item that is safe to use (e.g. after passing ownership).
  AppListItem* AddItem(std::unique_ptr<AppListItem> item);

  // Adds |item| to an existing folder or creates a new folder. If |folder_id|
  // is empty, adds the item to the top level model instead. The model takes
  // ownership of |item|. Returns a pointer to the item that is safe to use.
  AppListItem* AddItemToFolder(std::unique_ptr<AppListItem> item,
                               const std::string& folder_id);

  // Updates an item's metadata (e.g. name, position, etc).
  void SetItemMetadata(const std::string& id,
                       std::unique_ptr<AppListItemMetadata> data);

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
  // This method should only be called by `AppListControllerImpl`. It does the
  // real job of reparenting an item. Note that `app_list_model_delegate_`
  // should be used to request for reparenting an item. We should not call
  // `MoveItemToFolder()` in ash directly.
  // TODO(https://crbug.com/1257605): it is confusing that when reparenting an
  // item, `MoveItemToFolder()` is called through an indirect way. The reason
  // leading to confusion is that `AppListModel` plays two roles: (1) the class
  // that sends the request for app list item change (2) the class that manages
  // app list items. To fix this issue, `AppListModel` code should be splitted
  // based on these two roles.
  void MoveItemToFolder(AppListItem* item, const std::string& folder_id);

  // Moves `item` to the top level. The item will be inserted before `position`
  // or at the end of the list if `position` is invalid. Note: `position` is
  // copied in case it refers to the containing folder which may get deleted.
  // See also the comment for RemoveItemFromFolder. Returns true if the item was
  // moved. NOTE: This should only be called by the View code (not the sync
  // code); it enforces folder restrictions (e.g. the source folder can not be
  // type OEM).
  bool MoveItemToRootAt(AppListItem* item, syncer::StringOrdinal position);

  // Sets the position of |item| either in |top_level_item_list_| or the
  // folder specified by |item|->folder_id(). If |new_position| is invalid,
  // move the item to the end of the list.
  void SetItemPosition(AppListItem* item,
                       const syncer::StringOrdinal& new_position);

  // Sets the name of |item| and notifies observers.
  void SetItemName(AppListItem* item, const std::string& name);

  // Sets the accessible name of |item| and notifies observers.
  void SetItemAccessibleName(AppListItem* item, const std::string& name);

  // Deletes the item matching |id| from |top_level_item_list_| or from the
  // appropriate folder.
  void DeleteItem(const std::string& id);

  AppListModelDelegate* delegate() { return delegate_; }

  AppListItemList* top_level_item_list() const {
    return top_level_item_list_.get();
  }

  AppListModelStatus status() const { return status_; }

 private:
  enum class ReparentItemReason {
    // Reparent an item when adding the item to the model.
    kAdd,

    // Reparent an item when updating the item in the model.
    kUpdate
  };

  // AppListItemListObserver
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

  // Adds |item_ptr| to |top_level_item_list_| and notifies observers.
  AppListItem* AddItemToRootListAndNotify(std::unique_ptr<AppListItem> item_ptr,
                                          ReparentItemReason reason);

  // Adds |item_ptr| to |folder| and notifies observers.
  AppListItem* AddItemToFolderListAndNotify(
      AppListFolderItem* folder,
      std::unique_ptr<AppListItem> item_ptr,
      ReparentItemReason reason);

  // Notifies observers of `item` being reparented.
  void NotifyItemParentChange(AppListItem* item, ReparentItemReason reason);

  // Removes `item` from the top item list.
  std::unique_ptr<AppListItem> RemoveFromTopList(AppListItem* item);

  // Removes `item` from its parent folder. If `destination_folder_id` is not
  // set, the removed item will be deleted; otherwise, move `item` to the top
  // list or a specified folder depending on `destination_folder_id`. If
  // the parent folder becomes empty after removal, deletes the folder from
  // `top_level_item_list_`. It is guaranteed that folder deletion is always
  // after moving or deleting `item`.
  void ReparentOrDeleteItemInFolder(
      AppListItem* item,
      std::optional<std::string> destination_folder_id);

  // Removes `item` from `folder` then returns a unique pointer to the removed
  // item.
  std::unique_ptr<AppListItem> RemoveItemFromFolder(AppListFolderItem* folder,
                                                    AppListItem* item);

  // Deletes folder with ID `folder_id` if it's empty.
  void DeleteFolderIfEmpty(const std::string& folder_id);

  // Sets the position of a root item.
  void SetRootItemPosition(AppListItem* item,
                           const syncer::StringOrdinal& new_position);

  // Used to initiate updates on app list items from the ash side.
  const raw_ptr<AppListModelDelegate> delegate_;

  std::unique_ptr<AppListItemList> top_level_item_list_;

  AppListModelStatus status_ = AppListModelStatus::kStatusNormal;

  base::ObserverList<AppListModelObserver, true> observers_;
  base::ScopedMultiSourceObservation<AppListItemList, AppListItemListObserver>
      item_list_scoped_observations_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_MODEL_H_
