// Copyright 2013 The Chromium Authors
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
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "components/sync/model/string_ordinal.h"

namespace ash {

namespace test {
class AppsGridViewTest;
class AppListTestModel;
}  // namespace test

class AppListItem;
class AppListModelDelegate;

// Class to manage items in the app list. Used both by AppListModel and
// AppListFolderItem. Manages the position ordinal of items in the list, and
// notifies observers when items in the list are added / deleted / moved.
// TODO(https://crbug.com/1257605): make `AppListItemList` a consumer of items.
// If `AppListItemList` wants to trigger updates on items, such as moving an
// item, `AppListItemList` should always use `app_list_model_delegate_`.
class APP_LIST_MODEL_EXPORT AppListItemList {
 public:
  explicit AppListItemList(AppListModelDelegate* app_list_model_delegate);

  AppListItemList(const AppListItemList&) = delete;
  AppListItemList& operator=(const AppListItemList&) = delete;

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

  // Sets the position of `item` which is expected to be a member of
  // `app_list_items_` and sorts the list accordingly. Returns true if the index
  // of `item` in the sorted list changes after setting the position. If
  // `new_position` is invalid, move the item to the end of the list. This
  // method should not be called by `AppListItemList` itself. Because
  // `AppListItemList` is not the owner of app list item attributes (such as
  // item position) but the consumer. If `AppListItemList` wants to trigger the
  // update on app list item positions, it should always use the APIs provided
  // by `app_list_model_delegate_`.
  // TODO(crbug.com/40200822): It is confusing to have a method that
  // shares the similar functionality with a delegate but is only available to
  // external classes. Fixing this issue can eliminate such confusion.
  bool SetItemPosition(AppListItem* item, syncer::StringOrdinal new_position);

  AppListItem* item_at(size_t index) {
    DCHECK_LT(index, app_list_items_.size());
    return app_list_items_[index].get();
  }
  const AppListItem* item_at(size_t index) const {
    DCHECK_LT(index, app_list_items_.size());
    return app_list_items_[index].get();
  }
  size_t item_count() const { return app_list_items_.size(); }

  // For debugging only.
  std::string ToString();

 private:
  friend class AppListItemListTest;
  friend class AppListModel;
  friend class test::AppListTestModel;
  friend class test::AppsGridViewTest;

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

  // Used to initiate updates on app list item positions from the ash side.
  const raw_ptr<AppListModelDelegate> app_list_model_delegate_;

  std::vector<std::unique_ptr<AppListItem>> app_list_items_;
  base::ObserverList<AppListItemListObserver, true> observers_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_MODEL_APP_LIST_ITEM_LIST_H_
