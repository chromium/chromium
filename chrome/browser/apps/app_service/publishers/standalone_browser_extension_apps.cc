// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/standalone_browser_extension_apps.h"

#include <utility>

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

  subscribers_.Add(std::move(subscriber));
  // TODO(https://crbug.com/1225848): Call subscriber->OnApps() with the set of
  // registered apps.
}

void StandaloneBrowserExtensionApps::LoadIcon(const std::string& app_id,
                                              apps::mojom::IconKeyPtr icon_key,
                                              apps::mojom::IconType icon_type,
                                              int32_t size_hint_in_dip,
                                              bool allow_placeholder_icon,
                                              LoadIconCallback callback) {
  // TODO(https://crbug.com/1225848): Implement.
  std::move(callback).Run(apps::mojom::IconValue::New());
}

void StandaloneBrowserExtensionApps::Launch(
    const std::string& app_id,
    int32_t event_flags,
    apps::mojom::LaunchSource launch_source,
    apps::mojom::WindowInfoPtr window_info) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void StandaloneBrowserExtensionApps::GetMenuModel(
    const std::string& app_id,
    apps::mojom::MenuType menu_type,
    int64_t display_id,
    GetMenuModelCallback callback) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
}

void StandaloneBrowserExtensionApps::OnApps(
    std::vector<apps::mojom::AppPtr> deltas) {
  // TODO(https://crbug.com/1225848): Implement.
  NOTIMPLEMENTED();
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
