// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/standalone_browser_test_controller.h"

#include "chrome/browser/apps/app_service/app_service_proxy.h"
#include "chrome/browser/apps/app_service/app_service_proxy_factory.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/web_app_install_manager.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_application_info.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/webapps/browser/installable/installable_metrics.h"

namespace {
blink::mojom::DisplayMode WindowModeToDisplayMode(
    apps::mojom::WindowMode window_mode) {
  switch (window_mode) {
    case apps::mojom::WindowMode::kBrowser:
      return blink::mojom::DisplayMode::kBrowser;
    case apps::mojom::WindowMode::kTabbedWindow:
      return blink::mojom::DisplayMode::kTabbed;
    case apps::mojom::WindowMode::kWindow:
      return blink::mojom::DisplayMode::kStandalone;
    case apps::mojom::WindowMode::kUnknown:
      return blink::mojom::DisplayMode::kUndefined;
  }
}
}  // namespace

StandaloneBrowserTestController::StandaloneBrowserTestController(
    mojo::Remote<crosapi::mojom::TestController>& test_controller) {
  test_controller->RegisterStandaloneBrowserTestController(
      controller_receiver_.BindNewPipeAndPassRemoteWithVersion());
}

StandaloneBrowserTestController::~StandaloneBrowserTestController() = default;

void StandaloneBrowserTestController::InstallWebApp(
    const std::string& start_url,
    apps::mojom::WindowMode window_mode,
    InstallWebAppCallback callback) {
  auto info = std::make_unique<WebApplicationInfo>();
  info->title = u"Test Web App";
  info->start_url = GURL(start_url);
  info->display_mode = WindowModeToDisplayMode(window_mode);
  info->user_display_mode = WindowModeToDisplayMode(window_mode);
  Profile* profile = ProfileManager::GetPrimaryUserProfile();
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->install_manager().InstallWebAppFromInfo(
      std::move(info), /*overwrite_existing_manifest_fields=*/false,
      web_app::ForInstallableSite::kYes,
      /*install_source=*/webapps::WebappInstallSource::SYNC,
      base::BindOnce(&StandaloneBrowserTestController::WebAppInstallationDone,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)));
}

void StandaloneBrowserTestController::WebAppInstallationDone(
    InstallWebAppCallback callback,
    const web_app::AppId& installed_app_id,
    web_app::InstallResultCode code) {
  std::move(callback).Run(code == web_app::InstallResultCode::kSuccessNewInstall
                              ? installed_app_id
                              : "");
}
