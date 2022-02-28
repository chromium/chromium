// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"

#include <utility>

#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_icon/app_icon_factory.h"
#include "chrome/browser/apps/app_service/app_launch_params.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/intent_util.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "components/app_restore/app_launch_info.h"
#include "components/app_restore/features.h"
#include "components/app_restore/full_restore_utils.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

StandaloneBrowserExtensionApps::StandaloneBrowserExtensionApps(
    AppServiceProxy* proxy)
    : apps::AppPublisher(proxy) {
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }
  PublisherBase::Initialize(app_service,
                            apps::mojom::AppType::kStandaloneBrowserChromeApp);
}

StandaloneBrowserExtensionApps::~StandaloneBrowserExtensionApps() = default;

void StandaloneBrowserExtensionApps::RegisterChromeAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
  RegisterPublisher(AppType::kStandaloneBrowserChromeApp);
  apps::AppPublisher::Publish(std::vector<AppPtr>{},
                              AppType::kStandaloneBrowserChromeApp,
                              /*should_notify_initialized=*/true);

  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&StandaloneBrowserExtensionApps::OnReceiverDisconnected,
                     weak_factory_.GetWeakPtr()));
}

void StandaloneBrowserExtensionApps::RegisterKeepAlive() {
  keep_alive_ = crosapi::BrowserManager::Get()->KeepAlive(
      crosapi::BrowserManager::Feature::kChromeApps);
}

void StandaloneBrowserExtensionApps::LoadIcon(const std::string& app_id,
                                              const IconKey& icon_key,
                                              IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              bool allow_placeholder_icon,
                                              apps::LoadIconCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(std::make_unique<IconValue>());
    return;
  }

  IconType crosapi_icon_type = icon_type;
  IconKeyPtr crosapi_icon_key = icon_key.Clone();
  if (crosapi_icon_type == apps::IconType::kCompressed) {
    // If the request is for a compressed icon, modify request so that
    // uncompressed icon is sent over crosapi.
    crosapi_icon_type = apps::IconType::kUncompressed;
    crosapi_icon_key->icon_effects = apps::IconEffects::kNone;

    // To compensate for the above, wrap |callback| icon recompression. This is
    // applied after OnLoadIcon() runs, which is appropriate since OnLoadIcon()
    // needs an uncompressed icon for ApplyIconEffects().
    callback = base::BindOnce(
        [](apps::LoadIconCallback wrapped_callback, IconValuePtr icon_value) {
          ConvertUncompressedIconToCompressedIcon(std::move(icon_value),
                                                  std::move(wrapped_callback));
        },
        std::move(callback));
  }

  const uint32_t icon_effects = icon_key.icon_effects;
  controller_->LoadIcon(
      app_id, std::move(crosapi_icon_key), crosapi_icon_type, size_hint_in_dip,
      base::BindOnce(&StandaloneBrowserExtensionApps::OnLoadIcon,
                     weak_factory_.GetWeakPtr(), icon_effects, size_hint_in_dip,
                     std::move(callback)));
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

  if (!::full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      params.app_id, params.container, params.disposition, params.display_id,
      std::move(params.launch_files), std::move(params.intent));
  full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                  std::move(launch_info));
}

void StandaloneBrowserExtensionApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));

  mojo::RemoteSetElementId id = subscribers_.Add(std::move(subscriber));

  std::vector<apps::mojom::AppPtr> apps;
  for (auto& it : app_ptr_cache_) {
    apps.push_back(it.second.Clone());
  }

  subscribers_.Get(id)->OnApps(
      std::move(apps), apps::mojom::AppType::kStandaloneBrowserChromeApp,
      true /* should_notify_initialized */);
}

void StandaloneBrowserExtensionApps::LoadIcon(const std::string& app_id,
                                              apps::mojom::IconKeyPtr icon_key,
                                              apps::mojom::IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              bool allow_placeholder_icon,
                                              LoadIconCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(apps::mojom::IconValue::New());
    return;
  }

  controller_->LoadIcon(app_id, ConvertMojomIconKeyToIconKey(icon_key),
                        ConvertMojomIconTypeToIconType(icon_type),
                        size_hint_in_dip,
                        IconValueToMojomIconValueCallback(std::move(callback)));
}

