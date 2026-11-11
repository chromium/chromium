// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_result.h"

#include <memory>

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "base/time/time.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/app_list/app_list_controller_delegate.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

AppResult::AppResult(Profile* profile,
                     const std::string& app_id,
                     AppListControllerDelegate* controller,
                     bool is_recommendation,
                     apps::IconLoader* icon_loader)
    : profile_(profile),
      app_id_(app_id),
      controller_(controller),
      icon_loader_(icon_loader),
      app_type_(apps::AppType::kUnknown) {
  SetDisplayType(ash::SearchResultDisplayType::kList);
  SetResultType(ash::AppListSearchResultType::kInstalledApp);
  SetIsRecommendation(is_recommendation);
  SetCategory(Category::kApps);

  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
        app_type_ = update.AppType();

        if (apps_util::IsDisabled(update.Readiness())) {
          SetAccessibleName(l10n_util::GetStringFUTF16(
              IDS_APP_ACCESSIBILITY_BLOCKED_INSTALLED_APP_ANNOUNCEMENT,
              base::UTF8ToUTF16(update.Name())));
        } else if (update.Paused().value_or(false)) {
          SetAccessibleName(l10n_util::GetStringFUTF16(
              IDS_APP_ACCESSIBILITY_PAUSED_INSTALLED_APP_ANNOUNCEMENT,
              base::UTF8ToUTF16(update.Name())));
        }
      });

  // Zero-state recommendations (`DisplayType::kRecentApps`) render grid items
  // from `AppListModel` in `RecentAppsView` and never use `SearchResult`'s
  // icon, so only load icons for queried search results.
  if (!is_recommendation) {
    CallLoadIcon(/*allow_placeholder_icon=*/true);
  }

  SetMetricsType(GetSearchResultType());

  switch (app_type_) {
    case apps::AppType::kChromeApp:
      // TODO(crbug.com/40569217): why do we pass the URL and not the app_id??
      // Can we replace this by the simpler "set_id(app_id)", and therefore
      // pull that out of the switch?
      set_id(extensions::Extension::GetBaseURLFromExtensionId(app_id).spec());
      break;
    default:
      set_id(app_id);
      break;
  }
}

AppResult::~AppResult() = default;

void AppResult::UpdateFromLastLaunchedOrInstalledTime(
    const base::Time& current_time,
    const base::Time& old_time) {
  // |current_time| can be before |old_time| in weird cases such as users
  // playing with their clocks. Handle this gracefully.
  if (current_time < old_time) {
    set_relevance(1.0);
    return;
  }

  const double weeks = (current_time - old_time) / base::Days(7);

  // Set the relevance to a value between 0 and 1. This function decays as the
  // time delta increases and reaches a value of 0.5 at 1 week.
  set_relevance(1 / (weeks + 1));
}

void AppResult::Open(int event_flags) {
  Launch(event_flags,
         (is_recommendation() ? apps::LaunchSource::kFromAppListRecommendation
                              : apps::LaunchSource::kFromAppListQuery));
}

ash::SearchResultType AppResult::GetSearchResultType() const {
  switch (app_type_) {
    case apps::AppType::kArc:
      return ash::PLAY_STORE_APP;
    case apps::AppType::kCrostini:
      return ash::CROSTINI_APP;
    case apps::AppType::kChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
      return ash::EXTENSION_APP;
    case apps::AppType::kRemote:
      return ash::REMOTE_APP;
    case apps::AppType::kBorealis:
      return ash::BOREALIS_APP;
    case apps::AppType::kBruschetta:
      return ash::BRUSCHETTA_APP;
    case apps::AppType::kExtension:
    case apps::AppType::kPluginVm:
    case apps::AppType::kUnknown:
      NOTREACHED();
  }
}

void AppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::LaunchSource::kFromAppListQueryContextMenu);
}

void AppResult::Launch(int event_flags, apps::LaunchSource launch_source) {
  apps::AppServiceProxy* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile());

  // For Crostini apps, non-platform Chrome apps, Web apps, it could be
  // selecting an existing delegate for the app, so call
  // ChromeShelfController's ActivateApp interface. Platform apps or ARC
  // apps, Crostini apps treat activations as a launch. The app can decide
  // whether to show a new window or focus an existing window as it sees fit.
  //
  // TODO(crbug.com/40659878): Move this special case to ExtensionApps,
  // when AppService Instance feature is done.
  bool is_active_app = false;
  proxy->AppRegistryCache().ForOneApp(
      app_id(), [&is_active_app](const apps::AppUpdate& update) {
        if (update.AppType() == apps::AppType::kCrostini ||
            update.AppType() == apps::AppType::kWeb ||
            update.AppType() == apps::AppType::kSystemWeb ||
            (update.AppType() == apps::AppType::kChromeApp &&
             update.IsPlatformApp().value_or(true))) {
          is_active_app = true;
        }
      });
  if (is_active_app) {
    ash::ShelfLaunchSource source =
        is_recommendation() ? ash::LAUNCH_FROM_APP_LIST_RECOMMENDATION
                            : ash::LAUNCH_FROM_APP_LIST_SEARCH;
    ash::ShelfID shelf_id(app_id());
    ash::ShelfModel* model = ChromeShelfController::instance()->shelf_model();
    ash::ShelfItemDelegate* delegate = model->GetShelfItemDelegate(shelf_id);
    if (delegate) {
      delegate->ItemSelected(/*event=*/nullptr,
                             controller_->GetAppListDisplayId(), source,
                             /*callback=*/base::DoNothing(),
                             /*filter_predicate=*/base::NullCallback());
      return;
    }
  }

  proxy->Launch(
      app_id(), event_flags, launch_source,
      std::make_unique<apps::WindowInfo>(controller_->GetAppListDisplayId()));
}

void AppResult::CallLoadIcon(bool allow_placeholder_icon) {
  if (!icon_loader_) {
    return;
  }

  // The releaser is intentionally discarded so that this result does not keep
  // the icon referenced in `icon_loader_`'s cache. Once delivered, the icon is
  // held by this result's metadata, and the cached copy can be swept by
  // `AppSearchDataSource::SweepIconCache()`. Note that if a sweep happens while
  // the load is still in flight, the icon just loaded may be discarded from the
  // cache (it is still delivered to this result) and reloaded by a later
  // search.
  icon_loader_->LoadIcon(
      app_id(), apps::IconType::kStandard, kAppIconDimension,
      allow_placeholder_icon,
      base::BindOnce(&AppResult::OnLoadIcon, weak_ptr_factory_.GetWeakPtr()));
}

void AppResult::OnLoadIcon(apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }

  SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon_value->uncompressed),
                   kAppIconDimension));

  if (icon_value->is_placeholder_icon) {
    CallLoadIcon(/*allow_placeholder_icon=*/false);
  }
}

}  // namespace app_list
