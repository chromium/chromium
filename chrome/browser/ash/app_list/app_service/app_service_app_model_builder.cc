// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_app_model_builder.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "base/check_deref.h"
#include "chrome/browser/ash/app_list/app_service/app_service_app_item.h"
#include "chrome/browser/ash/arc/arc_util.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/sync/protocol/app_list_specifics.pb.h"
#include "ui/base/l10n/l10n_util.h"

namespace {

bool ShouldShowInLauncher(const apps::AppUpdate& update) {
  switch (update.Readiness()) {
    case apps::Readiness::kReady:
    case apps::Readiness::kDisabledByUser:
    case apps::Readiness::kDisabledByBlocklist:
    case apps::Readiness::kDisabledByPolicy:
    case apps::Readiness::kTerminated:
    case apps::Readiness::kDisabledByLocalSettings:
      return update.ShowInLauncher().value_or(false);
    default:
      return false;
  }
}

}  // namespace

AppServiceAppModelBuilder::AppServiceAppModelBuilder(
    AppListControllerDelegate* controller,
    apps::AppRegistryCache* app_registry_cache)
    : AppListModelBuilder(controller, AppServiceAppItem::kItemType),
      app_registry_cache_(CHECK_DEREF(app_registry_cache)) {}

AppServiceAppModelBuilder::~AppServiceAppModelBuilder() = default;

void AppServiceAppModelBuilder::BuildModel() {
  app_registry_cache_->ForEachApp(
      [this](const apps::AppUpdate& update) { OnAppUpdate(update); });
  app_registry_cache_observer_.Observe(&app_registry_cache_.get());
}

void AppServiceAppModelBuilder::OnAppUpdate(const apps::AppUpdate& update) {
  ChromeAppListItem* item = GetAppItem(update.AppId());
  bool show = ShouldShowInLauncher(update);
  if (item) {
    if (show) {
      CHECK_EQ(item->GetItemType(), AppServiceAppItem::kItemType,
               base::NotFatalUntil::M160);
      static_cast<AppServiceAppItem*>(item)->OnAppUpdate(update);

      // TODO(crbug.com/40569217): drop the check for kChromeApp or kWeb, and
      // call UpdateItem unconditionally?
      apps::AppType app_type = update.AppType();
      if ((app_type == apps::AppType::kChromeApp) ||
          (app_type == apps::AppType::kSystemWeb) ||
          (app_type == apps::AppType::kWeb)) {
        app_list::AppListSyncableService* serv = service();
        if (serv) {
          serv->UpdateItem(item);
        }
      }

    } else {
      bool unsynced_change = false;
      if (update.AppType() == apps::AppType::kArc) {
        // Don't sync app removal in case it was caused by disabling Google
        // Play Store.
        unsynced_change = !arc::IsArcPlayStoreEnabledForProfile(profile());
      }

      if (update.InstalledInternally()) {
        // Don't sync default app removal as default installed apps are not
        // synced.
        unsynced_change = true;
      }

      if (update.Readiness() == apps::Readiness::kUninstalledByNonUser) {
        // Don't sync migration uninstallations as it will interfere with other
        // devices doing their own migration.
        unsynced_change = true;
      }

      RemoveApp(update.AppId(), unsynced_change);
    }

  } else if (show) {
    InsertApp(std::make_unique<AppServiceAppItem>(
        profile(), model_updater(),
        GetSyncItem(update.AppId(), sync_pb::AppListSpecifics::TYPE_APP),
        update));
  }
}

void AppServiceAppModelBuilder::OnAppRegistryCacheWillBeDestroyed(
    apps::AppRegistryCache* cache) {
  app_registry_cache_observer_.Reset();
}
