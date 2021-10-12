// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/web_apps_crosapi.h"

#include <utility>
#include <vector>

#include "ash/public/cpp/app_menu_constants.h"
#include "base/bind.h"
#include "base/callback_helpers.h"
#include "base/location.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/apps/app_service/browser_app_instance_registry.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/menu_util.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/common/chrome_features.h"
#include "chrome/grit/generated_resources.h"
#include "components/services/app_service/public/cpp/crosapi_utils.h"
#include "components/services/app_service/public/cpp/instance_registry.h"
#include "components/services/app_service/public/mojom/types.mojom.h"
#include "extensions/common/constants.h"

namespace apps {

WebAppsCrosapi::WebAppsCrosapi(Profile* profile) : profile_(profile) {
  // This object may be created when the flag is on or off, but only register
  // the publisher if the flag is on.
  if (base::FeatureList::IsEnabled(features::kWebAppsCrosapi)) {
    apps::AppServiceProxyChromeOs* proxy =
        apps::AppServiceProxyFactory::GetForProfile(profile);
    mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
    if (!app_service.is_bound()) {
      return;
    }
    PublisherBase::Initialize(app_service, apps::mojom::AppType::kWeb);
  }
}

WebAppsCrosapi::~WebAppsCrosapi() = default;

void WebAppsCrosapi::RegisterWebAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnCrosapiDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));
  subscribers_.Add(std::move(subscriber));
}

void WebAppsCrosapi::LoadIcon(const std::string& app_id,
                              apps::mojom::IconKeyPtr icon_key,
                              apps::mojom::IconType icon_type,
                              int32_t size_hint_in_dip,
                              bool allow_placeholder_icon,
                              LoadIconCallback callback) {
  if (!LogIfNotConnected(FROM_HERE)) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  if (!icon_key) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  const uint32_t icon_effects = icon_key->icon_effects;
  controller_->LoadIcon(
      app_id, std::move(icon_key), icon_type, size_hint_in_dip,
      base::BindOnce(&WebAppsCrosapi::OnLoadIcon, weak_factory_.GetWeakPtr(),
                     icon_effects, size_hint_in_dip, std::move(callback)));
}

void WebAppsCrosapi::Launch(const std::string& app_id,
                            int32_t event_flags,
                            apps::mojom::LaunchSource launch_source,
                            apps::mojom::WindowInfoPtr window_info) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  controller_->Launch(std::move(launch_params), base::DoNothing());
}

void WebAppsCrosapi::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent =
      apps_util::ConvertAppServiceToCrosapiIntent(intent, profile_);
  controller_->Launch(std::move(launch_params), base::DoNothing());
}

void WebAppsCrosapi::LaunchAppWithFiles(const std::string& app_id,
                                        int32_t event_flags,
                                        apps::mojom::LaunchSource launch_source,
                                        apps::mojom::FilePathsPtr file_paths) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(file_paths);
  controller_->Launch(std::move(launch_params), base::DoNothing());
}

