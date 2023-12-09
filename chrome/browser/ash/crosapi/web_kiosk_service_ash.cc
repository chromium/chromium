// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/web_kiosk_service_ash.h"

#include <optional>

#include "base/logging.h"
#include "chromeos/crosapi/mojom/web_kiosk_service.mojom.h"

namespace crosapi {

WebKioskServiceAsh::WebKioskServiceAsh() = default;
WebKioskServiceAsh::~WebKioskServiceAsh() = default;

void WebKioskServiceAsh::BindReceiver(
    mojo::PendingReceiver<mojom::WebKioskService> pending_receiver) {
  receivers_.Add(this, std::move(pending_receiver));
}

void WebKioskServiceAsh::BindInstaller(
    mojo::PendingRemote<mojom::WebKioskInstaller> installer) {
  installers_.Add(std::move(installer));
}

void WebKioskServiceAsh::GetWebKioskInstallState(
    const GURL& url,
    mojom::WebKioskInstaller::GetWebKioskInstallStateCallback callback) {
  if (!GetInstaller()) {
    LOG(WARNING) << "WebKioskInstallController not present";
    std::move(callback).Run(mojom::WebKioskInstallState::kUnknown, "");
    return;
  }

  GetInstaller()->GetWebKioskInstallState(url, std::move(callback));
}

void WebKioskServiceAsh::InstallWebKiosk(
    const GURL& url,
    mojom::WebKioskInstaller::InstallWebKioskCallback callback) {
  if (!GetInstaller()) {
    LOG(WARNING) << "WebKioskInstallController not present";
    std::move(callback).Run(std::nullopt);
    return;
  }

  GetInstaller()->InstallWebKiosk(url, std::move(callback));
}

mojom::WebKioskInstaller* WebKioskServiceAsh::GetInstaller() {
  if (installers_.empty()) {
    LOG(WARNING) << "Lacros installer has not been bound";
    return nullptr;
  }

  return installers_.begin()->get();
}

}  // namespace crosapi
