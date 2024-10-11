// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include <memory>
#include <utility>
#include <vector>

#include "ash/constants/ash_features.h"
#include "ash/constants/web_app_id_constants.h"
#include "ash/public/cpp/app_menu_constants.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager.h"
#include "chrome/browser/apps/almanac_api_client/device_info_manager_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/apps/app_service/promise_apps/promise_app_web_apps_utils.h"
#include "chrome/browser/apps/browser_instance/browser_app_instance_registry.h"
#include "chrome/browser/ash/mall/mall_url.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/grit/generated_resources.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/crosapi_utils.h"
#include "components/services/app_service/public/cpp/features.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/cpp/intent_util.h"
#include "extensions/common/constants.h"

namespace apps {

WebAppsCrosapi::WebAppsCrosapi(AppServiceProxy* proxy)
    : apps::AppPublisher(proxy), proxy_(proxy) {}

WebAppsCrosapi::~WebAppsCrosapi() = default;

void WebAppsCrosapi::RegisterWebAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::GetCompressedIconData(const std::string& app_id,
                                           int32_t size_in_dip,
                                           ui::ResourceScaleFactor scale_factor,
                                           LoadIconCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  controller_->GetCompressedIcon(app_id, size_in_dip, scale_factor,
                                 std::move(callback));
}

void WebAppsCrosapi::Launch(const std::string& app_id,
                            int32_t event_flags,
                            LaunchSource launch_source,
                            WindowInfoPtr window_info) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  // Redirect launches of the Mall app so that we can add additional context to
  // the URL. Loading the context will cause a slight delay on first launch, but
  // it is then cached in the DeviceInfoManager for subsequent launches.
  // TODO(b/331702863): Remove this custom integration.
  if (chromeos::features::IsCrosMallWebAppEnabled() &&
      app_id == web_app::kMallAppId) {
    apps::DeviceInfoManager* device_info_manager =
        apps::DeviceInfoManagerFactory::GetForProfile(proxy_->profile());
    CHECK(device_info_manager);
    device_info_manager->GetDeviceInfo(base::BindOnce(
        &WebAppsCrosapi::LaunchMallWithContext, weak_factory_.GetWeakPtr(),
        event_flags, launch_source, std::move(window_info)));
    return;
  }

  controller_->Launch(
      CreateCrosapiLaunchParamsWithEventFlags(
          proxy_, app_id, event_flags, launch_source,
          window_info ? window_info->display_id : display::kInvalidDisplayId),
      base::DoNothing());
}

void WebAppsCrosapi::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    LaunchSource launch_source,
    std::vector<base::FilePath> file_paths) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      proxy_, app_id, event_flags, launch_source, display::kInvalidDisplayId);
  params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(std::move(file_paths));
  controller_->Launch(std::move(params), base::DoNothing());
}

void WebAppsCrosapi::LaunchAppWithIntent(const std::string& app_id,
                                         int32_t event_flags,
                                         IntentPtr intent,
                                         LaunchSource launch_source,
                                         WindowInfoPtr window_info,
                                         LaunchCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(LaunchResult(State::kFailed));
    return;
  }

  auto params = CreateCrosapiLaunchParamsWithEventFlags(
      proxy_, app_id, event_flags, launch_source,
      window_info ? window_info->display_id : display::kInvalidDisplayId);

  params->intent =
      apps_util::ConvertAppServiceToCrosapiIntent(intent, proxy_->profile());
  controller_->Launch(std::move(params), base::DoNothing());
  // TODO(crbug.com/40202131): handle the case where launch fails.
  std::move(callback).Run(LaunchResult(State::kSuccess));
}

void WebAppsCrosapi::LaunchAppWithParams(AppLaunchParams&& params,
                                         LaunchCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(LaunchResult());
    return;
  }
  controller_->Launch(
      apps::ConvertLaunchParamsToCrosapi(params, proxy_->profile()),
      apps::LaunchResultToMojomLaunchResultCallback(std::move(callback)));
}

