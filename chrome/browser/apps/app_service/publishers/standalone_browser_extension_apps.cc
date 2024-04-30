// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"

#include <utility>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/check.h"
#include "base/feature_list.h"
#include "base/functional/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/intent.h"

namespace apps {

namespace {

// Returns true if app's launch info should be saved to full restore.
bool ShouldSaveToFullRestore(AppServiceProxy* proxy,
                             const std::string& app_id) {
  if (!::full_restore::features::IsFullRestoreForLacrosEnabled()) {
    return false;
  }

  bool is_platform_app = true;
  proxy->AppRegistryCache().ForOneApp(
      app_id, [&is_platform_app](const apps::AppUpdate& update) {
        is_platform_app = update.IsPlatformApp().value_or(true);
      });
  // Note: Hosted apps will be restored by Lacros session restoration. No need
  // to save launch info to full restore. Only platform app's launch info should
  // be saved to full restore.
  return is_platform_app;
}

}  // namespace

StandaloneBrowserExtensionApps::StandaloneBrowserExtensionApps(
    AppServiceProxy* proxy,
    AppType app_type)
    : apps::AppPublisher(proxy), app_type_(app_type) {
  login_observation_.Observe(ash::LoginState::Get());
  // Check now in case login has already happened.
  LoggedInStateChanged();
}

StandaloneBrowserExtensionApps::~StandaloneBrowserExtensionApps() = default;

void StandaloneBrowserExtensionApps::RegisterCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  // At the moment the app service publisher will only accept one browser client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&StandaloneBrowserExtensionApps::OnReceiverDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void StandaloneBrowserExtensionApps::GetCompressedIconData(
    const std::string& app_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    LoadIconCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  controller_->GetCompressedIcon(app_id, size_in_dip, scale_factor,
                                 std::move(callback));
}

void StandaloneBrowserExtensionApps::Launch(const std::string& app_id,
                                            int32_t event_flags,
                                            LaunchSource launch_source,
                                            WindowInfoPtr window_info) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  // The following code assumes |app_type_| must be
  // AppType::kStandaloneBrowserChromeApp. Therefore, the app must be either
  // platform app or hosted app.
  // In the future, this class is possible to be instantiated with other
  // AppType, please make sure to modify the logic if necessary.
  controller_->Launch(
      CreateCrosapiLaunchParamsWithEventFlags(
          proxy(), app_id, event_flags, launch_source,
          window_info ? window_info->display_id : display::kInvalidDisplayId),
      /*callback=*/base::DoNothing());

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::vector<base::FilePath>{}, nullptr);
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  std::vector<base::FilePath> file_paths_for_restore = file_paths;
  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(std::move(file_paths));
  controller_->Launch(std::move(launch_params),
                      /*callback=*/base::DoNothing());

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::move(file_paths_for_restore), nullptr);
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    IntentPtr intent,
    LaunchSource launch_source,
    WindowInfoPtr window_info,
    LaunchCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
      intent, ProfileManager::GetPrimaryUserProfile());
  controller_->Launch(std::move(launch_params),
                      /*callback=*/base::DoNothing());
  std::move(callback).Run(LaunchResult(State::kSuccess));

  if (ShouldSaveToFullRestore(proxy(), app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        app_id, apps::LaunchContainer::kLaunchContainerNone,
        WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
        std::vector<base::FilePath>{}, std::move(intent));
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::LaunchAppWithParams(
    AppLaunchParams&& params,
    LaunchCallback callback) {
  if (!controller_.is_bound()) {
    std::move(callback).Run(LaunchResult());
    return;
  }

  controller_->Launch(
      apps::ConvertLaunchParamsToCrosapi(
          params, ProfileManager::GetPrimaryUserProfile()),
      apps::LaunchResultToMojomLaunchResultCallback(std::move(callback)));

  if (ShouldSaveToFullRestore(proxy(), params.app_id)) {
    auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
        params.app_id, params.container, params.disposition, params.display_id,
        std::move(params.launch_files), std::move(params.intent));
    full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                    std::move(launch_info));
  }
}

