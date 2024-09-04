// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_service/app_install/app_install_service_lacros.h"

#include "base/unguessable_token.h"
#include "chromeos/crosapi/mojom/app_service.mojom.h"
#include "chromeos/crosapi/mojom/app_service_types.mojom.h"
#include "chromeos/lacros/lacros_service.h"

namespace apps {

AppInstallServiceLacros::AppInstallServiceLacros() = default;

AppInstallServiceLacros::~AppInstallServiceLacros() = default;

void AppInstallServiceLacros::InstallAppWithFallback(
    AppInstallSurface surface,
    std::string serialized_package_id,
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
      case AppInstallSurface::kAppInstallUriMallV2:
        return Surface::kAppInstallUriMall;
      case AppInstallSurface::kAppInstallUriGetit:
        return Surface::kAppInstallUriGetit;
      case AppInstallSurface::kAppInstallUriLauncher:
        return Surface::kAppInstallUriLauncher;
      case AppInstallSurface::kAppInstallUriPeripherals:
        return Surface::kAppInstallUriPeripherals;
      case AppInstallSurface::kAppPreloadServiceOem:
      case AppInstallSurface::kAppPreloadServiceDefault:
      case AppInstallSurface::kOobeAppRecommendations:
        // These surfaces are triggered from Ash, not Lacros.
        NOTREACHED_IN_MIGRATION();
        return Surface::kUnknown;
    }
  }();
  params->serialized_package_id = std::move(serialized_package_id);

  chromeos::LacrosService::Get()
      ->GetRemote<crosapi::mojom::AppServiceProxy>()
      ->InstallAppWithFallback(
          std::move(params),
          base::IgnoreArgs<crosapi::mojom::AppInstallResultPtr>(
              std::move(callback)));
}

}  // namespace apps