void WebAppsCrosapi::LaunchShortcut(const std::string& app_id,
                                    const std::string& shortcut_id,
                                    int64_t display_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->ExecuteContextMenuCommand(app_id, shortcut_id,
                                         base::DoNothing());
}

void WebAppsCrosapi::SetPermission(const std::string& app_id,
                                   PermissionPtr permission) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->SetPermission(app_id, std::move(permission));
}

void WebAppsCrosapi::Uninstall(const std::string& app_id,
                               UninstallSource uninstall_source,
                               bool clear_site_data,
                               bool report_abuse) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void WebAppsCrosapi::GetMenuModel(
    const std::string& app_id,
    MenuType menu_type,
    int64_t display_id,
    base::OnceCallback<void(MenuItems)> callback) {
  bool is_system_web_app = false;
  bool can_use_uninstall = false;
  bool can_close = true;
  bool allow_window_mode_selection = true;
  WindowMode display_mode = WindowMode::kUnknown;

  proxy_->AppRegistryCache().ForOneApp(
      app_id,
      [&is_system_web_app, &allow_window_mode_selection, &can_use_uninstall,
       &can_close, &display_mode](const AppUpdate& update) {
        is_system_web_app = update.InstallReason() == InstallReason::kSystem;
        allow_window_mode_selection =
            update.AllowWindowModeSelection().value_or(true);
        can_use_uninstall = update.AllowUninstall().value_or(false);
        can_close = update.AllowClose().value_or(true);
        display_mode = update.WindowMode();
      });

  MenuItems menu_items;

  if (display_mode != WindowMode::kUnknown && !is_system_web_app && can_close) {
    if (!allow_window_mode_selection) {
      apps::AddCommandItem(ash::LAUNCH_NEW,
                           IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW, menu_items);
    } else {
      CreateOpenNewSubmenu(display_mode == WindowMode::kBrowser
                               ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                               : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                           menu_items);
    }
  }

  if (ShouldAddCloseItem(app_id, menu_type, proxy_->profile())) {
    AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE, menu_items);
  }

  if (can_use_uninstall) {
    AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM, menu_items);
  }

  if (!is_system_web_app) {
    AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                   menu_items);
  }

  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  controller_->GetMenuModel(
      app_id, base::BindOnce(&WebAppsCrosapi::OnGetMenuModelFromCrosapi,
                             weak_factory_.GetWeakPtr(), app_id, menu_type,
                             std::move(menu_items), std::move(callback)));
}

void WebAppsCrosapi::UpdateAppSize(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }
  controller_->UpdateAppSize(app_id);
}

void WebAppsCrosapi::SetWindowMode(const std::string& app_id,
                                   WindowMode window_mode) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->SetWindowMode(app_id, window_mode);
}

void WebAppsCrosapi::OnGetMenuModelFromCrosapi(
    const std::string& app_id,
    MenuType menu_type,
    MenuItems menu_items,
    base::OnceCallback<void(MenuItems)> callback,
    crosapi::mojom::MenuItemsPtr crosapi_menu_items) {
  if (crosapi_menu_items->items.empty()) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  auto separator_type = ui::DOUBLE_SEPARATOR;
  const int crosapi_menu_items_size = crosapi_menu_items->items.size();

  for (int item_index = 0; item_index < crosapi_menu_items_size; item_index++) {
    const auto& crosapi_menu_item = crosapi_menu_items->items[item_index];
    AddSeparator(std::exchange(separator_type, ui::PADDED_SEPARATOR),
                 menu_items);

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + item_index;

    auto& icon_image = crosapi_menu_item->image;

    icon_image = ApplyBackgroundAndMask(icon_image);

    AddShortcutCommandItem(command_id, crosapi_menu_item->id.value_or(""),
                           crosapi_menu_item->label, icon_image, menu_items);
  }

  std::move(callback).Run(std::move(menu_items));
}

void WebAppsCrosapi::PauseApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->PauseApp(app_id);
}

void WebAppsCrosapi::UnpauseApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->UnpauseApp(app_id);
}

void WebAppsCrosapi::StopApp(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->StopApp(app_id);
}

void WebAppsCrosapi::OpenNativeSettings(const std::string& app_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->OpenNativeSettings(app_id);
}

