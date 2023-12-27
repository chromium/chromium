// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/model/app_icon_load_helper.h"

#include <algorithm>
#include <utility>

#include "ash/app_list/model/folder_image.h"
#include "base/check.h"
#include "base/memory/raw_ptr.h"

namespace ash {

// AppIconLoadHelper::AppItemHelper
class AppIconLoadHelper::AppItemHelper : public AppListItemObserver {
 public:
  AppItemHelper(AppListItem* app_item, IconLoadCallback icon_load_callback);
  AppItemHelper(const AppItemHelper&) = delete;
  AppItemHelper& operator=(const AppItemHelper&) = delete;
  ~AppItemHelper() override;

  // AppListItemObserver:
  void ItemIconVersionChanged() override;
  void ItemBeingDestroyed() override;

  const AppListItem* app_item() const { return app_item_; }

 private:
  raw_ptr<AppListItem> app_item_ = nullptr;
  IconLoadCallback icon_load_callback_;
  base::ScopedObservation<AppListItem, AppListItemObserver>
      app_item_observeration_{this};
};

AppIconLoadHelper::AppItemHelper::AppItemHelper(
    AppListItem* app_item,
    IconLoadCallback icon_load_callback)
    : app_item_(app_item), icon_load_callback_(std::move(icon_load_callback)) {
  DCHECK(!icon_load_callback_.is_null());

  app_item_observeration_.Observe(app_item_.get());

  if (app_item_->GetDefaultIcon().isNull())
    icon_load_callback_.Run(app_item_->id());
}

AppIconLoadHelper::AppItemHelper::~AppItemHelper() = default;

void AppIconLoadHelper::AppItemHelper::ItemIconVersionChanged() {
  icon_load_callback_.Run(app_item_->id());
}

void AppIconLoadHelper::AppItemHelper::ItemBeingDestroyed() {
  app_item_ = nullptr;
  app_item_observeration_.Reset();
}

// AppIconLoadHelper::AppItemListHelper
class AppIconLoadHelper::AppItemListHelper : public AppListItemListObserver {
 public:
  AppItemListHelper(AppListItemList* list, IconLoadCallback icon_load_callback);
  AppItemListHelper(const AppItemListHelper&) = delete;
  AppItemListHelper& operator=(const AppItemListHelper&) = delete;
  ~AppItemListHelper() override;

  // AppListItemListObserver:
  void OnListItemAdded(size_t index, AppListItem* item) override;
  void OnListItemRemoved(size_t index, AppListItem* item) override;
  void OnListItemMoved(size_t from_index,
                       size_t to_index,
                       AppListItem* item) override;

 private:
  using AppItemHelper = AppIconLoadHelper::AppItemHelper;

  void UpdateHelper();

  const raw_ptr<AppListItemList> list_;
  IconLoadCallback icon_load_callback_;
  base::ScopedObservation<AppListItemList, AppListItemListObserver>
      list_observeration_{this};
  std::vector<std::unique_ptr<AppItemHelper>> icon_helpers_;
};

AppIconLoadHelper::AppItemListHelper::AppItemListHelper(
    AppListItemList* list,
    IconLoadCallback icon_load_callback)
    : list_(list), icon_load_callback_(std::move(icon_load_callback)) {
  DCHECK(!icon_load_callback_.is_null());

  list_observeration_.Observe(list_.get());

  UpdateHelper();
}

AppIconLoadHelper::AppItemListHelper::~AppItemListHelper() = default;

void AppIconLoadHelper::AppItemListHelper::OnListItemAdded(size_t index,
                                                           AppListItem* item) {
  if (index < FolderImage::kNumFolderTopItems)
    UpdateHelper();
}

void AppIconLoadHelper::AppItemListHelper::OnListItemRemoved(
    size_t index,
    AppListItem* item) {
  if (index < FolderImage::kNumFolderTopItems)
    UpdateHelper();
}

void AppIconLoadHelper::AppItemListHelper::OnListItemMoved(size_t from_index,
                                                           size_t to_index,
                                                           AppListItem* item) {
  if (from_index < FolderImage::kNumFolderTopItems ||
      to_index < FolderImage::kNumFolderTopItems) {
    UpdateHelper();
  }
}

void AppIconLoadHelper::AppItemListHelper::UpdateHelper() {
  icon_helpers_.clear();

  const size_t count =
      std::min(FolderImage::kNumFolderTopItems, list_->item_count());
  for (size_t i = 0; i < count; ++i) {
    AppListItem* app_item = list_->item_at(i);
    icon_helpers_.emplace_back(
        std::make_unique<AppItemHelper>(app_item, icon_load_callback_));
  }
}

// AppIconLoadHelper
AppIconLoadHelper::AppIconLoadHelper(AppListItem* app_item,
                                     IconLoadCallback icon_load_callback)
    : item_helper_(
          std::make_unique<AppItemHelper>(app_item,
                                          std::move(icon_load_callback))) {}

AppIconLoadHelper::AppIconLoadHelper(AppListItemList* list,
                                     IconLoadCallback icon_load_callback)
    : list_helper_(
          std::make_unique<AppItemListHelper>(list,
                                              std::move(icon_load_callback))) {}

AppIconLoadHelper::~AppIconLoadHelper() = default;

}  // namespace ash