void StandaloneBrowserExtensionApps::Uninstall(const std::string& app_id,
                                               UninstallSource uninstall_source,
                                               bool clear_site_data,
                                               bool report_abuse) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void StandaloneBrowserExtensionApps::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  bool is_platform_app = true;
  bool can_use_uninstall = false;
  bool show_app_info = false;
  WindowMode display_mode = WindowMode::kUnknown;
  proxy()->AppRegistryCache().ForOneApp(
      app_id, [&is_platform_app, &can_use_uninstall, &show_app_info,
               &display_mode](const apps::AppUpdate& update) {
        is_platform_app = update.IsPlatformApp().value_or(true);
        can_use_uninstall = update.AllowUninstall().value_or(false);
        show_app_info = update.ShowInManagement().value_or(false);
        display_mode = update.WindowMode();
      });

  // This provides the context menu for hosted app in standalone browser.
  // Note: The context menu for platform app in standalone browser is provided
  // by StandaloneBrowserExtensionAppContextMenu.
  DCHECK(!is_platform_app);

  MenuItems menu_items;
  CreateOpenNewSubmenu(display_mode == WindowMode::kWindow
                           ? IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW
                           : IDS_APP_LIST_CONTEXT_MENU_NEW_TAB,
                       menu_items);

  if (menu_type == MenuType::kShelf) {
    if (proxy()->BrowserAppInstanceRegistry()->IsAppRunning(app_id)) {
      AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
    }
  }

  if (can_use_uninstall) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (show_app_info) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void StandaloneBrowserExtensionApps::SetWindowMode(const std::string& app_id,
                                                   WindowMode window_mode) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  controller_->SetWindowMode(app_id, window_mode);
}

void StandaloneBrowserExtensionApps::StopApp(const std::string& app_id) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  controller_->StopApp(app_id);
}

void StandaloneBrowserExtensionApps::UpdateAppSize(const std::string& app_id) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  controller_->UpdateAppSize(app_id);
}

void StandaloneBrowserExtensionApps::OpenNativeSettings(
    const std::string& app_id) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    return;
  }

  controller_->OpenNativeSettings(app_id);
}

void StandaloneBrowserExtensionApps::OnApps(std::vector<AppPtr> deltas) {
  if (deltas.empty()) {
    return;
  }

  if (controller_.is_bound()) {
    apps::AppPublisher::Publish(std::move(deltas), app_type_,
                                should_notify_initialized_);
    should_notify_initialized_ = false;
  } else {
    // If `controller_` is not bound, add `deltas` to `app_cache_` to wait for
    // registering the crosapi controller to publish all deltas saved in
    // `app_cache_`.
    for (AppPtr& delta : deltas) {
      app_cache_[delta->app_id] = std::move(delta);
    }
  }
}

void StandaloneBrowserExtensionApps::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  if (controller_.is_bound()) {
    return;
  }

  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(
      base::BindOnce(&StandaloneBrowserExtensionApps::OnControllerDisconnected,
                     weak_factory_.GetWeakPtr()));
  RegisterPublisher(app_type_);

  if (app_cache_.empty()) {
    // If there is no apps saved in `app_cache_`, still publish an empty app
    // list to initialize `app_type_`.
    apps::AppPublisher::Publish(std::vector<AppPtr>{}, app_type_,
                                should_notify_initialized_);
  } else {
    std::vector<AppPtr> deltas;
    for (auto& it : app_cache_) {
      deltas.push_back(std::move(it.second));
    }
    app_cache_.clear();
    apps::AppPublisher::Publish(std::move(deltas), app_type_,
                                should_notify_initialized_);
  }
  should_notify_initialized_ = false;
}

void StandaloneBrowserExtensionApps::OnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  proxy()->OnCapabilityAccesses(std::move(deltas));
}

void StandaloneBrowserExtensionApps::LoggedInStateChanged() {
  if (ash::LoginState::Get()->IsUserLoggedIn()) {
    if (!keep_alive_) {
      if (app_type_ == AppType::kStandaloneBrowserChromeApp) {
        keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
            crosapi::BrowserManager::Feature::kChromeApps);
      } else if (app_type_ == AppType::kStandaloneBrowserExtension) {
        keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
            crosapi::BrowserManager::Feature::kExtensions);
      }
    }
  } else {
    keep_alive_.reset();
  }
}

void StandaloneBrowserExtensionApps::OnReceiverDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void StandaloneBrowserExtensionApps::OnControllerDisconnected() {
  receiver_.reset();
  controller_.reset();
}

}  // namespace apps
