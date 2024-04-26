// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/web_app_service_ash.h"

#include <utility>

#include "base/functional/callback.h"
#include "chrome/browser/ash/apps/apk_web_app_service.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "components/sync/model/string_ordinal.h"

namespace crosapi {

WebAppServiceAsh::WebAppServiceAsh() = default;
WebAppServiceAsh::~WebAppServiceAsh() {
  for (auto& observer : observers_) {
    observer.OnWebAppServiceAshDestroyed();
  }
}

void WebAppServiceAsh::AddObserver(Observer* observer) {
  observers_.AddObserver(observer);
}

void WebAppServiceAsh::RemoveObserver(Observer* observer) {
  observers_.RemoveObserver(observer);
}

void WebAppServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::WebAppService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebAppServiceAsh::RegisterWebAppProviderBridge(
    mojo::PendingRemote<mojom::WebAppProviderBridge> web_app_provider_bridge) {
  if (web_app_provider_bridge_.is_bound()) {
    // At the moment only a single registration (from a single client) is
    // supported. The rest will be ignored.
    // TODO(crbug.com/40167449): Support SxS lacros.
    LOG(WARNING) << "WebAppProviderBridge already connected";
    return;
  }
  web_app_provider_bridge_.Bind(std::move(web_app_provider_bridge));
  web_app_provider_bridge_.set_disconnect_handler(base::BindOnce(
      &WebAppServiceAsh::OnBridgeDisconnected, weak_factory_.GetWeakPtr()));
  for (auto& observer : observers_) {
    observer.OnWebAppProviderBridgeConnected();
  }
}

void WebAppServiceAsh::GetAssociatedAndroidPackage(
    const std::string& app_id,
    GetAssociatedAndroidPackageCallback callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  ash::ApkWebAppService* apk_web_app_service =
      ash::ApkWebAppService::Get(profile);
  if (!apk_web_app_service || !apk_web_app_service->IsWebOnlyTwa(app_id)) {
    std::move(callback).Run({});
    return;
  }

  const std::optional<std::string> package_name =
      apk_web_app_service->GetPackageNameForWebApp(app_id);
  const std::optional<std::string> fingerprint =
      apk_web_app_service->GetCertificateSha256Fingerprint(app_id);

  // Any web-only TWA should have an associated package name and fingerprint.
  DCHECK(package_name.has_value());
  DCHECK(fingerprint.has_value());

  auto result = crosapi::mojom::WebAppAndroidPackage::New();
  result->package_name = *package_name;
  result->sha256_fingerprint = *fingerprint;
  std::move(callback).Run(std::move(result));
}

void WebAppServiceAsh::MigrateLauncherState(
    const std::string& from_app_id,
    const std::string& to_app_id,
    MigrateLauncherStateCallback callback) {
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  web_app::WebAppProvider::GetForLocalAppsUnchecked(profile)
      ->ui_manager()
      .MigrateLauncherState(from_app_id, to_app_id, std::move(callback));
}

mojom::WebAppProviderBridge* WebAppServiceAsh::GetWebAppProviderBridge() {
  // At the moment only a single connection is supported.
  // TODO(crbug.com/40167449): Support SxS lacros.
  if (!web_app_provider_bridge_.is_bound()) {
    return nullptr;
  }
  return web_app_provider_bridge_.get();
}

void WebAppServiceAsh::OnBridgeDisconnected() {
  web_app_provider_bridge_.reset();
  for (auto& observer : observers_) {
    observer.OnWebAppProviderBridgeDisconnected();
  }
}

}  // namespace crosapi