void WebAppsCrosapi::ExecuteContextMenuCommand(const std::string& app_id,
                                               int command_id,
                                               const std::string& shortcut_id,
                                               int64_t display_id) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->ExecuteContextMenuCommand(app_id, shortcut_id,
                                         base::DoNothing());
}

void WebAppsCrosapi::OnApps(std::vector<AppPtr> deltas) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }

  on_initial_apps_received_ = true;

  if (!controller_.is_bound()) {
    // If `controller_` is not bound, add `deltas` to `delta_app_cache_` to wait
    // for registering the crosapi controller to publish all deltas saved in
    // `delta_app_cache_`.
    for (auto& delta : deltas) {
      delta_app_cache_.push_back(std::move(delta));
    }
    return;
  }

  PublishImpl(std::move(deltas));
}

void WebAppsCrosapi::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  DCHECK(web_app::IsWebAppsCrosapiEnabled());
  if (controller_.is_bound()) {
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnControllerDisconnected, base::Unretained(this)));
  RegisterPublisher(AppType::kWeb);

  if (on_initial_apps_received_) {
    PublishImpl(std::move(delta_app_cache_));
    delta_app_cache_.clear();
  }

  if (!delta_capability_access_cache_.empty()) {
    PublishCapabilityAccessesImpl(std::move(delta_capability_access_cache_));
    delta_capability_access_cache_.clear();
  }
}

void WebAppsCrosapi::OnCapabilityAccesses(
    std::vector<CapabilityAccessPtr> deltas) {
  if (!web_app::IsWebAppsCrosapiEnabled()) {
    return;
  }

  if (!controller_.is_bound()) {
    // If `controller_` is not bound, add `deltas` to
    // `delta_capability_access_cache_` to wait for registering the crosapi
    // controller to publish all deltas saved in
    // `delta_capability_access_cache_`.
    for (auto& delta : deltas) {
      delta_capability_access_cache_.push_back(std::move(delta));
    }
    return;
  }

  PublishCapabilityAccessesImpl(std::move(deltas));
}

bool WebAppsCrosapi::LogIfNotConnected(const base::Location& from_here) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.

  if (controller_.is_bound()) {
    return true;
  }
  LOG(WARNING) << "Controller not connected: " << from_here.ToString();
  return false;
}

void WebAppsCrosapi::OnCrosapiDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void WebAppsCrosapi::OnControllerDisconnected() {
  controller_.reset();

  // If Lacros stops running (e.g. due to a crash/update), all apps will no
  // longer be accessing capabilities.
  ResetCapabilityAccess(AppType::kWeb);
}

void WebAppsCrosapi::PublishImpl(std::vector<AppPtr> deltas) {
  // This is for prototyping and testing only. It is to provide an easy way to
  // simulate web app promise icon behaviour for the UI/ client development of
  // web app promise icons.
  // TODO(b/261907269): Remove this code snippet and use real listeners for web
  // app installation events.
  if (ash::features::ArePromiseIconsForWebAppsEnabled()) {
    for (auto& delta : deltas) {
      apps::MaybeSimulatePromiseAppInstallationEvents(proxy(), delta.get());
    }
  }
  apps::AppPublisher::Publish(std::move(deltas), AppType::kWeb,
                              should_notify_initialized_);
  should_notify_initialized_ = false;
}

void WebAppsCrosapi::PublishCapabilityAccessesImpl(
    std::vector<CapabilityAccessPtr> deltas) {
  proxy()->OnCapabilityAccesses(std::move(deltas));
}

void WebAppsCrosapi::LaunchMallWithContext(int32_t event_flags,
                                           apps::LaunchSource launch_source,
                                           apps::WindowInfoPtr window_info,
                                           apps::DeviceInfo device_info) {
  LaunchAppWithIntent(
      web_app::kMallAppId, event_flags,
      std::make_unique<apps::Intent>(apps_util::kIntentActionView,
                                     ash::GetMallLaunchUrl(device_info)),
      launch_source, std::move(window_info), base::DoNothing());
}

}  // namespace apps
