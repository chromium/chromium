// Copyright 2013 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_list_item.h"

#include "ash/app_list/model/app_list_item_observer.h"
#include "ash/public/cpp/app_list/app_list_config_provider.h"

namespace ash {

AppListItem::AppListItem(const std::string& id)
    : metadata_(std::make_unique<AppListItemMetadata>()) {
  metadata_->id = id;
}

AppListItem::~AppListItem() {
  for (auto& observer : observers_)
    observer.ItemBeingDestroyed();
}

void AppListItem::SetIcon(AppListConfigType config_type,
                          const gfx::ImageSkia& icon) {
  per_config_icons_[config_type] = icon;
  icon.EnsureRepsForSupportedScales();

  for (auto& observer : observers_)
    observer.ItemIconChanged(config_type);
}

const gfx::ImageSkia& AppListItem::GetIcon(
    AppListConfigType config_type) const {
  const auto& it = per_config_icons_.find(config_type);
  if (it != per_config_icons_.end())
    return it->second;

  // If icon for requested config cannt be found, default to the shared config
  // icon.
  return metadata_->icon;
}

void AppListItem::SetDefaultIcon(const gfx::ImageSkia& icon) {
  metadata_->icon = icon;
  icon.EnsureRepsForSupportedScales();

  // If the item does not have a config specific icon, it will be represented by
  // the (possibly scaled) default icon, which means that changing the default
  // icon will also change item icons for configs that don't have a designated
  // icon.
  for (auto config_type :
       AppListConfigProvider::Get().GetAvailableConfigTypes()) {
    if (per_config_icons_.find(config_type) == per_config_icons_.end()) {
      for (auto& observer : observers_)
        observer.ItemIconChanged(config_type);
    }
  }
}

const gfx::ImageSkia& AppListItem::GetDefaultIcon() const {
  return metadata_->icon;
}

void AppListItem::SetNotificationBadgeColor(const SkColor color) {
  metadata_->badge_color = color;
  for (auto& observer : observers_) {
    observer.ItemBadgeColorChanged();
  }
}

void AppListItem::AddObserver(AppListItemObserver* observer) {
  observers_.AddObserver(observer);
}

void AppListItem::RemoveObserver(AppListItemObserver* observer) {
  observers_.RemoveObserver(observer);
}

const char* AppListItem::GetItemType() const {
  static const char* app_type = "";
  return app_type;
}

AppListItem* AppListItem::FindChildItem(const std::string& id) {
  return nullptr;
}

size_t AppListItem::ChildItemCount() const {
  return 0;
}

std::string AppListItem::ToDebugString() const {
  return id().substr(0, 8) + " '" + (is_page_break() ? "page_break" : name()) +
         "'" + " [" + position().ToDebugString() + "]";
}

// Protected methods

void AppListItem::SetName(const std::string& name) {
  if (metadata_->name == name && (short_name_.empty() || short_name_ == name))
    return;
  metadata_->name = name;
  short_name_.clear();
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

void AppListItem::SetNameAndShortName(const std::string& name,
                                      const std::string& short_name) {
  if (metadata_->name == name && short_name_ == short_name)
    return;
  metadata_->name = name;
  short_name_ = short_name;
  for (auto& observer : observers_)
    observer.ItemNameChanged();
}

void AppListItem::UpdateNotificationBadge(bool has_badge) {
  if (has_notification_badge_ == has_badge)
    return;

  has_notification_badge_ = has_badge;
  for (auto& observer : observers_) {
    observer.ItemBadgeVisibilityChanged();
  }
}

}  // namespace ash
