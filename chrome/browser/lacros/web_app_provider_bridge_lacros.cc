// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/web_app_provider_bridge_lacros.h"
#include "base/functional/bind.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/webapk/webapk_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/office_web_app/office_web_app.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_id.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_app_types.mojom.h"
#include "chromeos/crosapi/mojom/web_app_types_mojom_traits.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "url/gurl.h"

namespace crosapi {

WebAppProviderBridgeLacros::WebAppProviderBridgeLacros() {
  auto* service = chromeos::LacrosService::Get();
  if (service->IsAvailable<mojom::WebAppService>()) {
    service->GetRemote<mojom::WebAppService>()->RegisterWebAppProviderBridge(
        receiver_.BindNewPipeAndPassRemote());
  }
}

WebAppProviderBridgeLacros::~WebAppProviderBridgeLacros() = default;

void WebAppProviderBridgeLacros::WebAppInstalledInArc(
    mojom::ArcWebAppInstallInfoPtr arc_install_info,
    WebAppInstalledInArcCallback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(&WebAppProviderBridgeLacros::WebAppInstalledInArcImpl,
                     std::move(arc_install_info), std::move(callback)));
}

void WebAppProviderBridgeLacros::WebAppUninstalledInArc(
    const std::string& app_id,
    WebAppUninstalledInArcCallback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(&WebAppProviderBridgeLacros::WebAppUninstalledInArcImpl,
                     app_id, std::move(callback)));
}

void WebAppProviderBridgeLacros::GetWebApkCreationParams(
    const std::string& app_id,
    GetWebApkCreationParamsCallback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(&WebAppProviderBridgeLacros::GetWebApkCreationParamsImpl,
                     app_id, std::move(callback)));
}

void WebAppProviderBridgeLacros::InstallMicrosoft365(
    InstallMicrosoft365Callback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(&WebAppProviderBridgeLacros::InstallMicrosoft365Impl,
                     std::move(callback)));
}

void WebAppProviderBridgeLacros::GetSubAppIds(const web_app::AppId& app_id,
                                              GetSubAppIdsCallback callback) {
  g_browser_process->profile_manager()->LoadProfileByPath(
      ProfileManager::GetPrimaryUserProfilePath(),
      /*incognito=*/false,
      base::BindOnce(&WebAppProviderBridgeLacros::GetSubAppIdsImpl, app_id,
                     std::move(callback)));
}

// static
void WebAppProviderBridgeLacros::WebAppInstalledInArcImpl(
    mojom::ArcWebAppInstallInfoPtr arc_install_info,
    WebAppInstalledInArcCallback callback,
    Profile* profile) {
  DCHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto install_info = std::make_unique<WebAppInstallInfo>();
  install_info->title = arc_install_info->title;
  install_info->start_url = arc_install_info->start_url;
  install_info->display_mode = blink::mojom::DisplayMode::kStandalone;
  install_info->user_display_mode =
      web_app::mojom::UserDisplayMode::kStandalone;
  install_info->theme_color = arc_install_info->theme_color;
  const SkBitmap& bitmap = *arc_install_info->icon.bitmap();
  install_info->icon_bitmaps.any[bitmap.width()] = bitmap;
  if (arc_install_info->additional_policy_ids) {
    install_info->additional_policy_ids =
        std::move(*arc_install_info->additional_policy_ids);
  }

  provider->scheduler().InstallFromInfo(
      std::move(install_info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::ARC, std::move(callback));
}

// static
void WebAppProviderBridgeLacros::WebAppUninstalledInArcImpl(
    const std::string& app_id,
    WebAppUninstalledInArcCallback callback,
    Profile* profile) {
  DCHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  provider->install_finalizer().UninstallExternalWebApp(
      app_id, web_app::WebAppManagement::kWebAppStore,
      webapps::WebappUninstallSource::kArc, std::move(callback));
}

// static
void WebAppProviderBridgeLacros::GetWebApkCreationParamsImpl(
    const std::string& app_id,
    GetWebApkCreationParamsCallback callback,
    Profile* profile) {
  apps::GetWebApkCreationParams(profile, app_id, std::move(callback));
}

// static
void WebAppProviderBridgeLacros::InstallMicrosoft365Impl(
    InstallMicrosoft365Callback callback,
    Profile* profile) {
  chromeos::InstallMicrosoft365(profile, std::move(callback));
}

// static
void WebAppProviderBridgeLacros::GetSubAppIdsImpl(const web_app::AppId& app_id,
                                                  GetSubAppIdsCallback callback,
                                                  Profile* profile) {
  DCHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);

  provider->scheduler().ScheduleCallbackWithLock<web_app::AppLock>(
      "WebAppServiceAsh::GetSubApps",
      std::make_unique<web_app::AppLockDescription>(app_id),
      base::BindOnce(
          [](web_app::AppId app_id, web_app::AppLock& lock) {
            return lock.registrar().GetAllSubAppIds(app_id);
          },
          app_id)
          .Then(std::move(callback)));
}

}  // namespace crosapi
