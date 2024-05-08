// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/app_service/app_service_app_item.h"

#include "ash/constants/ash_features.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/public/cpp/shelf_types.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "base/metrics/histogram_functions.h"
#include "base/notreached.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/package_id_util.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/app_service/app_service_context_menu.h"
#include "chrome/browser/ash/app_list/apps_collections_util.h"
#include "chrome/browser/ash/crostini/crostini_util.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager.h"
#include "chrome/browser/ash/remote_apps/remote_apps_manager_factory.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "ui/display/screen.h"

namespace {

// Parameters used by the time duration metrics.
constexpr base::TimeDelta kTimeMetricsMin = base::Seconds(1);
constexpr base::TimeDelta kTimeMetricsMax = base::Days(1);
constexpr int kTimeMetricsBucketCount = 100;

// Returns true if `app_update` should be considered a new app install.
bool IsNewInstall(const apps::AppUpdate& app_update) {
  // Ignore internally-installed apps.
  if (app_update.InstalledInternally())
    return false;

  switch (app_update.AppType()) {
    case apps::AppType::kUnknown:
    case apps::AppType::kBuiltIn:
    case apps::AppType::kStandaloneBrowser:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kRemote:
      // Chrome, Lacros, Settings, etc. are built-in.
      return false;
    case apps::AppType::kArc:
    case apps::AppType::kCrostini:
    case apps::AppType::kChromeApp:
    case apps::AppType::kExtension:
    case apps::AppType::kWeb:
    case apps::AppType::kPluginVm:
    case apps::AppType::kBorealis:
    case apps::AppType::kBruschetta:
    case apps::AppType::kStandaloneBrowserChromeApp:
    case apps::AppType::kStandaloneBrowserExtension:
      // Other app types are user-installed.
      return true;
  }
}

}  // namespace

// static
const char AppServiceAppItem::kItemType[] = "AppServiceAppItem";

AppServiceAppItem::AppServiceAppItem(
    Profile* profile,
    AppListModelUpdater* model_updater,
    const app_list::AppListSyncableService::SyncItem* sync_item,
    const apps::AppUpdate& app_update)
    : ChromeAppListItem(profile, app_update.AppId()),
      app_type_(app_update.AppType()),
      creation_time_(base::TimeTicks::Now()) {
  OnAppUpdate(app_update, /*in_constructor=*/true);
  if (sync_item && sync_item->item_ordinal.IsValid()) {
    InitFromSync(sync_item);
  } else {
    // Handle the case that the app under construction is a remote app.
    if (app_type_ == apps::AppType::kRemote) {
      SetIsEphemeral(true);
      ash::RemoteAppsManager* remote_manager =
          ash::RemoteAppsManagerFactory::GetForProfile(profile);
      if (remote_manager->ShouldAddToFront(app_update.AppId())) {
        SetPosition(model_updater->GetPositionBeforeFirstItem());
      } else {
        // Add the app at the end of the app list to preserve behavior from
        // before productivity launcher, so the positions in which remote apps
        // are added are consistent with old launcher order (which may be
        // assumed by extensions using remote apps API).
        SetPosition(model_updater->GetFirstAvailablePosition());
      }

      const ash::RemoteAppsModel::AppInfo* app_info =
          remote_manager->GetAppInfo(app_update.AppId());
      if (app_info)
        SetChromeFolderId(app_info->folder_id);
    }

    // Crostini and Bruschetta apps start in their respective folders.
    if (app_type_ == apps::AppType::kCrostini) {
      DCHECK(folder_id().empty());
      SetChromeFolderId(ash::kCrostiniFolderId);
    } else if (app_type_ == apps::AppType::kBruschetta) {
      DCHECK(folder_id().empty());
      SetChromeFolderId(ash::kBruschettaFolderId);
    }
  }

  std::optional<apps::PackageId> package_id =
      apps_util::GetPackageIdForApp(profile, app_update);
  if (package_id.has_value()) {
    SetPromisePackageId(package_id.value().ToString());
  }

  SetCollectionId(apps_util::GetCollectionIdForAppId(app_update.AppId()));

  const bool is_new_install =
      (!sync_item || sync_item->is_new) && IsNewInstall(app_update);
  DVLOG(1) << "New AppServiceAppItem is_new_install " << is_new_install
           << " from update " << app_update;
  SetIsNewInstall(is_new_install);

  // Set model updater last to avoid being called during construction.
  set_model_updater(model_updater);
}

AppServiceAppItem::~AppServiceAppItem() = default;

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update) {
  OnAppUpdate(app_update, /*in_constructor=*/false);
}

