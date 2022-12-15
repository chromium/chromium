// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include <memory>

#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/commands/install_from_info_command.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"

namespace apps {

WebAppPreloadInstaller::WebAppPreloadInstaller(Profile* profile)
    : profile_(profile) {}

WebAppPreloadInstaller::~WebAppPreloadInstaller() = default;

void WebAppPreloadInstaller::InstallApp(
    const PreloadAppDefinition& app,
    WebAppPreloadInstalledCallback callback) {
  DCHECK_EQ(app.GetPlatform(), AppType::kWeb);

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);
  DCHECK(provider);

  provider->on_registry_ready().Post(
      FROM_HERE,
      base::BindOnce(&WebAppPreloadInstaller::InstallAppImpl,
                     weak_ptr_factory_.GetWeakPtr(), app, std::move(callback)));
}

std::string WebAppPreloadInstaller::GetAppId(
    const PreloadAppDefinition& app) const {
  // The app's "Web app manifest ID" is the equivalent of the unhashed app ID.
  return web_app::GenerateAppIdFromUnhashed(app.GetWebAppManifestId());
}

void WebAppPreloadInstaller::InstallAppImpl(
    PreloadAppDefinition app,
    WebAppPreloadInstalledCallback callback) {
  web_app::WebAppInstallParams params;
  params.add_to_quick_launch_bar = false;

  auto* provider = web_app::WebAppProvider::GetForWebApps(profile_);

  std::unique_ptr<WebAppInstallInfo> info = nullptr;
  if (!info) {
    std::move(callback).Run(/*success=*/false);
    return;
  }

  provider->command_manager().ScheduleCommand(
      std::make_unique<web_app::InstallFromInfoCommand>(
          std::move(info),
          /*overwrite_existing_manifest_fields=*/false,
          webapps::WebappInstallSource::PRELOADED_OEM,
          base::BindOnce(&WebAppPreloadInstaller::OnAppInstalled,
                         weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
          params));
}

void WebAppPreloadInstaller::OnAppInstalled(
    WebAppPreloadInstalledCallback callback,
    const web_app::AppId& app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(webapps::IsSuccess(code));
}

}  // namespace apps
