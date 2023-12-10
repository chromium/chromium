// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_model.h"

#include <string>
#include <utility>

#include "ash/app_list/model/app_list_folder_item.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model_observer.h"
#include "ash/public/cpp/app_list/app_list_model_delegate.h"
#include "base/logging.h"

namespace ash {

AppListModel::AppListModel(AppListModelDelegate* app_list_model_delegate)
    : delegate_(app_list_model_delegate),
      top_level_item_list_(
          std::make_unique<AppListItemList>(app_list_model_delegate)) {
  item_list_scoped_observations_.AddObservation(top_level_item_list_.get());
}

AppListModel::~AppListModel() {
  item_list_scoped_observations_.RemoveAllObservations();
}

void AppListModel::AddObserver(AppListModelObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListModel::RemoveObserver(AppListModelObserver* observer) {
  observers_.RemoveObserver(observer);
}

void AppListModel::SetStatus(AppListModelStatus status) {
  if (status_ == status)
    return;

  status_ = status;
  for (auto& observer : observers_)
    observer.OnAppListModelStatusChanged();
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

AppListFolderItem* AppListModel::CreateFolderItem(
    const std::string& folder_id) {
  DCHECK(!top_level_item_list()->FindItem(folder_id));
  std::unique_ptr<AppListFolderItem> new_folder =
      std::make_unique<AppListFolderItem>(folder_id, delegate_);
  new_folder->set_position(
      top_level_item_list_->CreatePositionBefore(syncer::StringOrdinal()));
  AppListItem* new_folder_item = AddItemToRootListAndNotify(
      std::move(new_folder), ReparentItemReason::kAdd);
  return static_cast<AppListFolderItem*>(new_folder_item);
}

AppListItem* AppListModel::AddItem(std::unique_ptr<AppListItem> item) {
  DCHECK(!item->IsInFolder());
  DCHECK(!top_level_item_list()->FindItem(item->id()));
  return AddItemToRootListAndNotify(std::move(item), ReparentItemReason::kAdd);
}

void AppListModel::SetItemMetadata(const std::string& id,
                                   std::unique_ptr<AppListItemMetadata> data) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;

  // TODO(https://crbug.com/1252433): refactor this function because the current
  // implementation is bug prone.

  // data may not contain valid position or icon. Preserve it in this case.
  if (!data->position.IsValid())
    data->position = item->position();

  // Update the item's position and name based on the metadata.
  if (!data->position.Equals(item->position()))
    SetItemPosition(item, data->position);

  if (data->name != item->name()) {
    SetItemName(item, data->name);
  }

  if (data->accessible_name != item->accessible_name()) {
    SetItemAccessibleName(item, data->accessible_name);
  }

  if (data->progress > item->progress() ||
      data->app_status != item->app_status()) {
    item->SetProgress(data->progress);
    item->SetAppStatus(data->app_status);
    DVLOG(2) << "AppListModel::SetProgress: " << item->ToDebugString();
    for (auto& observer : observers_) {
      observer.OnAppListItemUpdated(item);
    }
  }

  if (data->icon.isNull()) {
    // Folder icons are generated on ash side so the icon of the metadata passed
    // from chrome side is null. Do not alter `item` default icon in this case.
    data->icon = item->GetDefaultIcon();
    data->icon_color = item->GetDefaultIconColor();
  }

  if (data->folder_id != item->folder_id())
    MoveItemToFolder(item, data->folder_id);

  item->SetMetadata(std::move(data));
}

AppListItem* AppListModel::AddItemToFolder(std::unique_ptr<AppListItem> item,
                                           const std::string& folder_id) {
  if (folder_id.empty())
    return AddItem(std::move(item));
  DVLOG(2) << "AddItemToFolder: " << item->id() << ": " << folder_id;
  CHECK_NE(folder_id, item->folder_id());
  DCHECK_NE(AppListFolderItem::kItemType, item->GetItemType());
  AppListFolderItem* dest_folder = FindFolderItem(folder_id);
  if (!dest_folder)
    dest_folder = CreateFolderItem(folder_id);
  DCHECK(!dest_folder->item_list()->FindItem(item->id()))
      << "Already in folder: " << dest_folder->id();
  return AddItemToFolderListAndNotify(dest_folder, std::move(item),
                                      ReparentItemReason::kAdd);
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
    delegate_->RequestMoveItemToFolder(source_item_id, target_item_id);
    return target_folder->id();
  }

  return delegate_->RequestFolderCreation(target_item_id, source_item_id);
}

void AppListModel::MoveItemToFolder(AppListItem* item,
                                    const std::string& folder_id) {
  DVLOG(2) << "MoveItemToFolder: " << folder_id << " <- "
           << item->ToDebugString();
  if (item->folder_id() == folder_id)
    return;

  if (!item->IsInFolder()) {
    AppListFolderItem* dest_folder = FindFolderItem(folder_id);
    if (!dest_folder)
      dest_folder = CreateFolderItem(folder_id);
    // Handle the case that `item` is a top list item.
    std::unique_ptr<AppListItem> item_ptr = RemoveFromTopList(item);
    AddItemToFolderListAndNotify(dest_folder, std::move(item_ptr),
                                 ReparentItemReason::kUpdate);
    return;
  }

  ReparentOrDeleteItemInFolder(item, folder_id);
}

bool AppListModel::MoveItemToRootAt(AppListItem* item,
                                    syncer::StringOrdinal position) {
  DVLOG(2) << "MoveItemToRootAt: "
           << "[" << position.ToDebugString() << "]"
           << " <- " << item->ToDebugString();
  if (item->folder_id().empty())
    return false;
  AppListFolderItem* src_folder = FindFolderItem(item->folder_id());
  if (src_folder &&
      src_folder->folder_type() == AppListFolderItem::FOLDER_TYPE_OEM) {
    LOG(WARNING) << "MoveItemToFolderAt called with OEM folder as source";
    return false;
  }
  delegate_->RequestMoveItemToRoot(
      item->id(), top_level_item_list_->CreatePositionBefore(position));
  return true;
}

void AppListModel::SetItemPosition(AppListItem* item,
                                   const syncer::StringOrdinal& new_position) {
  if (!item->IsInFolder()) {
    SetRootItemPosition(item, new_position);
    return;
  }

  // The code below handles the case that `item` has a parent folder.

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

void AppListModel::SetItemAccessibleName(AppListItem* item,
                                         const std::string& name) {
  item->SetAccessibleName(name);
  for (auto& observer : observers_) {
    observer.OnAppListItemUpdated(item);
  }
}

void AppListModel::DeleteItem(const std::string& id) {
  AppListItem* item = FindItem(id);
  if (!item)
    return;

  const std::string copied_folder_id = item->folder_id();
  if (!item->IsInFolder()) {
    DCHECK_EQ(0u, item->ChildItemCount())
        << "Invalid call to DeleteItem for item with children: " << id;
    for (auto& observer : observers_)
      observer.OnAppListItemWillBeDeleted(item);
    if (item->GetItemType() == AppListFolderItem::kItemType) {
      item_list_scoped_observations_.RemoveObservation(
          static_cast<AppListFolderItem*>(item)->item_list());
    }
    top_level_item_list_->DeleteItem(id);
    return;
  }

  // Destroy `item`.
  ReparentOrDeleteItemInFolder(item,
                               /*destination_folder_id=*/std::nullopt);
}

// Private methods

void AppListModel::OnListItemMoved(size_t from_index,
                                   size_t to_index,
                                   AppListItem* item) {
  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

AppListItem* AppListModel::AddItemToRootListAndNotify(
    std::unique_ptr<AppListItem> item_ptr,
    ReparentItemReason reason) {
  DCHECK(!item_ptr->IsInFolder());
  if (reason == ReparentItemReason::kAdd &&
      item_ptr->GetItemType() == AppListFolderItem::kItemType) {
    item_list_scoped_observations_.AddObservation(
        static_cast<AppListFolderItem*>(item_ptr.get())->item_list());
  }
  AppListItem* item = top_level_item_list_->AddItem(std::move(item_ptr));
  NotifyItemParentChange(item, reason);
  return item;
}

AppListItem* AppListModel::AddItemToFolderListAndNotify(
    AppListFolderItem* folder,
    std::unique_ptr<AppListItem> item_ptr,
    ReparentItemReason reason) {
  CHECK_NE(folder->id(), item_ptr->folder_id());

  // Calling `AppListItemList::AddItem()` could trigger
  // `AppListModel::SetItemMetadata()` so set the folder id before addition.
  item_ptr->set_folder_id(folder->id());

  AppListItem* item = folder->item_list()->AddItem(std::move(item_ptr));
  NotifyItemParentChange(item, reason);
  return item;
}

void AppListModel::NotifyItemParentChange(AppListItem* item,
                                          ReparentItemReason reason) {
  for (auto& observer : observers_) {
    switch (reason) {
      case ReparentItemReason::kAdd:
        observer.OnAppListItemAdded(item);
        break;
      case ReparentItemReason::kUpdate:
        observer.OnAppListItemUpdated(item);
        break;
    }
  }
}

std::unique_ptr<AppListItem> AppListModel::RemoveFromTopList(
    AppListItem* item) {
  DCHECK(!item->IsInFolder());

  if (item->GetItemType() == AppListFolderItem::kItemType) {
    item_list_scoped_observations_.RemoveObservation(
        static_cast<AppListFolderItem*>(item)->item_list());
  }

  return top_level_item_list_->RemoveItem(item->id());
}

void AppListModel::ReparentOrDeleteItemInFolder(
    AppListItem* item,
    std::optional<std::string> destination_folder_id) {
  AppListFolderItem* folder = FindFolderItem(item->folder_id());
  DCHECK(folder) << "Folder not found for item: " << item->ToDebugString();

  const std::string item_parent_id = item->folder_id();
  std::unique_ptr<AppListItem> removed_item =
      RemoveItemFromFolder(folder, item);
  if (destination_folder_id.has_value()) {
    // When an item is removed from a folder, it can be moved to the top
    // list or a folder.
    if (destination_folder_id->empty()) {
      AddItemToRootListAndNotify(std::move(removed_item),
                                 ReparentItemReason::kUpdate);
    } else {
      // Create a folder if the destination folder doesn't exist.
      AppListFolderItem* destination_folder =
          FindFolderItem(*destination_folder_id);
      if (!destination_folder)
        destination_folder = CreateFolderItem(*destination_folder_id);

      AddItemToFolderListAndNotify(destination_folder, std::move(removed_item),
                                   ReparentItemReason::kUpdate);
    }
  } else {
    // Destroy `removed_item` and notify observers.
    for (auto& observer : observers_)
      observer.OnAppListItemWillBeDeleted(item);
    removed_item.reset();  // Deletes item.
  }

  // Delete the folder if the folder becomes empty after child removal.
  DeleteFolderIfEmpty(item_parent_id);
}

std::unique_ptr<AppListItem> AppListModel::RemoveItemFromFolder(
    AppListFolderItem* folder,
    AppListItem* item) {
  CHECK_EQ(item->folder_id(), folder->id());
  std::unique_ptr<AppListItem> removed_item =
      folder->item_list()->RemoveItem(item->id());
  removed_item->set_folder_id("");
  return removed_item;
}

void AppListModel::DeleteFolderIfEmpty(const std::string& folder_id) {
  const AppListFolderItem* folder = FindFolderItem(folder_id);
  if (!folder || folder->item_list()->item_count())
    return;

  DVLOG(2) << "Deleting empty folder: " << folder->ToDebugString();
  std::string copy_id = folder->id();
  DeleteItem(copy_id);
}

void AppListModel::SetRootItemPosition(
    AppListItem* item,
    const syncer::StringOrdinal& new_position) {
  DCHECK(!item->IsInFolder());
  DCHECK(FindItem(item->id()));

  const bool index_change =
      top_level_item_list_->SetItemPosition(item, new_position);

  // If `index_change` is true, `OnListItemMoved()` is called and model
  // observers are signaled. Nothing to do so return early.
  if (index_change)
    return;

  for (auto& observer : observers_)
    observer.OnAppListItemUpdated(item);
}

}  // namespace ash
