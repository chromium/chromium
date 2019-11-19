// Copyright (c) 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_H_
#define ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_H_

#include <stddef.h>

#include <memory>
#include <string>
#include <vector>

#include "ash/app_list/model/app_list_item_list_observer.h"
#include "ash/app_list/model/app_list_model_export.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"

namespace ash {

class AppListItem;

// Class to manage items in the app list. Used both by AppListModel and
// AppListFolderItem. Manages the position ordinal of items in the list, and
// notifies observers when items in the list are added / deleted / moved.
class APP_LIST_MODEL_EXPORT AppListItemList {
 public:
  AppListItemList();
  virtual ~AppListItemList();

  void AddObserver(AppListItemListObserver* observer);
  void RemoveObserver(AppListItemListObserver* observer);

  // Finds item matching |id|. NOTE: Requires a linear search.
  // Generally this should not be used directly, AppListModel::FindItem
  // should be used instead.
  AppListItem* FindItem(const std::string& id);

  // Finds the |index| of the the item matching |id| in |app_list_items_|.
  // Returns true if the matching item is found.
  // Note: Requires a linear search.
  bool FindItemIndex(const std::string& id, size_t* index);

  // Moves item at |from_index| to |to_index|.
  // Triggers observers_.OnListItemMoved().
  void MoveItem(size_t from_index, size_t to_index);

  // Sets the position of |item| which is expected to be a member of
  // |app_list_items_| and sorts the list accordingly. If |new_position| is
  // invalid, move the item to the end of the list.
  void SetItemPosition(AppListItem* item, syncer::StringOrdinal new_position);

  // Add a "page break" item right after the specified item in item list.
  AppListItem* AddPageBreakItemAfter(const AppListItem* previous_item);

  // Highlights the given item in the app list. If not present and it is later
  // added, the item will be highlighted after being added.
  void HighlightItemInstalledFromUI(const std::string& id);

  AppListItem* item_at(size_t index) {
    DCHECK_LT(index, app_list_items_.size());
    return app_list_items_[index].get();
  }
  const AppListItem* item_at(size_t index) const {
    DCHECK_LT(index, app_list_items_.size());
    return app_list_items_[index].get();
  }
  size_t item_count() const { return app_list_items_.size(); }

 private:
  friend class AppListItemListTest;
  friend class AppListModel;

  // Returns a unique, valid StringOrdinal immediately before |position| or at
  // the end of the list if |position| is invalid.
  syncer::StringOrdinal CreatePositionBefore(
      const syncer::StringOrdinal& position);

  // Adds |item| to the end of |app_list_items_|. Takes ownership of |item|.
  // Triggers observers_.OnListItemAdded(). Returns a pointer to the added item
  // that is safe to use (e.g. after releasing a scoped ptr).
  AppListItem* AddItem(std::unique_ptr<AppListItem> item_ptr);

  // Finds item matching |id| in |app_list_items_| (linear search) and deletes
  // it. Triggers observers_.OnListItemRemoved() after removing the item from
  // the list and before deleting it.
  void DeleteItem(const std::string& id);

  // Removes the item with matching |id| in |app_list_items_| without deleting
  // it. Returns a scoped pointer containing the removed item.
  std::unique_ptr<AppListItem> RemoveItem(const std::string& id);

  // Removes the item at |index| from |app_list_items_| without deleting it.
  // Returns a scoped pointer containing the removed item.
  std::unique_ptr<AppListItem> RemoveItemAt(size_t index);

  // Deletes item at |index| and signals observers.
  void DeleteItemAt(size_t index);

  // If |item|->position() is not a valid ordinal, sets |item|->position()
  // to a valid ordinal after the last item in the list.
  void EnsureValidItemPosition(AppListItem* item);

  // Returns the index at which to insert an item in |app_list_items_| based on
  // |position| (which must be valid) and |id| (if the positions are equal).
  size_t GetItemSortOrderIndex(const syncer::StringOrdinal& position,
                               const std::string& id);

  // Fixes the position of the item at |index| when the position matches the
  // previous item's position. |index| must be > 0.
  void FixItemPosition(size_t index);

  std::vector<std::unique_ptr<AppListItem>> app_list_items_;
  base::ObserverList<AppListItemListObserver, true>::Unchecked observers_;
  std::string highlighted_id_;

  DISALLOW_COPY_AND_ASSIGN(AppListItemList);
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_H_
