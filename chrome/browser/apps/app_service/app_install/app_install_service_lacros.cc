// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_lacros.h"

#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace apps {

AppInstallServiceLacros::AppInstallServiceLacros(
    crosapi::mojom::AppServiceProxy& remote_crosapi_app_service_proxy)
    : remote_crosapi_app_service_proxy_(remote_crosapi_app_service_proxy) {}
AppInstallServiceLacros::~AppInstallServiceLacros() = default;

void AppInstallServiceLacros::InstallApp(AppInstallSurface surface,
                                         PackageId package_id,
                                         base::OnceClosure callback) {
  auto params = crosapi::mojom::InstallAppParams::New();
  params->surface = [&] {
    using Surface = crosapi::mojom::InstallAppParams::Surface;
    switch (surface) {
      case AppInstallSurface::kAppInstallNavigationThrottle:
        return Surface::kAppInstallNavigationThrottle;
    }
  }();
  params->package_id = package_id.ToString();
  remote_crosapi_app_service_proxy_->InstallApp(
      std::move(params), base::IgnoreArgs<crosapi::mojom::AppInstallResultPtr>(
                             std::move(callback)));
}

}  // namespace apps
