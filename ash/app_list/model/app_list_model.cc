// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_model.h"

#include <string>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model_observer.h"

namespace ash {

AppListModel::AppListModel()
    : top_level_item_list_(std::make_unique<AppListItemList>()) {
  top_level_item_list_->AddObserver(this);
}

AppListModel::~AppListModel() {
  top_level_item_list_->RemoveObserver(this);
}

void AppListModel::AddObserver(AppListModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListModel::RemoveObserver(AppListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListModel::SetStatus(ash::AppListModelStatus status) {
  if (status_ == status)
    return;

  status_ = status;
  for (auto& observer : observers_)
    observer.OnAppListModelStatusChanged();
}

void AppListModel::SetState(ash::AppListState state) {
  if (state_ == state)
    return;

  auto old_state = state_;
  state_ = state;
  for (auto& observer : observers_)
    observer.OnAppListStateChanged(state_, old_state);
}

void AppListModel::SetStateFullscreen(ash::AppListViewState state) {
  state_fullscreen_ = state;
}

AppListItem* AppListModel::FindItem(const std::string& id) {
  AppListItem* item = top_level_item_list_->FindItem(id);
  if (item)
    return item;
  for (size_t i = 0; i < top_level_item_list_->item_count(); ++i) {
    AppListItem* child_item =
        top_level_item_list_->item_at(i)->FindChildItem(id);
    if (child_item)
      return child_item;
  }
  return nullptr;
}

AppListFolderItem* AppListModel::FindFolderItem(const std::string& id) {
  AppListItem* item = top_level_item_list_->FindItem(id);
  if (item && item->GetItemType() == AppListFolderItem::kItemType)
    return static_cast<AppListFolderItem*>(item);
  DCHECK(!item);
  return nullptr;
}

AppListItem* AppListModel::AddItem(std::unique_ptr<AppListItem> item) {
  DCHECK(!item->IsInFolder());
  DCHECK(!top_level_item_list()->FindItem(item->id()));
  return AddItemToItemListAndNotify(std::move(item));
}

void AppListModel::AddPageBreakItemAfter(const AppListItem* previous_item) {
  AppListItem* page_break_item =
      top_level_item_list()->AddPageBreakItemAfter(previous_item);
  for (auto& observer : observers_)
    observer.OnAppListItemAdded(page_break_item);
}

AppListItem* AppListModel::AddItemToFolder(std::unique_ptr<AppListItem> item,
                                           const std::string& folder_id) {
  if (folder_id.empty())
    return AddItem(std::move(item));
  DVLOG(2) << "AddItemToFolder: " << item->id() << ": " << folder_id;
  CHECK_NE(folder_id, item->folder_id());
  DCHECK_NE(AppListFolderItem::kItemType, item->GetItemType());
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  if (!dest_folder)
    return nullptr;
  DCHECK(!dest_folder->item_list()->FindItem(item->id()))
      << "Already in folder: " << dest_folder->id();
  return AddItemToFolderItemAndNotify(dest_folder, std::move(item));
}

const std::string AppListModel::MergeItems(const std::string& target_item_id,
                                           const std::string& source_item_id) {
  DVLOG(2) << "MergeItems: " << source_item_id << " -> " << target_item_id;

  if (target_item_id == source_item_id) {
    LOG(WARNING) << "MergeItems tried to drop item onto itself ("
                 << source_item_id << " -> " << target_item_id << ").";
    return "";
  }

  // Find the target item.
  AppListItem* target_item = top_level_item_list_->FindItem(target_item_id);
  if (!target_item) {
    LOG(ERROR) << "MergeItems: Target no longer exists.";
    return "";
  }

  AppListItem* source_item = FindItem(source_item_id);
  if (!source_item) {
    LOG(ERROR) << "MergeItems: Source no longer exists.";
    return "";
  }

  // If the target item is a folder, just add the source item to it.
  if (target_item->GetItemType() == AppListFolderItem::kItemType) {
    AppListFolderItem* target_folder =
        static_cast<AppListFolderItem*>(target_item);
    if (target_folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM) {
      LOG(WARNING) << "MergeItems called with OEM folder as target";
      return "";
    }
    std::unique_ptr<AppListItem> source_item_ptr = RemoveItem(source_item);
    source_item_ptr->set_position(
        target_folder->item_list()->CreatePositionBefore(
            syncer::StringOrdinal()));
    AddItemToFolderItemAndNotify(target_folder, std::move(source_item_ptr));
    return target_folder->id();
  }

  // Otherwise remove the source item and target item from their current
  // location, they will become owned by the new folder.
  std::unique_ptr<AppListItem> source_item_ptr = RemoveItem(source_item);
  CHECK(source_item_ptr);
  // Note: This would fail if |target_item_id == source_item_id|, except we
  // checked that they are distinct at the top of this method.
  std::unique_ptr<AppListItem> target_item_ptr =
      top_level_item_list_->RemoveItem(target_item_id);
  CHECK(target_item_ptr);

  // Create a new folder in the same location as the target item.
  std::string new_folder_id = AppListFolderItem::GenerateId();
  DVLOG(2) << "Creating folder for merge: " << new_folder_id;
  std::unique_ptr<AppListItem> new_folder_ptr =
      std::make_unique<AppListFolderItem>(new_folder_id);
  new_folder_ptr->set_position(target_item_ptr->position());
  AppListFolderItem* new_folder = static_cast<AppListFolderItem*>(
      AddItemToItemListAndNotify(std::move(new_folder_ptr)));

  // Add the items to the new folder.
  target_item_ptr->set_position(
      new_folder->item_list()->CreatePositionBefore(syncer::StringOrdinal()));
  AddItemToFolderItemAndNotify(new_folder, std::move(target_item_ptr));
  source_item_ptr->set_position(
      new_folder->item_list()->CreatePositionBefore(syncer::StringOrdinal()));
  AddItemToFolderItemAndNotify(new_folder, std::move(source_item_ptr));

  return new_folder->id();
}

void AppListModel::MoveItemToFolder(AppListItem* item,
                                    const std::string& folder_id) {
  DVLOG(2) << "MoveItemToFolder: " << folder_id << " <- "
           << item->ToDebugString();
  if (item->folder_id() == folder_id)
    return;
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  std::unique_ptr<AppListItem> item_ptr = RemoveItem(item);
  if (dest_folder) {
    CHECK(!item->IsInFolder());
    AddItemToFolderItemAndNotify(dest_folder, std::move(item_ptr));
  } else {
    AddItemToItemListAndNotifyUpdate(std::move(item_ptr));
  }
}

bool AppListModel::MoveItemToFolderAt(AppListItem* item,
                                      const std::string& folder_id,
                                      syncer::StringOrdinal position) {
  DVLOG(2) << "MoveItemToFolderAt: " << folder_id << "["
           << position.ToDebugString() << "]"
           << " <- " << item->ToDebugString();
  if (item->folder_id() == folder_id)
    return false;
  AppListFolderItem* src_folder = FindOrCreateFolderItem(item->folder_id());
  if (src_folder &&
      src_folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM) {
    LOG(WARNING) << "MoveItemToFolderAt called with OEM folder as source";
    return false;
  }
  AppListFolderItem* dest_folder = FindOrCreateFolderItem(folder_id);
  std::unique_ptr<AppListItem> item_ptr = RemoveItem(item);
  if (dest_folder) {
    item_ptr->set_position(
        dest_folder->item_list()->CreatePositionBefore(position));
    AddItemToFolderItemAndNotify(dest_folder, std::move(item_ptr));
  } else {
    item_ptr->set_position(
        top_level_item_list_->CreatePositionBefore(position));
    AddItemToItemListAndNotifyUpdate(std::move(item_ptr));
  }
  return true;
}

void AppListModel::SetItemPosition(AppListItem* item,
                                   const syncer::StringOrdinal& new_position) {
  if (!item->IsInFolder()) {
    top_level_item_list_->SetItemPosition(item, new_position);
    // Note: this will trigger OnListItemMoved which will signal observers.
    // (This is done this way because some View code still moves items within
    // the item list directly).
    return;
  }
  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  DCHECK(folder);
  folder->item_list()->SetItemPosition(item, new_position);
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

void AppListModel::SetItemName(AppListItem* item, const std::string& name) {
  item->SetName(name);
  DVLOG(2) << "AppListModel::SetItemName: " << item->ToDebugString();
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

void AppListModel::SetItemNameAndShortName(AppListItem* item,
                                           const std::string& name,
                                           const std::string& short_name) {
  item->SetNameAndShortName(name, short_name);
  DVLOG(2) << "AppListModel::SetItemNameAndShortName: "
           << item->ToDebugString();
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

void AppListModel::DeleteItem(const std::string& id) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;
  if (!item->IsInFolder()) {
    DCHECK_EQ(0u, item->ChildItemCount())
        << "Invalid call to DeleteItem for item with children: " << id;
    for (auto& observer : observers_)
      observer.OnAppListItemWillBeDeleted(item);
    top_level_item_list_->DeleteItem(id);
    for (auto& observer : observers_)
      observer.OnAppListItemDeleted(id);
    return;
  }
  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  DCHECK(folder) << "Folder not found for item: " << item->ToDebugString();
  std::unique_ptr<AppListItem> child_item = RemoveItemFromFolder(folder, item);
  DCHECK_EQ(item, child_item.get());
  for (auto& observer : observers_)
    observer.OnAppListItemWillBeDeleted(item);
  child_item.reset();  // Deletes item.
  for (auto& observer : observers_)
    observer.OnAppListItemDeleted(id);
}

void AppListModel::DeleteUninstalledItem(const std::string& id) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;
  const std::string folder_id = item->folder_id();
  DeleteItem(id);

  // crbug.com/368111: Deleting a child item may cause the parent folder to be
  // auto-removed. Further, if an auto-removed folder has an item in it, that
  // item needs to be reparented first.
  AppListFolderItem* folder = FindFolderItem(folder_id);
  if (folder && folder->ShouldAutoRemove() &&
      folder->item_list()->item_count() == 1) {
    AppListItem* last_item = folder->item_list()->item_at(0);
    MoveItemToFolderAt(last_item, "", folder->position());
  }
}

void AppListModel::DeleteAllItems() {
  while (top_level_item_list_->item_count() > 0) {
    AppListItem* item = top_level_item_list_->item_at(0);
    const std::string id = item->id();
    for (auto& observer : observers_)
      observer.OnAppListItemWillBeDeleted(item);
    top_level_item_list_->DeleteItemAt(0);
    for (auto& observer : observers_)
      observer.OnAppListItemDeleted(id);
  }
}

// Private methods

void AppListModel::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

AppListFolderItem* AppListModel::FindOrCreateFolderItem(
    const std::string& folder_id) {
  if (folder_id.empty())
    return nullptr;

  AppListFolderItem* dest_folder = FindFolderItem(folder_id);
  if (dest_folder)
    return dest_folder;

  DVLOG(2) << "Creating new folder: " << folder_id;
  std::unique_ptr<AppListFolderItem> new_folder =
      std::make_unique<AppListFolderItem>(folder_id);
  new_folder->set_position(
      top_level_item_list_->CreatePositionBefore(syncer::StringOrdinal()));
  AppListItem* new_folder_item =
      AddItemToItemListAndNotify(std::move(new_folder));
  return static_cast<AppListFolderItem*>(new_folder_item);
}

AppListItem* AppListModel::AddItemToItemListAndNotify(
    std::unique_ptr<AppListItem> item_ptr) {
  DCHECK(!item_ptr->IsInFolder());
  AppListItem* item = top_level_item_list_->AddItem(std::move(item_ptr));
  for (auto& observer : observers_)
    observer.OnAppListItemAdded(item);
  return item;
}

AppListItem* AppListModel::AddItemToItemListAndNotifyUpdate(
    std::unique_ptr<AppListItem> item_ptr) {
  DCHECK(!item_ptr->IsInFolder());
  AppListItem* item = top_level_item_list_->AddItem(std::move(item_ptr));
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
  return item;
}

AppListItem* AppListModel::AddItemToFolderItemAndNotify(
    AppListFolderItem* folder,
    std::unique_ptr<AppListItem> item_ptr) {
  CHECK_NE(folder->id(), item_ptr->folder_id());
  AppListItem* item = folder->item_list()->AddItem(std::move(item_ptr));
  item->set_folder_id(folder->id());
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
  return item;
}

std::unique_ptr<AppListItem> AppListModel::RemoveItem(AppListItem* item) {
  if (!item->IsInFolder())
    return top_level_item_list_->RemoveItem(item->id());

  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  return RemoveItemFromFolder(folder, item);
}

std::unique_ptr<AppListItem> AppListModel::RemoveItemFromFolder(
    AppListFolderItem* folder,
    AppListItem* item) {
  std::string folder_id = folder->id();
  CHECK_EQ(item->folder_id(), folder_id);
  std::unique_ptr<AppListItem> result =
      folder->item_list()->RemoveItem(item->id());
  result->set_folder_id("");
  if (folder->item_list()->item_count() == 0) {
    DVLOG(2) << "Deleting empty folder: " << folder->ToDebugString();
    DeleteItem(folder_id);
  }
  return result;
}

}  // namespace ash
