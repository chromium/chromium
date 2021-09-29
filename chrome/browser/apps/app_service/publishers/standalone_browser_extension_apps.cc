// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"

#include <utility>

#include "base/callback_helpers.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/crosapi/browser_util.h"
#include "components/services/app_service/public/mojom/types.mojom.h"

namespace apps {

StandaloneBrowserExtensionApps::StandaloneBrowserExtensionApps(
    Profile* profile) {
  apps::AppServiceProxyChromeOs* proxy =
      apps::AppServiceProxyFactory::GetForProfile(profile);
  mojo::Remote<apps::mojom::AppService>& app_service = proxy->AppService();
  if (!app_service.is_bound()) {
    return;
  }
  PublisherBase::Initialize(app_service,
                            apps::mojom::AppType::kStandaloneBrowserExtension);
}

StandaloneBrowserExtensionApps::~StandaloneBrowserExtensionApps() = default;

void StandaloneBrowserExtensionApps::RegisterChromeAppsCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppPublisher> receiver) {
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

void StandaloneBrowserExtensionApps::Connect(
    mojo::PendingRemote<apps::mojom::Subscriber> subscriber_remote,
    apps::mojom::ConnectOptionsPtr opts) {
  mojo::Remote<apps::mojom::Subscriber> subscriber(
      std::move(subscriber_remote));

  mojo::RemoteSetElementId id = subscribers_.Add(std::move(subscriber));

  if (app_ptr_cache_.empty())
    return;

  std::vector<apps::mojom::AppPtr> apps;
  for (auto& it : app_ptr_cache_) {
    apps.push_back(it.second.Clone());
  }

  subscribers_.Get(id)->OnApps(
      std::move(apps), apps::mojom::AppType::kStandaloneBrowserExtension,
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

  controller_->LoadIcon(app_id, std::move(icon_key), std::move(icon_type),
                        size_hint_in_dip, std::move(callback));
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

void StandaloneBrowserExtensionApps::OnApps(
    std::vector<apps::mojom::AppPtr> deltas) {
  for (apps::mojom::AppPtr& delta : deltas) {
    app_ptr_cache_[delta->app_id] = delta.Clone();
    Publish(std::move(delta), subscribers_);
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

}  // namespace apps
