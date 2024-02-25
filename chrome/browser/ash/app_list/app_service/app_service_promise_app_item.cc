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
#include "chrome/browser/apps/app_service/promise_apps/promise_app_metrics.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_service.h"
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
    const std::string& promised_app_id,
    const app_list::AppListSyncableService::SyncItem* sync_item)
    : ChromeAppListItem(profile, update.PackageId().ToString()),
      package_id_(update.PackageId()),
      promised_app_id_(promised_app_id) {
  InitializeItem(update);

  // Promise icons should not be synced as they are transient and only present
  // during app installations.
  SetIsEphemeral(true);

  const syncer::StringOrdinal position =
      sync_item ? sync_item->item_ordinal : syncer::StringOrdinal();
  SetPosition(position.IsValid() ? position
                                 : CalculateDefaultPositionIfApplicable());
  if (sync_item) {
    SetChromeFolderId(sync_item->parent_id);
  }

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

void AppServicePromiseAppItem::ExecuteLaunchCommand(int event_flags) {
  // Promise app items should not be launched.
}

void AppServicePromiseAppItem::Activate(int event_flags) {
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->PromiseAppService()
      ->UpdateInstallPriority(package_id_.ToString());
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
    SetName(base::UTF16ToUTF8(
        ShelfControllerHelper::GetLabelForPromiseStatus(update.Status())));
    SetAccessibleName(base::UTF16ToUTF8(
        ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
            update.Name(), update.Status())));
  }
  if (update.ProgressChanged() && update.Progress().has_value()) {
    SetProgress(update.Progress().value());
  }
  LoadIcon();
}

void AppServicePromiseAppItem::LoadIcon() {
  apps::AppServiceProxyFactory::GetForProfile(profile())->LoadPromiseIcon(
      package_id_,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      apps::IconEffects::kCrOsStandardMask,
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
  SetPromisePackageId(update.PackageId().ToString());
  SetName(base::UTF16ToUTF8(
      ShelfControllerHelper::GetLabelForPromiseStatus(update.Status())));
  SetAccessibleName(base::UTF16ToUTF8(
      ShelfControllerHelper::GetAccessibleLabelForPromiseStatus(
          update.Name(), update.Status())));
  SetProgress(update.Progress().value_or(0));
  SetAppStatus(
      ShelfControllerHelper::ConvertPromiseStatusToAppStatus(update.Status()));
  apps::RecordPromiseAppLifecycleEvent(
      apps::PromiseAppLifecycleEvent::kCreatedInLauncher);
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

std::string AppServicePromiseAppItem::GetPromisedItemId() const {
  return promised_app_id_;
}
