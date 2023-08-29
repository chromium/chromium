// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/check.h"
#include "base/notreached.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_util.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_context_menu.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"
#include "chrome/browser/ui/ash/shelf/shelf_controller_helper.h"
#include "components/sync/model/string_ordinal.h"

// static
const char AppServicePromiseAppItem::kItemType[] = "AppServicePromiseAppItem";

AppServicePromiseAppItem::AppServicePromiseAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::PromiseAppUpdate& update,
    const syncer::StringOrdinal position)
    : ChromeAppListItem(profile, update.PackageId().ToString()),
      package_id_(update.PackageId()) {
  InitializeItem(update);

  SetPromisePackageId(update.PackageId().ToString());
  SetAppStatus(
      ShelfControllerHelper::ConvertPromiseStatusToAppStatus(update.Status()));
  SetProgress(update.Progress().value_or(0));

  // Promise icons should not be synced as they are transient and only present
  // during app installations.
  SetIsEphemeral(true);

  SetPosition(position.IsValid() ? position
                                 : CalculateDefaultPositionIfApplicable());

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

void AppServicePromiseAppItem::ExecuteLaunchCommand(int event_flags) {
  // Promise app items should not be launched.
}

void AppServicePromiseAppItem::Activate(int event_flags) {
  // Promise app items should not be activated.
}

const char* AppServicePromiseAppItem::GetItemType() const {
  return AppServicePromiseAppItem::kItemType;
}

AppServicePromiseAppItem::~AppServicePromiseAppItem() = default;

void AppServicePromiseAppItem::OnPromiseAppUpdate(
    const apps::PromiseAppUpdate& update) {
  // Each status has its own set of visual effects.
  if (update.StatusChanged()) {
    SetAppStatus(ShelfControllerHelper::ConvertPromiseStatusToAppStatus(
        update.Status()));
    SetName(ShelfControllerHelper::GetLabelForPromiseStatus(update.Status()));
    LoadIcon();
  }
  if (update.ProgressChanged() && update.Progress().has_value()) {
    SetProgress(update.Progress().value());
  }
}

void AppServicePromiseAppItem::LoadIcon() {
  apps::AppServiceProxyFactory::GetForProfile(profile())->LoadPromiseIcon(
      package_id_,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      apps::GetPromiseIconEffectsForAppStatus(app_status()),
      base::BindOnce(&AppServicePromiseAppItem::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServicePromiseAppItem::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    // TODO(b/261907495): Hide the promise app item from the user when there is
    // no icon to show.
    return;
  }
  SetIcon(icon_value->uncompressed, icon_value->is_placeholder_icon);
}

void AppServicePromiseAppItem::InitializeItem(
    const apps::PromiseAppUpdate& update) {
  CHECK(update.ShouldShow());
  SetName(ShelfControllerHelper::GetLabelForPromiseStatus(update.Status()));
  if (update.Progress().has_value()) {
    SetProgress(update.Progress().value());
  }
  SetAppStatus(
      ShelfControllerHelper::ConvertPromiseStatusToAppStatus(update.Status()));
}

void AppServicePromiseAppItem::GetContextMenuModel(
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<AppServicePromiseAppContextMenu>(
      this, profile(), package_id_, GetController(), item_context);
  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServicePromiseAppItem::GetAppContextMenu() {
  return context_menu_.get();
}
