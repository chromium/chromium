// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/web_app_preload_installer.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_command_scheduler.h"
#include "chrome/browser/web_applications/web_app_helpers.h"
#include "chrome/browser/web_applications/web_app_install_params.h"
#include "chrome/browser/web_applications/web_app_provider.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "components/webapps/browser/install_result_code.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace apps {

WebAppPreloadInstaller::WebAppPreloadInstaller(Profile* profile)
    : profile_(profile) {}

WebAppPreloadInstaller::~WebAppPreloadInstaller() = default;

std::unique_ptr<WebAppInstallInfo>
WebAppPreloadInstaller::ManifestToWebAppInstallInfo(
    GURL document_url,
    GURL original_manifest_url,
    base::Value::Dict& manifest) {
  auto install_info = std::make_unique<WebAppInstallInfo>();

  // Shared error message
  constexpr char kSharedError[] =
      "Unable to convert manifest into WebAppInstallInfo. Field that was not "
      "defined or was the wrong type: ";

  if (!document_url.is_valid()) {
    VLOG(1) << kSharedError << "Document URL";
    return nullptr;
  }

  if (!original_manifest_url.is_valid()) {
    VLOG(1) << kSharedError << "Original Manifest URL";
    return nullptr;
  }

  // Manifest keys
  constexpr char kName[] = "name";
  constexpr char kStartUrl[] = "start_url";
  constexpr char kManifestId[] = "id";
  constexpr char kScope[] = "scope";

  // Title
  auto* title = manifest.FindString(kName);
  if (!title) {
    VLOG(1) << kSharedError << kName;
    return nullptr;
  }
  install_info->title = base::UTF8ToUTF16(*title);

  // Start URL
  auto* start_url = manifest.FindString(kStartUrl);
  if (!start_url) {
    VLOG(1) << kSharedError << kStartUrl;
    return nullptr;
  }
  install_info->start_url = original_manifest_url.Resolve(*start_url);
  if (!install_info->start_url.is_valid()) {
    VLOG(1) << kSharedError << kStartUrl << ". Value: " << *start_url;
    return nullptr;
  }
  if (!url::IsSameOriginWith(document_url, install_info->start_url)) {
    VLOG(1) << kSharedError << kStartUrl << ". Value: " << *start_url
            << ". Mismatch in origin with document url: " << document_url;
    return nullptr;
  }

  // Manifest ID
  auto* manifest_id = manifest.FindString(kManifestId);
  if (manifest_id) {
    GURL start_url_origin = install_info->start_url.GetWithEmptyPath();
    GURL processed_id = start_url_origin.Resolve(*manifest_id);
    if (processed_id.is_valid() &&
        url::IsSameOriginWith(start_url_origin, processed_id)) {
      install_info->manifest_id = processed_id.spec().substr(
          processed_id.GetWithEmptyPath().spec().size());
    }
  }

  // Scope
  auto* scope = manifest.FindString(kScope);
  if (scope) {
    install_info->scope = original_manifest_url.Resolve(*scope);
  }

  if (install_info->scope.is_empty() || !install_info->scope.is_valid() ||
      !url::IsSameOriginWith(install_info->start_url, install_info->scope) ||
      !base::StartsWith(install_info->start_url.path(),
                        install_info->scope.path(),
                        base::CompareCase::SENSITIVE)) {
    install_info->scope = install_info->start_url;
  }

  // Display mode
  install_info->display_mode = blink::mojom::DisplayMode::kStandalone;
  install_info->user_display_mode = web_app::UserDisplayMode::kStandalone;

  return install_info;
}

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

  provider->scheduler().InstallFromInfoWithParams(
      std::move(info),
      /*overwrite_existing_manifest_fields=*/false,
      webapps::WebappInstallSource::PRELOADED_OEM,
      base::BindOnce(&WebAppPreloadInstaller::OnAppInstalled,
                     weak_ptr_factory_.GetWeakPtr(), std::move(callback)),
      params);
}

void WebAppPreloadInstaller::OnAppInstalled(
    WebAppPreloadInstalledCallback callback,
    const web_app::AppId& app_id,
    webapps::InstallResultCode code) {
  std::move(callback).Run(webapps::IsSuccess(code));
}

}  // namespace apps
