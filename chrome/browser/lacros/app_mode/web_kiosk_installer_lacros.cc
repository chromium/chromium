// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/web_kiosk_installer_lacros.h"

#include <memory>
#include <utility>

#include "base/functional/callback_helpers.h"
#include "chrome/browser/chromeos/app_mode/web_kiosk_app_installer.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"

namespace {

template <typename T>
auto GetDeletePointerCallback(std::unique_ptr<T> ptr) {
  return base::BindOnce(
      [](std::unique_ptr<T>) {
        // Do nothing. Callback is solely here to ensure the unique pointer gets
        // deleted.
      },
      std::move(ptr));
}

}  // namespace

WebKioskInstallerLacros::WebKioskInstallerLacros(Profile& profile)
    : profile_(profile) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::WebKioskService>()) {
    return;
  }

  service->GetRemote<crosapi::mojom::WebKioskService>()->BindInstaller(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

WebKioskInstallerLacros::~WebKioskInstallerLacros() {}

void WebKioskInstallerLacros::GetWebKioskInstallState(
    const GURL& url,
    GetWebKioskInstallStateCallback callback) {
  // web_installer is a self-owned object so that multiple parallel calls can be
  // handled.
  auto web_installer =
      std::make_unique<chromeos::WebKioskAppInstaller>(profile_.get(), url);
  web_installer->GetInstallState(std::move(callback).Then(
      GetDeletePointerCallback(std::move(web_installer))));
}

void WebKioskInstallerLacros::InstallWebKiosk(
    const GURL& url,
    InstallWebKioskCallback callback) {
  // web_installer is a self-owned object so that multiple parallel calls can be
  // handled.
  auto web_installer =
      std::make_unique<chromeos::WebKioskAppInstaller>(profile_.get(), url);
  web_installer->InstallApp(std::move(callback).Then(
      GetDeletePointerCallback(std::move(web_installer))));
}