void WebAppsCrosapi::Uninstall(const std::string& app_id,
                               apps::mojom::UninstallSource uninstall_source,
                               bool clear_site_data,
                               bool report_abuse) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void WebAppsCrosapi::GetMenuModel(const std::string& app_id,
                                  apps::mojom::MenuType menu_type,
                                  int64_t display_id,
                                  GetMenuModelCallback callback) {
  bool is_system_web_app = false;
  bool can_use_uninstall = true;
  apps::mojom::WindowMode display_mode;
  auto* proxy = apps::AppServiceProxyFactory::GetForProfile(profile_);

  proxy->AppRegistryCache().ForOneApp(
      app_id, [&is_system_web_app, &can_use_uninstall,
               &display_mode](const apps::AppUpdate& update) {
        if (update.InstallReason() == apps::mojom::InstallReason::kSystem) {
          is_system_web_app = true;
          can_use_uninstall = false;
        } else if (update.InstallReason() ==
                   apps::mojom::InstallReason::kPolicy) {
          can_use_uninstall = false;
        }
        display_mode = update.WindowMode();
      });

  apps::mojom::MenuItemsPtr menu_items = apps::mojom::MenuItems::New();

  if (!is_system_web_app) {
    apps::CreateOpenNewSubmenu(menu_type,
                               display_mode == apps::mojom::WindowMode::kBrowser
                                   ? IDS_APP_LIST_CONTEXT_MENU_NEW_TAB
                                   : IDS_APP_LIST_CONTEXT_MENU_NEW_WINDOW,
                               &menu_items);
  }

  if (menu_type == apps::mojom::MenuType::kShelf) {
    // TODO(crbug.com/1203992): We cannot use InstanceRegistry with lacros yet,
    // because InstanceRegistry updates for lacros isn't implemented yet, so we
    // need to check BrowserAppInstanceRegistry directly. Remove this when
    // InstanceRegistry updates are implemented.
    bool app_running =
        base::FeatureList::IsEnabled(features::kWebAppsCrosapi)
            ? proxy->BrowserAppInstanceRegistry()->IsAppRunning(app_id)
            : proxy->InstanceRegistry().ContainsAppId(app_id);
    if (app_running) {
      apps::AddCommandItem(ash::MENU_CLOSE, IDS_SHELF_CONTEXT_MENU_CLOSE,
                           &menu_items);
    }
  }

  if (can_use_uninstall) {
    apps::AddCommandItem(ash::UNINSTALL, IDS_APP_LIST_UNINSTALL_ITEM,
                         &menu_items);
  }

  if (!is_system_web_app) {
    apps::AddCommandItem(ash::SHOW_APP_INFO, IDS_APP_CONTEXT_MENU_SHOW_INFO,
                         &menu_items);
  }
  if (base::FeatureList::IsEnabled(
          features::kDesktopPWAsAppIconShortcutsMenuUI)) {
    if (!LogIfNotConnected(FROM_HERE)) {
      std::move(callback).Run(std::move(menu_items));
      return;
    }

    controller_->GetMenuModel(
        app_id, base::BindOnce(&WebAppsCrosapi::OnGetMenuModelFromCrosapi,
                               weak_factory_.GetWeakPtr(), app_id, menu_type,
                               std::move(menu_items), std::move(callback)));
  } else {
    std::move(callback).Run(std::move(menu_items));
  }
}

void WebAppsCrosapi::OnGetMenuModelFromCrosapi(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    apps::mojom::MenuItemsPtr menu_items,
    GetMenuModelCallback callback,
    crosapi::mojom::MenuItemsPtr crosapi_menu_items) {
  if (crosapi_menu_items->items.empty()) {
    std::move(callback).Run(std::move(menu_items));
    return;
  }

  auto separator_type = ui::DOUBLE_SEPARATOR;

  for (int item_index = 0; item_index < crosapi_menu_items->items.size();
       item_index++) {
    const auto& crosapi_menu_item = crosapi_menu_items->items[item_index];
    apps::AddSeparator(std::exchange(separator_type, ui::PADDED_SEPARATOR),
                       &menu_items);

    // Uses integer |command_id| to store menu item index.
    const int command_id = ash::LAUNCH_APP_SHORTCUT_FIRST + item_index;

    auto& icon_image = crosapi_menu_item->image;

    icon_image = apps::ApplyBackgroundAndMask(icon_image);

    apps::AddShortcutCommandItem(command_id, crosapi_menu_item->id.value_or(""),
                                 crosapi_menu_item->label, icon_image,
                                 &menu_items);
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

void WebAppsCrosapi::SetWindowMode(const std::string& app_id,
                                   apps::mojom::WindowMode window_mode) {
  if (!LogIfNotConnected(FROM_HERE)) {
    return;
  }

  controller_->SetWindowMode(app_id, window_mode);
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

void WebAppsCrosapi::OnApps(std::vector<apps::mojom::AppPtr> deltas) {
  if (!base::FeatureList::IsEnabled(features::kWebAppsCrosapi))
    return;
  for (auto& subscriber : subscribers_) {
    subscriber->OnApps(apps_util::CloneStructPtrVector(deltas),
                       apps::mojom::AppType::kWeb, should_notify_initialized_);
  }
  should_notify_initialized_ = false;
}

void WebAppsCrosapi::RegisterAppController(
    mojo::PendingRemote<crosapi::mojom::AppController> controller) {
  if (controller_.is_bound()) {
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(base::BindOnce(
      &WebAppsCrosapi::OnControllerDisconnected, base::Unretained(this)));
}

void WebAppsCrosapi::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  if (!base::FeatureList::IsEnabled(features::kWebAppsCrosapi))
    return;
  for (auto& subscriber : subscribers_) {
    subscriber->OnCapabilityAccesses(apps_util::CloneStructPtrVector(deltas));
  }
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
}

void WebAppsCrosapi::OnLoadIcon(uint32_t icon_effects,
                                int size_hint_in_dip,
                                LoadIconCallback callback,
                                apps::mojom::IconValuePtr icon_value) {
  // We apply the masking effect here, as masking is not implemented in Lacros.
  ApplyIconEffects(static_cast<IconEffects>(icon_effects), size_hint_in_dip,
                   std::move(icon_value), std::move(callback));
}

}  // namespace apps
