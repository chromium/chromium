// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/lacros/web_app_provider_bridge_lacros.h"

#include "base/files/file_path.h"
#include "base/functional/bind.h"
#include "base/functional/callback_helpers.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/apps/app_service/webapk/webapk_utils.h"
#include "chrome/browser/browser_process.h"
#include "chrome/browser/chromeos/office_web_app/office_web_app.h"
#include "chrome/browser/lacros/profile_loader.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/web_applications/commands/install_preloaded_verified_app_command.h"
#include "chrome/browser/web_applications/locks/all_apps_lock.h"
#include "chrome/browser/web_applications/locks/app_lock.h"
#include "chrome/browser/web_applications/mojom/user_display_mode.mojom.h"
#include "chrome/browser/web_applications/web_app_command_manager.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_constants.h"
#include "chrome/browser/web_applications/web_app_install_finalizer.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_ui_manager.h"
#include "chromeos/crosapi/mojom/web_app_service.mojom.h"
#include "chromeos/crosapi/mojom/web_app_types.mojom.h"
#include "chromeos/crosapi/mojom/web_app_types_mojom_traits.h"
#include "chromeos/lacros/lacros_service.h"
#include "components/webapps/browser/install_result_code.h"
#include "components/webapps/browser/installable/installable_metrics.h"
#include "components/webapps/common/web_app_id.h"
#include "url/gurl.h"

namespace crosapi {

namespace {

webapps::WebappInstallSource GetInstallSourceForPreload(
    mojom::PreloadWebAppInstallSource source) {
  switch (source) {
    case mojom::PreloadWebAppInstallSource::kOemPreload:
      return webapps::WebappInstallSource::PRELOADED_OEM;
    case mojom::PreloadWebAppInstallSource::kDefaultPreload:
      return webapps::WebappInstallSource::PRELOADED_DEFAULT;
  }
}

}  // namespace

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
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::WebAppInstalledInArcImpl,
                     std::move(arc_install_info), std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::WebAppUninstalledInArc(
    const std::string& app_id,
    WebAppUninstalledInArcCallback callback) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::WebAppUninstalledInArcImpl,
                     app_id, std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::GetWebApkCreationParams(
    const std::string& app_id,
    GetWebApkCreationParamsCallback callback) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::GetWebApkCreationParamsImpl,
                     app_id, std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::InstallMicrosoft365(
    InstallMicrosoft365Callback callback) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::InstallMicrosoft365Impl,
                     std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::ScheduleNavigateAndTriggerInstallDialog(
    const GURL& install_url,
    const GURL& origin_url,
    bool is_renderer_initiated) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::
                         ScheduleNavigateAndTriggerInstallDialogImpl,
                     install_url, origin_url, is_renderer_initiated),
      /*can_trigger_fre=*/true);
}

void WebAppProviderBridgeLacros::GetSubAppIds(const webapps::AppId& app_id,
                                              GetSubAppIdsCallback callback) {
  LoadMainProfile(base::BindOnce(&WebAppProviderBridgeLacros::GetSubAppIdsImpl,
                                 app_id, std::move(callback)),
                  /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::GetSubAppToParentMap(
    GetSubAppToParentMapCallback callback) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::GetSubAppToParentMapImpl,
                     std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::InstallPreloadWebApp(
    mojom::PreloadWebAppInstallInfoPtr preload_install_info,
    InstallPreloadWebAppCallback callback) {
  LoadMainProfile(
      base::BindOnce(&WebAppProviderBridgeLacros::InstallPreloadWebAppImpl,
                     std::move(preload_install_info), std::move(callback)),
      /*can_trigger_fre=*/false);
}

void WebAppProviderBridgeLacros::LaunchIsolatedWebAppInstaller(
    const base::FilePath& bundle_path) {
  LoadMainProfile(
      base::BindOnce(
          &WebAppProviderBridgeLacros::LaunchIsolatedWebAppInstallerImpl,
          bundle_path),
      /*can_trigger_fre=*/false);
}

// static
void WebAppProviderBridgeLacros::WebAppInstalledInArcImpl(
    mojom::ArcWebAppInstallInfoPtr arc_install_info,
    WebAppInstalledInArcCallback callback,
    Profile* profile) {
  DCHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  auto install_info = std::make_unique<web_app::WebAppInstallInfo>();
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
  provider->scheduler().RemoveInstallSource(
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
void WebAppProviderBridgeLacros::ScheduleNavigateAndTriggerInstallDialogImpl(
    const GURL& install_url,
    const GURL& origin_url,
    bool is_renderer_initiated,
    Profile* profile) {
  web_app::WebAppProvider* provider =
      web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);
  provider->scheduler().ScheduleNavigateAndTriggerInstallDialog(
      install_url, origin_url, is_renderer_initiated, base::DoNothing());
}

// static
void WebAppProviderBridgeLacros::GetSubAppIdsImpl(const webapps::AppId& app_id,
                                                  GetSubAppIdsCallback callback,
                                                  Profile* profile) {
  DCHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);

  provider->scheduler().ScheduleCallbackWithLock<web_app::AppLock>(
      "WebAppServiceAsh::GetSubApps",
      std::make_unique<web_app::AppLockDescription>(app_id),
      base::BindOnce(
          [](webapps::AppId app_id, web_app::AppLock& lock) {
            return lock.registrar().GetAllSubAppIds(app_id);
          },
          app_id)
          .Then(std::move(callback)));
}

// static
void WebAppProviderBridgeLacros::GetSubAppToParentMapImpl(
    GetSubAppToParentMapCallback callback,
    Profile* profile) {
  CHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);
  CHECK(provider);

  provider->scheduler().ScheduleCallbackWithLock<web_app::AllAppsLock>(
      "WebAppProviderBridgeLacros::GetSubAppToParentMap",
      std::make_unique<web_app::AllAppsLockDescription>(),
      base::BindOnce([](web_app::AllAppsLock& lock) {
        return lock.registrar().GetSubAppToParentMap();
      }).Then(std::move(callback)));
}

// static
void WebAppProviderBridgeLacros::InstallPreloadWebAppImpl(
    mojom::PreloadWebAppInstallInfoPtr preload_install_info,
    InstallPreloadWebAppCallback callback,
    Profile* profile) {
  CHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);

  provider->command_manager().ScheduleCommand(
      std::make_unique<web_app::InstallPreloadedVerifiedAppCommand>(
          GetInstallSourceForPreload(preload_install_info->install_source),
          preload_install_info->document_url,
          preload_install_info->manifest_url, preload_install_info->manifest,
          preload_install_info->expected_app_id, std::move(callback)));
}

// static
void WebAppProviderBridgeLacros::LaunchIsolatedWebAppInstallerImpl(
    const base::FilePath& bundle_path,
    Profile* profile) {
  CHECK(profile);
  auto* provider = web_app::WebAppProvider::GetForWebApps(profile);

  provider->ui_manager().LaunchIsolatedWebAppInstaller(bundle_path);
}

}  // namespace crosapi
