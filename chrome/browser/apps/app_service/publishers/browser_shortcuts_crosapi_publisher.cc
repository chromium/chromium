// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/publishers/browser_shortcuts_crosapi_publisher.h"

#include <memory>
#include <utility>
#include <vector>

#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/logging.h"
#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chromeos/constants/chromeos_features.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut.h"
#include "components/services/app_service/public/cpp/shortcut/shortcut_registry_cache.h"
#include "mojo/public/cpp/bindings/receiver.h"

namespace apps {

BrowserShortcutsCrosapiPublisher::BrowserShortcutsCrosapiPublisher(
    apps::AppServiceProxy* proxy)
    : apps::ShortcutPublisher(proxy), proxy_(proxy) {}

BrowserShortcutsCrosapiPublisher::~BrowserShortcutsCrosapiPublisher() = default;

void BrowserShortcutsCrosapiPublisher::RegisterCrosapiHost(
    mojo::PendingReceiver<crosapi::mojom::AppShortcutPublisher> receiver) {
  // At the moment the app service publisher will only accept one client
  // publishing apps to ash chrome. Any extra clients will be ignored.
  // TODO(crbug.com/1174246): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&BrowserShortcutsCrosapiPublisher::OnCrosapiDisconnected,
                     base::Unretained(this)));
  RegisterShortcutPublisher(apps::AppType::kStandaloneBrowser);
}

void BrowserShortcutsCrosapiPublisher::SetLaunchShortcutCallbackForTesting(
    crosapi::mojom::AppShortcutController::LaunchShortcutCallback callback) {
  launch_shortcut_callback_for_testing_ = std::move(callback);
}

void BrowserShortcutsCrosapiPublisher::SetRemoveShortcutCallbackForTesting(
    crosapi::mojom::AppShortcutController::RemoveShortcutCallback callback) {
  remove_shortcut_callback_for_testing_ = std::move(callback);
}

void BrowserShortcutsCrosapiPublisher::PublishShortcuts(
    std::vector<apps::ShortcutPtr> deltas,
    PublishShortcutsCallback callback) {
  if (!chromeos::features::IsCrosWebAppShortcutUiUpdateEnabled()) {
    std::move(callback).Run();
    return;
  }

  for (auto& delta : deltas) {
    apps::ShortcutPublisher::PublishShortcut(std::move(delta));
  }
  std::move(callback).Run();
}

void BrowserShortcutsCrosapiPublisher::RegisterAppShortcutController(
    mojo::PendingRemote<crosapi::mojom::AppShortcutController> controller,
    RegisterAppShortcutControllerCallback callback) {
  if (controller_.is_bound()) {
    std::move(callback).Run(
        crosapi::mojom::ControllerRegistrationResult::kFailed);
    return;
  }
  controller_.Bind(std::move(controller));
  controller_.set_disconnect_handler(base::BindOnce(
      &BrowserShortcutsCrosapiPublisher::OnControllerDisconnected,
      base::Unretained(this)));
  std::move(callback).Run(
      crosapi::mojom::ControllerRegistrationResult::kSuccess);
}

void BrowserShortcutsCrosapiPublisher::ShortcutRemoved(
    const std::string& shortcut_id,
    ShortcutRemovedCallback callback) {
  apps::ShortcutPublisher::ShortcutRemoved(apps::ShortcutId(shortcut_id));
  std::move(callback).Run();
}

void BrowserShortcutsCrosapiPublisher::LaunchShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int64_t display_id) {
  if (!controller_.is_bound()) {
    LOG(WARNING) << "Controller not connected: " << FROM_HERE.ToString();
    return;
  }

  // TODO(b/308879297): Make launch shortcut async in the publisher interface.
  controller_->LaunchShortcut(
      host_app_id, local_shortcut_id, display_id,
      launch_shortcut_callback_for_testing_
          ? std::move(launch_shortcut_callback_for_testing_)
          : base::DoNothing());
}

void BrowserShortcutsCrosapiPublisher::RemoveShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    apps::UninstallSource uninstall_source) {
  if (!controller_.is_bound()) {
    LOG(WARNING) << "Controller not connected: " << FROM_HERE.ToString();
    return;
  }

  // TODO(b/308879297): Make remove shortcut async in the publisher interface.
  controller_->RemoveShortcut(
      host_app_id, local_shortcut_id, uninstall_source,
      remove_shortcut_callback_for_testing_
          ? std::move(remove_shortcut_callback_for_testing_)
          : base::DoNothing());
}

void BrowserShortcutsCrosapiPublisher::GetCompressedIconData(
    const std::string& shortcut_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  if (!controller_.is_bound()) {
    LOG(WARNING) << "Controller not connected: " << FROM_HERE.ToString();
    return;
  }

  apps::ShortcutId strong_typed_shortcut_id = apps::ShortcutId(shortcut_id);

  std::string host_app_id =
      proxy_->ShortcutRegistryCache()->GetShortcutHostAppId(
          strong_typed_shortcut_id);
  std::string local_shortcut_id =
      proxy_->ShortcutRegistryCache()->GetShortcutLocalId(
          strong_typed_shortcut_id);

  controller_->GetCompressedIcon(host_app_id, local_shortcut_id, size_in_dip,
                                 scale_factor, std::move(callback));
}

void BrowserShortcutsCrosapiPublisher::OnCrosapiDisconnected() {
  receiver_.reset();
  controller_.reset();
}

void BrowserShortcutsCrosapiPublisher::OnControllerDisconnected() {
  controller_.reset();
}

}  // namespace apps