void StandaloneBrowserExtensionApps::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  crosapi::mojom::LaunchParamsPtr params = crosapi::mojom::LaunchParams::New();
  params->app_id = app_id;
  params->launch_source = launch_source;
  controller_->Launch(std::move(params), /*callback=*/base::DoNothing());

  if (!::full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
      std::vector<base::FilePath>{}, nullptr);
  full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                  std::move(launch_info));
}

void StandaloneBrowserExtensionApps::LaunchAppWithIntent(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::IntentPtr intent,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info,
    LaunchAppWithIntentCallback callback) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound()) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent = apps_util::ConvertAppServiceToCrosapiIntent(
      intent, ProfileManager::GetPrimaryUserProfile());
  controller_->Launch(std::move(launch_params),
                      /*callback=*/base::DoNothing());
  std::move(callback).Run(/*success=*/true);

  if (!::full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
      std::vector<base::FilePath>{}, std::move(intent));
  full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                  std::move(launch_info));
}

void StandaloneBrowserExtensionApps::LaunchAppWithFiles(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::FilePathsPtr file_paths) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  auto launch_params = crosapi::mojom::LaunchParams::New();
  launch_params->app_id = app_id;
  launch_params->launch_source = launch_source;
  launch_params->intent =
      apps_util::CreateCrosapiIntentForViewFiles(file_paths);
  controller_->Launch(std::move(launch_params), /*callback=*/base::DoNothing());

  if (!::full_restore::features::IsFullRestoreForLacrosEnabled())
    return;

  auto launch_info = std::make_unique<app_restore::AppLaunchInfo>(
      app_id, apps::mojom::LaunchContainer::kLaunchContainerNone,
      WindowOpenDisposition::UNKNOWN, display::kInvalidDisplayId,
      std::move(file_paths->file_paths), nullptr);
  full_restore::SaveAppLaunchInfo(proxy()->profile()->GetPath(),
                                  std::move(launch_info));
}

void StandaloneBrowserExtensionApps::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    GetMenuModelCallback callback) {
  // The current implementation of chrome apps menu models never uses the
  // AppService GetMenuModel method. We always returns an empty array here.
  std::move(callback).Run(mojom::MenuItems::New());
}

void StandaloneBrowserExtensionApps::StopApp(const std::string& app_id) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  controller_->StopApp(app_id);
}
void StandaloneBrowserExtensionApps::Uninstall(
    const std::string& app_id,
    apps::mojom::UninstallSource uninstall_source,
    bool clear_site_data,
    bool report_abuse) {
  // It is possible that Lacros is briefly unavailable, for example if it shuts
  // down for an update.
  if (!controller_.is_bound())
    return;

  controller_->Uninstall(app_id, uninstall_source, clear_site_data,
                         report_abuse);
}

void StandaloneBrowserExtensionApps::OnApps(std::vector<AppPtr> deltas) {
  if (deltas.empty()) {
    return;
  }

  for (const AppPtr& delta : deltas) {
    app_ptr_cache_[delta->app_id] = ConvertAppToMojomApp(delta);
    PublisherBase::Publish(ConvertAppToMojomApp(delta), subscribers_);
  }

  apps::AppPublisher::Publish(std::move(deltas),
                              AppType::kStandaloneBrowserChromeApp,
                              /*should_notify_initialized=*/false);
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
}

void StandaloneBrowserExtensionApps::OnCapabilityAccesses(
    std::vector<apps::mojom::CapabilityAccessPtr> deltas) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void StandaloneBrowserExtensionApps::OnReceiverDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void StandaloneBrowserExtensionApps::OnControllerDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void StandaloneBrowserExtensionApps::OnLoadIcon(uint32_t icon_effects,
                                                int size_hint_in_dip,
                                                apps::LoadIconCallback callback,
                                                IconValuePtr icon_value) {
  // Apply masking effects here since masking is unimplemented in Lacros.
  ApplyIconEffects(static_cast<IconEffects>(icon_effects), size_hint_in_dip,
                   std::move(icon_value), std::move(callback));
}

}  // namespace apps
