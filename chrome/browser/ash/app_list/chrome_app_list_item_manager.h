// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_
#define CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_

#include <map>
#include <string>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/memory/raw_ptr.h"

class ChromeAppListItem;

// The class to manage chrome app list items and never talks to Ash.
class ChromeAppListItemManager {
 public:
  ChromeAppListItemManager();

  ChromeAppListItemManager(const ChromeAppListItemManager&) = delete;
  ChromeAppListItemManager& operator=(const ChromeAppListItemManager&) = delete;

  ~ChromeAppListItemManager();

  // Methods to find/add/update/remove an item.
  ChromeAppListItem* FindItem(const std::string& id);
  ChromeAppListItem* AddChromeItem(std::unique_ptr<ChromeAppListItem> app_item);
  void UpdateChromeItem(const std::string& id,
                        std::unique_ptr<ash::AppListItemMetadata>);
  void RemoveChromeItem(const std::string& id);

  // Implement `ChromeAppListModelUpdater` methods.
  size_t ItemCount() const;
  int BadgedItemCount() const;
  std::vector<ChromeAppListItem*> GetTopLevelItems() const;

  // Returns a position that is greater than all valid positions in `items_`.
  syncer::StringOrdinal CreateChromePositionOnLast() const;

  // Returns the child placed at the end of the specified folder's children
  // list. Copy the logic of `AppListItemList::CreatePositionBefore()` when
  // the parameter is an invalid position.
  // TODO(https://crbug.com/1252437): tune this comment when the app list sync
  // code is cleaned.
  ChromeAppListItem* FindLastChildInFolder(const std::string& folder_id);

  const auto& items() const { return items_; }

 private:
  friend class ChromeAppListItemManagerTest;

  // The methods to update `folder_item_mappings_`.
  void AddChildItemToFolderItemMapping(ChromeAppListItem* child_item,
                                       const std::string& dst_folder);
  void RemoveChildFromFolderItemMapping(ChromeAppListItem* child_item,
                                        const std::string& src_folder);

  // Ensures that `child_item` has valid position before adding to
  // `folder_item_mappings_`. `sorted_children` is the target folder's children
  // sorted with the increasing position order before adding `child_item`.
  // TODO(https://crbug.com/1252437): now this method is duplicate to
  // `AppListItemList::EnsureValidItemPosition()`. When the app list sync code
  // is cleaned, only this method should be kept.
  void EnsureChildItemValidPosition(
      ChromeAppListItem* child_item,
      const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>&
          sorted_children);

  // Returns the index at which to insert an item in `sorted_children` based on
  // `position` (which must be valid) and `id` (if the positions are equal).
  // TODO(https://crbug.com/1252437): now this method is duplicate to
  // `AppListItemList::GetItemSortOrderIndex()`. When the app list sync code is
  // cleaned, only this method should be kept.
  size_t GetItemSortOrderIndex(
      ChromeAppListItem* child_item,
      const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>&
          sorted_children);

  // A map from a `ChromeAppListItem`'s id to its unique pointer. This item set
  // matches the one in `AppListModel`.
  std::map<std::string, std::unique_ptr<ChromeAppListItem>> items_;

  // For a key-value pair, the key is a folder id while the value is a list of
  // app list items under the folder indexed by the key. Note that the list
  // maintains the item position increasing order.
  std::map<std::string,
           std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>>
      folder_item_mappings_;
};

#endif  // CHROME_BROWSER_ASH_APP_LIST_CHROME_APP_LIST_ITEM_MANAGER_H_
