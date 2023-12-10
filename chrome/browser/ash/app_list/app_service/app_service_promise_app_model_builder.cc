// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_model_builder.h"
#include <ostream>

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_registry_cache.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_list_model_builder.h"
#include "chrome/browser/ash/app_list/app_service/app_service_promise_app_item.h"

AppServicePromiseAppModelBuilder::AppServicePromiseAppModelBuilder(
    AppListControllerDelegate* controller)
    : AppListModelBuilder(controller, AppServicePromiseAppItem::kItemType) {}

AppServicePromiseAppModelBuilder::~AppServicePromiseAppModelBuilder() = default;

void AppServicePromiseAppModelBuilder::BuildModel() {
  CHECK(!promise_app_registry_cache_observation_.IsObserving());
  promise_app_registry_cache_observation_.Observe(
      apps::AppServiceProxyFactory::GetForProfile(profile())
          ->PromiseAppRegistryCache());

  // No need to iterate through the registry cache and insert existing promise
  // apps into the model since the registry cache will be empty on start up.
  // Promise apps in the cache only get registered/ created when we start new
  // app installations, at which point the AppServicePromiseAppModelBuilder
  // should already exist. This will change in the future when we support ARC
  // default promise apps.
  // TODO(b/286981938): Insert existing promise app entries from the registry
  // cache.
}

// Update the App Service Promise App Item for the appropriate promise app if
// one already exists. Otherwise, create a new one.
void AppServicePromiseAppModelBuilder::OnPromiseAppUpdate(
    const apps::PromiseAppUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.PackageId().ToString());
  bool show = update.ShouldShow();
  if (item) {
    CHECK(item->GetItemType() == AppServicePromiseAppItem::kItemType);
    static_cast<AppServicePromiseAppItem*>(item)->OnPromiseAppUpdate(update);

    if (update.StatusChanged() &&
        update.Status() == apps::PromiseStatus::kSuccess &&
        item->GetPromisedItemId().empty()) {
      if (service()) {
        service()->CopyPromiseItemAttributesToItem(
            update.PackageId().ToString(), update.InstalledAppId());
      }
    }

    if (!show) {
      RemoveApp(update.PackageId().ToString(), false);
    }
  } else if (show) {
    std::optional<app_list::AppListSyncableService::LinkedPromiseAppSyncItem>
        linked_sync_item =
            service() ? service()->CreateLinkedPromiseSyncItemIfAvailable(
                            update.PackageId().ToString())
                      : std::nullopt;
    if (linked_sync_item) {
      InsertApp(std::make_unique<AppServicePromiseAppItem>(
          profile(), model_updater(), update, linked_sync_item->linked_item_id,
          linked_sync_item->promise_item));
    } else {
      InsertApp(std::make_unique<AppServicePromiseAppItem>(
          profile(), model_updater(), update, "", nullptr));
    }
  }
}

void AppServicePromiseAppModelBuilder::OnPromiseAppRemoved(
    const apps::PackageId& package_id) {
  RemoveApp(package_id.ToString(), false);
}

void AppServicePromiseAppModelBuilder::OnPromiseAppRegistryCacheWillBeDestroyed(
    apps::PromiseAppRegistryCache* cache) {
  promise_app_registry_cache_observation_.Reset();
}