void AppServiceAppItem::OnAppUpdate(const apps::AppUpdate& app_update,
                                    bool in_constructor) {
  if (in_constructor || app_update.ShortNameChanged()) {
    // We display the short name rather than the full name here since
    // each launcher item only has a limited space.
    SetName(app_update.ShortName());
  }

  if (in_constructor || app_update.IconKeyChanged()) {
    // Increment icon version to indicate icon needs to be updated.
    IncrementIconVersion();
  }

  if (in_constructor || app_update.IsPlatformAppChanged()) {
    is_platform_app_ = app_update.IsPlatformApp().value_or(false);
  }

  if (in_constructor || app_update.ReadinessChanged() ||
      app_update.PausedChanged()) {
    if (apps_util::IsDisabled(app_update.Readiness())) {
      SetAppStatus(ash::AppStatus::kBlocked);
    } else if (app_update.Paused().value_or(false)) {
      SetAppStatus(ash::AppStatus::kPaused);
    } else {
      SetAppStatus(ash::AppStatus::kReady);
    }
  }
}

void AppServiceAppItem::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::LaunchSource::kFromAppListGridContextMenu);

  // TODO(crbug.com/40569217): drop the if, and call MaybeDismissAppList
  // unconditionally?
  if (app_type_ == apps::AppType::kArc || app_type_ == apps::AppType::kRemote) {
    MaybeDismissAppList();
  }
}

void AppServiceAppItem::LoadIcon() {
  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(allow_placeholder_icon);
}

void AppServiceAppItem::Activate(int event_flags) {
  // For Crostini apps, non-platform Chrome apps, Web apps, it could be
  // selecting an existing delegate for the app, so call
  // ChromeShelfController's ActivateApp interface. Platform apps or ARC
  // apps, Crostini apps treat activations as a launch. The app can decide
  // whether to show a new window or focus an existing window as it sees fit.
  //
  // TODO(crbug.com/40106663): Move the Chrome special case to ExtensionApps,
  // when AppService Instance feature is done.
  bool is_active_app = false;
  apps::AppServiceProxyFactory::GetForProfile(profile())
      ->AppRegistryCache()
      .ForOneApp(id(), [&is_active_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kCrostini ||
            update.AppType() == apps::AppType::kWeb ||
            update.AppType() == apps::AppType::kSystemWeb ||
            (update.AppType() == apps::AppType::kStandaloneBrowserChromeApp &&
             !update.IsPlatformApp().value_or(true)) ||
            (update.AppType() == apps::AppType::kChromeApp &&
             update.IsPlatformApp().value_or(true))) {
          is_active_app = true;
        }
      });
  if (is_active_app) {
    ash::ShelfID shelf_id(id());
    ash::ShelfModel* model = ChromeShelfController::instance()->shelf_model();
    ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(shelf_id);
    if (delegate) {
      ResetIsNewInstall();
      delegate->ItemSelected(
          /*event=*/nullptr, GetController()->GetAppListDisplayId(),
          ash::LAUNCH_FROM_APP_LIST, /*callback=*/base::DoNothing(),
          /*filter_predicate=*/base::NullCallback());
      return;
    }
  }
  Launch(event_flags, apps::LaunchSource::kFromAppListGrid);
}

const char* AppServiceAppItem::GetItemType() const {
  return AppServiceAppItem::kItemType;
}

void AppServiceAppItem::GetContextMenuModel(
    ash::AppListItemContext item_context,
    GetMenuModelCallback callback) {
  context_menu_ = std::make_unique<AppServiceContextMenu>(
      this, profile(), id(), GetController(), item_context);

  context_menu_->GetMenuModel(std::move(callback));
}

app_list::AppContextMenu* AppServiceAppItem::GetAppContextMenu() {
  return context_menu_.get();
}

void AppServiceAppItem::ResetIsNewInstall() {
  if (!is_new_install())
    return;
  SetIsNewInstall(false);

  // Record metric for approximate time from installation to launch.
  base::TimeDelta time_since_install = base::TimeTicks::Now() - creation_time_;
  if (display::Screen::GetScreen()->InTabletMode()) {
    base::UmaHistogramCustomTimes(
        "Apps.TimeBetweenAppInstallAndLaunch.TabletMode", time_since_install,
        kTimeMetricsMin, kTimeMetricsMax, kTimeMetricsBucketCount);
  } else {
    base::UmaHistogramCustomTimes(
        "Apps.TimeBetweenAppInstallAndLaunch.ClamshellMode", time_since_install,
        kTimeMetricsMin, kTimeMetricsMax, kTimeMetricsBucketCount);
  }
}

void AppServiceAppItem::Launch(int event_flags,
                               apps::LaunchSource launch_source) {
  ResetIsNewInstall();
  apps::AppServiceProxyFactory::GetForProfile(profile())->Launch(
      id(), event_flags, launch_source,
      std::make_unique<apps::WindowInfo>(
          GetController()->GetAppListDisplayId()));
}

void AppServiceAppItem::CallLoadIcon(bool allow_placeholder_icon) {
  apps::AppServiceProxyFactory::GetForProfile(profile())->LoadIcon(
      id(), apps::IconType::kStandard,
      ash::SharedAppListConfig::instance().default_grid_icon_dimension(),
      allow_placeholder_icon,
      base::BindOnce(&AppServiceAppItem::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr()));
}

void AppServiceAppItem::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }
  SetIcon(icon_value->uncompressed, icon_value->is_placeholder_icon);

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(allow_placeholder_icon);
  }
}
