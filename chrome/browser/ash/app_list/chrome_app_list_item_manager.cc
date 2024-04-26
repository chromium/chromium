// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/chrome_app_list_item_manager.h"

#include "base/memory/raw_ptr.h"
#include "base/ranges/algorithm.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

ChromeAppListItemManager::ChromeAppListItemManager() = default;

ChromeAppListItemManager::~ChromeAppListItemManager() = default;

ChromeAppListItem* ChromeAppListItemManager::FindItem(const std::string& id) {
  auto iter = items_.find(id);
  return iter != items_.end() ? iter->second.get() : nullptr;
}

ChromeAppListItem* ChromeAppListItemManager::AddChromeItem(
    std::unique_ptr<ChromeAppListItem> app_item) {
  ChromeAppListItem* item = app_item.get();
  items_[item->id()] = std::move(app_item);

  if (item->is_folder()) {
    folder_item_mappings_.emplace(
        item->id(),
        std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>());
  } else if (!item->folder_id().empty()) {
    AddChildItemToFolderItemMapping(item, item->folder_id());
  }

  return item;
}

void ChromeAppListItemManager::UpdateChromeItem(
    const std::string& id,
    std::unique_ptr<ash::AppListItemMetadata> data) {
  ChromeAppListItem* item = FindItem(id);

  // The old metadata is destroyed after setting the new data. Therefore copy
  // the old data.
  const std::string old_folder = item->folder_id();
  const syncer::StringOrdinal old_position = item->position();

  item->SetMetadata(std::move(data));

  const std::string& new_folder = item->folder_id();
  if (old_folder != new_folder) {
    if (!old_folder.empty())
      RemoveChildFromFolderItemMapping(item, old_folder);
    if (!new_folder.empty())
      AddChildItemToFolderItemMapping(item, new_folder);

    // The new position is taken into consideration when adding an item to
    // folder so nothing to do.
    return;
  }

  const syncer::StringOrdinal& new_positon = item->position();
  if (old_position.IsValid() && new_positon.IsValid() &&
      old_position.Equals(new_positon)) {
    return;
  }

  if (new_folder.empty())
    return;

  // Remove `item` from the sorted children list then add it back to ensure that
  // `item` is placed in the sorted list correctly after position update.
  // TODO(crbug.com/40203095): if `new_position` is always valid, clean
  // this code by using a function that moves an item in the sorted list.
  DCHECK(old_position.IsValid());
  RemoveChildFromFolderItemMapping(item, new_folder);
  AddChildItemToFolderItemMapping(item, new_folder);
}

void ChromeAppListItemManager::RemoveChromeItem(const std::string& id) {
  auto* item = FindItem(id);
  DCHECK(item);

  if (item->is_folder()) {
    auto iter = folder_item_mappings_.find(id);
    DCHECK(iter != folder_item_mappings_.end());
    DCHECK(iter->second.empty());
    folder_item_mappings_.erase(iter);
  } else if (!item->folder_id().empty()) {
    RemoveChildFromFolderItemMapping(item, item->folder_id());
  }

  items_.erase(id);
}

size_t ChromeAppListItemManager::ItemCount() const {
  return items_.size();
}

int ChromeAppListItemManager::BadgedItemCount() const {
  size_t count = 0u;
  for (const auto& key_val : items_) {
    if (key_val.second->IsBadged())
      ++count;
  }
  return count;
}

std::vector<ChromeAppListItem*> ChromeAppListItemManager::GetTopLevelItems()
    const {
  std::vector<ChromeAppListItem*> top_level_items;
  for (auto& entry : items_) {
    ChromeAppListItem* item = entry.second.get();
    DCHECK(item->position().IsValid())
        << "Item with invalid position: id=" << item->id()
        << ", name=" << item->name() << ", is_folder=" << item->is_folder();
    if (item->folder_id().empty() && item->position().IsValid())
      top_level_items.emplace_back(item);
  }
  return top_level_items;
}

syncer::StringOrdinal ChromeAppListItemManager::CreateChromePositionOnLast()
    const {
  syncer::StringOrdinal last_known_position;
  for (auto& it : items_) {
    if (!last_known_position.IsValid() ||
        (it.second->position().IsValid() &&
         it.second->position().GreaterThan(last_known_position))) {
      last_known_position = it.second->position();
    }
  }
  return last_known_position.IsValid()
             ? last_known_position.CreateAfter()
             : syncer::StringOrdinal::CreateInitialOrdinal();
}

ChromeAppListItem* ChromeAppListItemManager::FindLastChildInFolder(
    const std::string& folder_id) {
  auto iter = folder_item_mappings_.find(folder_id);
  DCHECK(iter != folder_item_mappings_.end());
  const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>&
      sorted_children = iter->second;

  if (sorted_children.empty())
    return nullptr;

  return sorted_children.back();
}

void ChromeAppListItemManager::AddChildItemToFolderItemMapping(
    ChromeAppListItem* child_item,
    const std::string& dst_folder) {
  DCHECK(!dst_folder.empty());

  // Find the target folder's children.
  auto iter = folder_item_mappings_.find(dst_folder);
  DCHECK(iter != folder_item_mappings_.end());
  std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>*
      sorted_children_ptr = &iter->second;

  EnsureChildItemValidPosition(child_item, *sorted_children_ptr);
  size_t target_index = GetItemSortOrderIndex(child_item, *sorted_children_ptr);
  sorted_children_ptr->insert(sorted_children_ptr->begin() + target_index,
                              child_item);
}

void ChromeAppListItemManager::RemoveChildFromFolderItemMapping(
    ChromeAppListItem* child_item,
    const std::string& src_folder) {
  DCHECK(!src_folder.empty());

  // Find the source folder's children.
  auto folder_item_mappings_iter = folder_item_mappings_.find(src_folder);
  DCHECK(folder_item_mappings_iter != folder_item_mappings_.end());
  std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>*
      sorted_children_ptr = &folder_item_mappings_iter->second;

  auto children_array_iter =
      base::ranges::find(*sorted_children_ptr, child_item);
  DCHECK(children_array_iter != sorted_children_ptr->cend());

  // Delete `child_item` from `src_folder`'s children list.
  sorted_children_ptr->erase(children_array_iter);
}

void ChromeAppListItemManager::EnsureChildItemValidPosition(
    ChromeAppListItem* child_item,
    const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>&
        sorted_children) {
  syncer::StringOrdinal position = child_item->position();
  if (position.IsValid())
    return;
  size_t nitems = sorted_children.size();
  if (nitems == 0) {
    position = syncer::StringOrdinal::CreateInitialOrdinal();
  } else {
    position = sorted_children.back()->position().CreateAfter();
  }
  child_item->SetChromePosition(position);
}

size_t ChromeAppListItemManager::GetItemSortOrderIndex(
    ChromeAppListItem* child_item,
    const std::vector<raw_ptr<ChromeAppListItem, VectorExperimental>>&
        sorted_children) {
  const syncer::StringOrdinal& position = child_item->position();
  const std::string& id = child_item->id();

  DCHECK(position.IsValid());
  for (size_t index = 0; index < sorted_children.size(); ++index) {
    if (position.LessThan(sorted_children[index]->position()) ||
        (position.Equals(sorted_children[index]->position()) &&
         (id < sorted_children[index]->id()))) {
      return index;
    }
  }
  return sorted_children.size();
}
