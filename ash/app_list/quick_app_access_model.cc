// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/quick_app_access_model.h"

#include <string>

#include "ash/app_list/app_list_controller_impl.h"
#include "ash/app_list/app_list_model_provider.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/shell.h"
#include "base/metrics/histogram_functions.h"
#include "base/time/time.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace ash {

QuickAppAccessModel::QuickAppAccessModel() = default;

QuickAppAccessModel::~QuickAppAccessModel() = default;

void QuickAppAccessModel::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void QuickAppAccessModel::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

bool QuickAppAccessModel::SetQuickApp(const std::string& app_id) {
  if (quick_app_id_ == app_id) {
    return false;
  }

  if (app_id.empty()) {
    // Remove quick app when setting a quick app with an empty app id.
    ClearQuickApp();
    UpdateQuickAppShouldShowState();
    return true;
  }

  AppListItem* item = AppListModelProvider::Get()->model()->FindItem(app_id);
  if (!item) {
    // Return and don't set the quick app id if the app item doesn't exist.
    return false;
  }

  ClearQuickApp();
  quick_app_id_ = app_id;
  item_observation_.Observe(item);

  // Request to load in the icon when the app item's icon is null.
  if (item->GetDefaultIcon().isNull()) {
    icon_load_start_time_ = base::TimeTicks::Now();
    Shell::Get()->app_list_controller()->LoadIcon(app_id);
  }

  // When quick app is already in a show state, notify that the app icon has
  // changed to cover the case where the shown quick app is changed to another
  // app.
  if (quick_app_should_show_state_) {
    for (auto& observer : observers_) {
      observer.OnQuickAppIconChanged();
    }
  }

  UpdateQuickAppShouldShowState();
  return true;
}

void QuickAppAccessModel::SetQuickAppActivated() {
  ClearQuickApp();
  UpdateQuickAppShouldShowState();
}

gfx::ImageSkia QuickAppAccessModel::GetAppIcon(gfx::Size icon_size) {
  AppListItem* item = GetQuickAppItem();

  if (!item) {
    return gfx::ImageSkia();
  }

  gfx::ImageSkia image = item->GetDefaultIcon();

  if (item->GetDefaultIcon().isNull()) {
    return gfx::ImageSkia();
  }

  image = gfx::ImageSkiaOperations::CreateResizedImage(
      image, skia::ImageOperations::RESIZE_BEST, icon_size);
  return image;
}

const std::u16string QuickAppAccessModel::GetAppName() const {
  AppListItem* item = GetQuickAppItem();
  if (!item) {
    return std::u16string();
  }
  return base::UTF8ToUTF16(item->GetDisplayName());
}

void QuickAppAccessModel::ItemDefaultIconChanged() {
  if (quick_app_should_show_state_) {
    // If quick app should already be shown, notify observers when the changed
    // icon is not null.
    if (!GetQuickAppItem()->GetDefaultIcon().isNull()) {
      for (auto& observer : observers_) {
        observer.OnQuickAppIconChanged();
      }
    }
  } else {
    if (icon_load_start_time_) {
      UmaHistogramTimes("Apps.QuickAppIconLoadTime",
                        base::TimeTicks::Now() - *icon_load_start_time_);
      icon_load_start_time_.reset();
    }
    UpdateQuickAppShouldShowState();
  }
}

void QuickAppAccessModel::ItemIconVersionChanged() {
  icon_load_start_time_ = base::TimeTicks::Now();
  Shell::Get()->app_list_controller()->LoadIcon(quick_app_id_);
}

void QuickAppAccessModel::ItemBeingDestroyed() {
  ClearQuickApp();
  UpdateQuickAppShouldShowState();
}

void QuickAppAccessModel::OnAppListVisibilityChanged(bool shown,
                                                     int64_t display_id) {
  if (shown) {
    ClearQuickApp();
    UpdateQuickAppShouldShowState();
  }
}

AppListItem* QuickAppAccessModel::GetQuickAppItem() const {
  return AppListModelProvider::Get()->model()->FindItem(quick_app_id_);
}

void QuickAppAccessModel::UpdateQuickAppShouldShowState() {
  const bool prev_should_show_quick_app = quick_app_should_show_state_;
  quick_app_should_show_state_ = ShouldShowQuickApp();

  if (prev_should_show_quick_app == quick_app_should_show_state_) {
    return;
  }

  if (!quick_app_should_show_state_) {
    app_list_controller_observer_.Reset();
  } else {
    app_list_controller_observer_.Observe(Shell::Get()->app_list_controller());
  }

  for (auto& observer : observers_) {
    observer.OnQuickAppShouldShowChanged(quick_app_should_show_state_);
  }
}

bool QuickAppAccessModel::ShouldShowQuickApp() {
  return !quick_app_id_.empty() &&
         !GetQuickAppItem()->GetDefaultIcon().isNull();
}

void QuickAppAccessModel::ClearQuickApp() {
  quick_app_id_ = "";
  item_observation_.Reset();
  icon_load_start_time_.reset();
}

}  // namespace ash
