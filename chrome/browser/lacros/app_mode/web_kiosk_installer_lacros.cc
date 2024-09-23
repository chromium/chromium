// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/app_mode/web_kiosk_installer_lacros.h"

#include <utility>

#include "chrome/browser/chromeos/app_mode/kiosk_web_app_install_util.h"
#include "chrome/browser/lacros/app_mode/kiosk_session_service_lacros.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "url/gurl.h"

WebKioskInstallerLacros::WebKioskInstallerLacros(Profile& profile)
    : profile_(profile) {
  auto* service = chromeos::LacrosService::Get();
  if (!service->IsAvailable<crosapi::mojom::WebKioskService>()) {
    return;
  }

  service->GetRemote<crosapi::mojom::WebKioskService>()->BindInstaller(
      receiver_.BindNewPipeAndPassRemoteWithVersion());
}

WebKioskInstallerLacros::~WebKioskInstallerLacros() = default;

void WebKioskInstallerLacros::GetWebKioskInstallState(
    const GURL& url,
    GetWebKioskInstallStateCallback callback) {
  auto [state, app_id] =
      chromeos::GetKioskWebAppInstallState(profile_.get(), url);
  std::move(callback).Run(state, std::move(app_id));

  KioskSessionServiceLacros::Get()->SetInstallUrl(url);
}

void WebKioskInstallerLacros::InstallWebKiosk(
    const GURL& url,
    InstallWebKioskCallback callback) {
  chromeos::InstallKioskWebApp(profile_.get(), url, std::move(callback));
}
