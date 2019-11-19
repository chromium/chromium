// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_folder_item.h"

#include "ash/app_list/model/app_list_item_list.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_features.h"
#include "base/guid.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

AppListFolderItem::AppListFolderItem(const std::string& id)
    : AppListItem(id),
      folder_type_(id == ash::kOemFolderId ? FOLDER_TYPE_OEM
                                           : FOLDER_TYPE_NORMAL),
      item_list_(new AppListItemList) {
  if (app_list_features::IsScalableAppListEnabled()) {
    EnsureIconsForAvailableConfigTypes(
        {ash::AppListConfigType::kLarge, ash::AppListConfigType::kMedium,
         ash::AppListConfigType::kSmall},
        false /*request_icon_update*/);
    config_provider_observer_.Add(&AppListConfigProvider::Get());
  } else {
    EnsureIconsForAvailableConfigTypes({ash::AppListConfigType::kShared},
                                       false /*reqest_icon_update*/);
  }
  set_is_folder(true);
}

AppListFolderItem::~AppListFolderItem() {
  for (auto& image : folder_images_)
    image.second->RemoveObserver(this);
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

const char* AppListFolderItem::GetItemType() const {
  return AppListFolderItem::kItemType;
}

AppListItem* AppListFolderItem::FindChildItem(const std::string& id) {
  return item_list_->FindItem(id);
}

size_t AppListFolderItem::ChildItemCount() const {
  return item_list_->item_count();
}

void AppListFolderItem::OnAppListConfigCreated(
    ash::AppListConfigType config_type) {
  // Ensure that the folder image icon gets created for the newly created config
  // type (this might get called after model initialization, so the FolderImage
  // might have missed |item_list_| updates).
  EnsureIconsForAvailableConfigTypes({config_type},
                                     true /*request_icon_update*/);
}

bool AppListFolderItem::IsPersistent() const {
  return GetMetadata()->is_persistent;
}

void AppListFolderItem::SetIsPersistent(bool is_persistent) {
  metadata()->is_persistent = is_persistent;
}

bool AppListFolderItem::ShouldAutoRemove() const {
  return ChildItemCount() <= (IsPersistent() ? 0u : 1u);
}

std::string AppListFolderItem::GenerateId() {
  return base::GenerateGUID();
}

void AppListFolderItem::OnFolderImageUpdated(ash::AppListConfigType config) {
  SetIcon(config, folder_images_[config]->icon());
}

void AppListFolderItem::NotifyOfDraggedItem(AppListItem* dragged_item) {
  for (auto& image : folder_images_)
    image.second->UpdateDraggedItem(dragged_item);
}

FolderImage* AppListFolderItem::GetFolderImageForTesting(
    ash::AppListConfigType type) const {
  const auto& image_it = folder_images_.find(type);
  if (image_it == folder_images_.end())
    return nullptr;
  return image_it->second.get();
}

void AppListFolderItem::EnsureIconsForAvailableConfigTypes(
    const std::vector<ash::AppListConfigType>& config_types,
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
    if (request_icon_update)
      image_ptr->UpdateIcon();
  }
}

}  // namespace ash
