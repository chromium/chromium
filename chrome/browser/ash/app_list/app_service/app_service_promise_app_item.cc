// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

#include "base/check.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_update.h"
#include "chrome/browser/ash/app_list/app_list_model_updater.h"
#include "chrome/browser/ash/app_list/chrome_app_list_item.h"

// static
const char AppServicePromiseAppItem::kItemType[] = "AppServicePromiseAppItem";

AppServicePromiseAppItem::AppServicePromiseAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const apps::PromiseAppUpdate& update)
    : ChromeAppListItem(profile, update.PackageId().ToString()) {
  InitializeItem(update);

  // Promise icons should not be synced as they are transient and only present
  // during app installations.
  SetIsEphemeral(true);

  SetPosition(CalculateDefaultPositionIfApplicable());

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

void AppServicePromiseAppItem::Activate(int event_flags) {
  base::DoNothing();
}

const char* AppServicePromiseAppItem::GetItemType() const {
  return AppServicePromiseAppItem::kItemType;
}

AppServicePromiseAppItem::~AppServicePromiseAppItem() = default;

void AppServicePromiseAppItem::OnPromiseAppUpdate(
    const apps::PromiseAppUpdate& update) {
  if (update.NameChanged() && update.Name().has_value()) {
    SetName(update.Name().value());
  }
  if (update.ProgressChanged() && update.Progress().has_value()) {
    progress_ = update.Progress();
  }
}

void AppServicePromiseAppItem::LoadIcon() {
  // TODO(b/261907495): Retrieve icon from Promise App Icon Cache.
}

void AppServicePromiseAppItem::InitializeItem(
    const apps::PromiseAppUpdate& update) {
  CHECK(update.Name().has_value());
  CHECK(update.ShouldShow());
  SetName(update.Name().value());
  if (update.Progress().has_value()) {
    progress_ = update.Progress();
  }
  // TODO(b/261907495): Consider adding new AppStatus values specific to promise
  // apps and update them in OnPromiseAppUpdate.
  SetAppStatus(ash::AppStatus::kReady);
}

void AppServicePromiseAppItem::GetContextMenuModel(
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  // TODO(b/261907495): Create Promise App Context Menu.
}

app_list::AppContextMenu* AppServicePromiseAppItem::GetAppContextMenu() {
  return nullptr;
}
