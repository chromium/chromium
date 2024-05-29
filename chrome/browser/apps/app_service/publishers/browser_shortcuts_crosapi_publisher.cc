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
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (receiver_.is_bound()) {
    return;
  }
  receiver_.Bind(std::move(receiver));
  receiver_.set_disconnect_handler(
      base::BindOnce(&BrowserShortcutsCrosapiPublisher::OnCrosapiDisconnected,
                     base::Unretained(this)));
  RegisterShortcutPublisher(apps::AppType::kStandaloneBrowser);
}

void BrowserShortcutsCrosapiPublisher::PublishShortcuts(
    std::vector<apps::ShortcutPtr> deltas,
    PublishShortcutsCallback callback) {
  // Deprecated
  std::move(callback).Run();
}

void BrowserShortcutsCrosapiPublisher::RegisterAppShortcutController(
    mojo::PendingRemote<crosapi::mojom::AppShortcutController> controller,
    RegisterAppShortcutControllerCallback callback) {
  // Deprecated
  std::move(callback).Run(
      crosapi::mojom::ControllerRegistrationResult::kFailed);
}

void BrowserShortcutsCrosapiPublisher::ShortcutRemoved(
    const std::string& shortcut_id,
    ShortcutRemovedCallback callback) {
  // Deprecated
  std::move(callback).Run();
}

void BrowserShortcutsCrosapiPublisher::LaunchShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    int64_t display_id) {
  // TODO(b/341640372)
}

void BrowserShortcutsCrosapiPublisher::RemoveShortcut(
    const std::string& host_app_id,
    const std::string& local_shortcut_id,
    apps::UninstallSource uninstall_source) {
  // TODO(b/341640372)
}

void BrowserShortcutsCrosapiPublisher::GetCompressedIconData(
    const std::string& shortcut_id,
    int32_t size_in_dip,
    ui::ResourceScaleFactor scale_factor,
    apps::LoadIconCallback callback) {
  // TODO(b/341640372)
}

void BrowserShortcutsCrosapiPublisher::OnCrosapiDisconnected() {
  receiver_.reset();
}

}  // namespace apps
