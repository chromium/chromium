// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/app_list/search/app_service_app_result.h"

#include "ash/public/cpp/app_list/app_list_types.h"
#include "ash/public/cpp/shelf_item_delegate.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/strings/grit/ash_strings.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/metrics/app_service_metrics.h"
#include "chrome/browser/ash/app_list/app_list_client_impl.h"
#include "chrome/browser/ash/app_list/search/common/icon_constants.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/shelf/chrome_shelf_controller.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/app_update.h"
#include "components/services/app_service/public/cpp/types_util.h"
#include "extensions/common/extension.h"
#include "ui/base/l10n/l10n_util.h"

namespace app_list {

AppServiceAppResult::AppServiceAppResult(Profile* profile,
                                         const std::string& app_id,
                                         AppListControllerDelegate* controller,
                                         bool is_recommendation,
                                         apps::IconLoader* icon_loader)
    : AppResult(profile, app_id, controller, is_recommendation),
      icon_loader_(icon_loader),
      app_type_(apps::AppType::kUnknown),
      is_platform_app_(false),
      show_in_launcher_(false) {
  apps::AppServiceProxyFactory::GetForProfile(profile)
      ->AppRegistryCache()
      .ForOneApp(app_id, [this](const apps::AppUpdate& update) {
        app_type_ = update.AppType();
        is_platform_app_ = update.IsPlatformApp().value_or(false);
        show_in_launcher_ = update.ShowInLauncher().value_or(false);

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

  constexpr bool allow_placeholder_icon = true;
  CallLoadIcon(false, allow_placeholder_icon);
  if (is_recommendation) {
    CallLoadIcon(true, allow_placeholder_icon);
  }

  SetMetricsType(GetSearchResultType());
  SetCategory(Category::kApps);

  switch (app_type_) {
    case apps::AppType::kBuiltIn:
      set_id(app_id);
      // TODO(crbug.com/40569217): Is this SetResultType call necessary?? Does
      // anyone care about the kInternalApp vs kInstalledApp distinction?
      SetResultType(ResultType::kInternalApp);
      break;
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

AppServiceAppResult::~AppServiceAppResult() = default;

void AppServiceAppResult::Open(int event_flags) {
  Launch(event_flags,
         (is_recommendation() ? apps::LaunchSource::kFromAppListRecommendation
                              : apps::LaunchSource::kFromAppListQuery));
}

ash::SearchResultType AppServiceAppResult::GetSearchResultType() const {
  switch (app_type_) {
    case apps::AppType::kArc:
      return ash::PLAY_STORE_APP;
    case apps::AppType::kBuiltIn:
      return ash::INTERNAL_APP;
    case apps::AppType::kPluginVm:
      return ash::PLUGIN_VM_APP;
    case apps::AppType::kCrostini:
      return ash::CROSTINI_APP;
    case apps::AppType::kChromeApp:
    case apps::AppType::kWeb:
    case apps::AppType::kSystemWeb:
    case apps::AppType::kStandaloneBrowserChromeApp:
      return ash::EXTENSION_APP;
    case apps::AppType::kStandaloneBrowser:
      return ash::LACROS;
    case apps::AppType::kRemote:
      return ash::REMOTE_APP;
    case apps::AppType::kBorealis:
      return ash::BOREALIS_APP;
    case apps::AppType::kBruschetta:
      return ash::BRUSCHETTA_APP;
    case apps::AppType::kExtension:
    case apps::AppType::kStandaloneBrowserExtension:
    case apps::AppType::kUnknown:
      NOTREACHED_IN_MIGRATION();
      return ash::SEARCH_RESULT_TYPE_BOUNDARY;
  }
}

void AppServiceAppResult::ExecuteLaunchCommand(int event_flags) {
  Launch(event_flags, apps::LaunchSource::kFromAppListQueryContextMenu);
}

void AppServiceAppResult::Launch(int event_flags,
                                 apps::LaunchSource launch_source) {
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
            (update.AppType() == apps::AppType::kStandaloneBrowserChromeApp &&
             !update.IsPlatformApp().value_or(true)) ||
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
                             controller()->GetAppListDisplayId(), source,
                             /*callback=*/base::DoNothing(),
                             /*filter_predicate=*/base::NullCallback());
      return;
    }
  }

  proxy->Launch(
      app_id(), event_flags, launch_source,
      std::make_unique<apps::WindowInfo>(controller()->GetAppListDisplayId()));
}

void AppServiceAppResult::CallLoadIcon(bool chip, bool allow_placeholder_icon) {
  if (!icon_loader_) {
    return;
  }

  // If |icon_loader_releaser_| is non-null, assigning to it will signal to
  // |icon_loader_| that the previous icon is no longer being used, as a hint
  // that it could be flushed from any caches.
  icon_loader_releaser_ = icon_loader_->LoadIcon(
      app_id(), apps::IconType::kStandard, kAppIconDimension,
      allow_placeholder_icon,
      base::BindOnce(&AppServiceAppResult::OnLoadIcon,
                     weak_ptr_factory_.GetWeakPtr(), chip));
}

void AppServiceAppResult::OnLoadIcon(bool chip, apps::IconValuePtr icon_value) {
  if (!icon_value || icon_value->icon_type != apps::IconType::kStandard) {
    return;
  }

  if (chip) {
    SetChipIcon(icon_value->uncompressed);
  } else {
    SetIcon(IconInfo(ui::ImageModel::FromImageSkia(icon_value->uncompressed),
                     kAppIconDimension));
  }

  if (icon_value->is_placeholder_icon) {
    constexpr bool allow_placeholder_icon = false;
    CallLoadIcon(chip, allow_placeholder_icon);
  }
}

}  // namespace app_list
