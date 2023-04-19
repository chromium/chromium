// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_folder_item.h"

#include <utility>

#include "ash/app_list/model/app_list_item_list.h"
#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/uuid.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

AppListFolderItem::AppListFolderItem(
    const std::string& id,
    AppListModelDelegate* app_list_model_delegate)
    : AppListItem(id),
      folder_type_(id == kOemFolderId ? FOLDER_TYPE_OEM : FOLDER_TYPE_NORMAL),
      item_list_(std::make_unique<AppListItemList>(app_list_model_delegate)) {
  // `item_list_` is initially empty, so there are no items to observe.
  // Item observers are added later in OnListItemAdded().
  item_list_->AddObserver(this);

  std::vector<AppListConfigType> configs = {AppListConfigType::kRegular,
                                            AppListConfigType::kDense};
  EnsureIconsForAvailableConfigTypes(configs, /*request_icon_update=*/false);
  config_provider_observation_.Observe(&AppListConfigProvider::Get());
  set_is_folder(true);
}

AppListFolderItem::~AppListFolderItem() {
  for (auto& image : folder_images_)
    image.second->RemoveObserver(this);
  for (size_t i = 0; i < item_list_->item_count(); ++i)
    item_list_->item_at(i)->RemoveObserver(this);
  item_list_->RemoveObserver(this);
}

gfx::Rect AppListFolderItem::GetTargetIconRectInFolderForItem(
    const AppListConfig& app_list_config,
    AppListItem* item,
    const gfx::Rect& folder_icon_bounds) {
  return folder_images_[app_list_config.type()]
      ->GetTargetIconRectInFolderForItem(app_list_config, item,
                                         folder_icon_bounds);
}

// static
const char AppListFolderItem::kItemType[] = "FolderItem";

AppListFolderItem* AppListFolderItem::AsFolderItem() {
  return this;
}

const char* AppListFolderItem::GetItemType() const {
  return AppListFolderItem::kItemType;
}

AppListItem* AppListFolderItem::FindChildItem(const std::string& id) {
  return item_list_->FindItem(id);
}

AppListItem* AppListFolderItem::GetChildItemAt(size_t index) {
  return item_list_->item_at(index);
}

size_t AppListFolderItem::ChildItemCount() const {
  return item_list_->item_count();
}

void AppListFolderItem::OnAppListConfigCreated(AppListConfigType config_type) {
  // Ensure that the folder image icon gets created for the newly created config
  // type (this might get called after model initialization, so the FolderImage
  // might have missed |item_list_| updates).
  EnsureIconsForAvailableConfigTypes({config_type},
                                     true /*request_icon_update*/);
}

void AppListFolderItem::OnListItemAdded(size_t index, AppListItem* item) {
  item->AddObserver(this);
  UpdateIsNewInstall();
  UpdateNotificationBadge();
}

void AppListFolderItem::OnListItemRemoved(size_t index, AppListItem* item) {
  item->RemoveObserver(this);
  UpdateIsNewInstall();
  UpdateNotificationBadge();
}

void AppListFolderItem::ItemBadgeVisibilityChanged() {
  UpdateNotificationBadge();
}

void AppListFolderItem::ItemIsNewInstallChanged() {
  UpdateIsNewInstall();
}

bool AppListFolderItem::IsSystemFolder() const {
  return GetMetadata()->is_system_folder;
}

void AppListFolderItem::SetIsSystemFolder(bool is_system_folder) {
  metadata()->is_system_folder = is_system_folder;
}

std::string AppListFolderItem::GenerateId() {
  return base::Uuid::GenerateRandomV4().AsLowercaseString();
}

void AppListFolderItem::OnFolderImageUpdated(AppListConfigType config) {
  SetIcon(config, folder_images_[config]->icon());
}

void AppListFolderItem::NotifyOfDraggedItem(AppListItem* dragged_item) {
  dragged_item_ = dragged_item;

  for (auto& image : folder_images_)
    image.second->UpdateDraggedItem(dragged_item);
}

FolderImage* AppListFolderItem::GetFolderImageForTesting(
    AppListConfigType type) const {
  const auto& image_it = folder_images_.find(type);
  if (image_it == folder_images_.end())
    return nullptr;
  return image_it->second.get();
}

void AppListFolderItem::RequestFolderIconUpdate() {
  // Request a folder icon refresh for each AppListConfigType available.
  for (auto& folder_image_pair : folder_images_)
    folder_image_pair.second->ItemIconChanged(folder_image_pair.first);
}

void AppListFolderItem::EnsureIconsForAvailableConfigTypes(
    const std::vector<AppListConfigType>& config_types,
    bool request_icon_update) {
  for (const auto& config_type : config_types) {
    if (folder_images_.count(config_type))
      continue;
    const AppListConfig* config = AppListConfigProvider::Get().GetConfigForType(
        config_type, false /*can_create*/);
    if (!config)
      continue;

    auto image = std::make_unique<FolderImage>(config, item_list_.get());
    image->AddObserver(this);
    auto* image_ptr = image.get();
    folder_images_.emplace(config_type, std::move(image));

    // Call this after the image has been added to |folder_images_| to make sure
    // |folder_images_| contains the image if the observer interface is called.
    // Note that UpdateDraggedItem will call UpdateIcon().
    if (dragged_item_) {
      DCHECK(request_icon_update);
      image_ptr->UpdateDraggedItem(dragged_item_);
    } else if (request_icon_update) {
      image_ptr->UpdateIcon();
    }
  }
}

void AppListFolderItem::UpdateIsNewInstall() {
  bool contains_new_install_item = false;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (item_list_->item_at(i)->is_new_install()) {
      contains_new_install_item = true;
      break;
    }
  }
  // The folder is marked with the "new install" dot if it contains an item that
  // is a new install.
  SetIsNewInstall(contains_new_install_item);
}

void AppListFolderItem::UpdateNotificationBadge() {
  bool contains_item_with_notification_badge = false;
  for (size_t i = 0; i < item_list_->item_count(); ++i) {
    if (item_list_->item_at(i)->has_notification_badge()) {
      contains_item_with_notification_badge = true;
      break;
    }
  }
  AppListItem::UpdateNotificationBadge(contains_item_with_notification_badge);
}

}  // namespace ash
