// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_lacros.h"

#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/services/app_service/public/cpp/package_id.h"

namespace apps {

AppInstallServiceLacros::AppInstallServiceLacros() = default;

AppInstallServiceLacros::~AppInstallServiceLacros() = default;

void AppInstallServiceLacros::InstallApp(
    AppInstallSurface surface,
    PackageId package_id,
    std::optional<base::UnguessableToken> anchor_window,
    base::OnceClosure callback) {
  auto params = crosapi::mojom::InstallAppParams::New();
  if (anchor_window.has_value()) {
    params->window_id = anchor_window.value();
  }
  params->surface = [&] {
    using Surface = crosapi::mojom::InstallAppParams::Surface;
    switch (surface) {
      case AppInstallSurface::kAppInstallUriUnknown:
        return Surface::kAppInstallUriUnknown;
      case AppInstallSurface::kAppInstallUriShowoff:
        return Surface::kAppInstallUriShowoff;
      case AppInstallSurface::kAppInstallUriMall:
        return Surface::kAppInstallUriMall;
      case AppInstallSurface::kAppInstallUriGetit:
        return Surface::kAppInstallUriGetit;
      case AppInstallSurface::kAppInstallUriLauncher:
        return Surface::kAppInstallUriLauncher;
      case AppInstallSurface::kAppPreloadServiceOem:
      case AppInstallSurface::kAppPreloadServiceDefault:
        // Preloads should be installed from Ash, not Lacros.
        NOTREACHED();
        return Surface::kUnknown;
    }
  }();
  params->package_id = package_id.ToString();

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppServiceProxy>()
      ->InstallApp(std::move(params),
                   base::IgnoreArgs<crosapi::mojom::AppInstallResultPtr>(
                       std::move(callback)));
}

}  // namespace apps
