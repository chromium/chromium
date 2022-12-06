// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/apps/app_preload_service/preload_app_definition.h"

#include <memory>

#include "base/strings/string_util.h"
#include "base/strings/utf_string_conversions.h"
#include "chrome/browser/web_applications/user_display_mode.h"
#include "chrome/browser/web_applications/web_app_install_info.h"
#include "components/services/app_service/public/cpp/app_types.h"
#include "third_party/blink/public/mojom/manifest/display_mode.mojom.h"

namespace {
blink::mojom::DisplayMode GetDisplayMode(
    apps::proto::AppProvisioningResponse_DisplayMode mode) {
  switch (mode) {
    case apps::proto::AppProvisioningResponse::DISPLAY_MODE_UNKNOWN:
      // If the server sends a value we don't recognise, default to "standalone"
      // display.
      return blink::mojom::DisplayMode::kStandalone;

    case apps::proto::AppProvisioningResponse::DISPLAY_MODE_FULLSCREEN:
      return blink::mojom::DisplayMode::kFullscreen;

    case apps::proto::AppProvisioningResponse::DISPLAY_MODE_STANDALONE:
      return blink::mojom::DisplayMode::kStandalone;

    case apps::proto::AppProvisioningResponse::DISPLAY_MODE_MINIMAL_UI:
      return blink::mojom::DisplayMode::kMinimalUi;

    case apps::proto::AppProvisioningResponse::DISPLAY_MODE_BROWSER:
      return blink::mojom::DisplayMode::kBrowser;
  }
}

}  // namespace

namespace apps {

std::string PreloadAppDefinition::GetName() const {
  return app_proto_.name();
}

AppType PreloadAppDefinition::GetPlatform() const {
  switch (app_proto_.platform()) {
    case proto::AppProvisioningResponse::PLATFORM_UNKNOWN:
      return AppType::kUnknown;
    case proto::AppProvisioningResponse::PLATFORM_WEB:
      return AppType::kWeb;
    case proto::AppProvisioningResponse::PLATFORM_ANDROID:
      return AppType::kArc;
  }
}

bool PreloadAppDefinition::IsOemApp() const {
  return app_proto_.install_reason() ==
         proto::AppProvisioningResponse_InstallReason::
             AppProvisioningResponse_InstallReason_INSTALL_REASON_OEM;
}

std::unique_ptr<WebAppInstallInfo>
PreloadAppDefinition::CreateWebAppInstallInfo() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  auto install_info = std::make_unique<WebAppInstallInfo>();

  install_info->title = base::UTF8ToUTF16(GetName());

  if (!app_proto_.has_web_extras()) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Missing required web_extras.";
    return nullptr;
  }

  auto web_extras = app_proto_.web_extras();

  const std::string& start_url = web_extras.start_url();
  install_info->start_url = GURL(start_url);

  if (!install_info->start_url.is_valid()) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Start URL is invalid: "
               << start_url;
    return nullptr;
  }

  // The server returns a manifest ID which has already been resolved against
  // the Start URL to make the processed manifest ID. WebAppInstallInfo requires
  // the opposite, an ID string which can be added to the base URL to create the
  // processed ID. So we need to 'unresolve' the URL by removing the base URL
  // from it.
  const std::string& manifest_id = web_extras.manifest_id();
  if (!GURL(manifest_id).is_valid()) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Manifest ID is invalid: "
               << manifest_id;
    return nullptr;
  }

  std::string manifest_id_base_url =
      GURL(manifest_id).GetWithEmptyPath().spec();
  DCHECK(base::StartsWith(manifest_id, manifest_id_base_url));

  if (manifest_id_base_url !=
      install_info->start_url.GetWithEmptyPath().spec()) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Manifest ID (" << manifest_id
               << ") does not have same origin as Start URL (" << start_url
               << ")";
    return nullptr;
  }

  install_info->manifest_id = manifest_id.substr(manifest_id_base_url.size());

  const std::string& scope = app_proto_.web_extras().scope();
  install_info->scope = GURL(scope);
  if (!install_info->scope.is_valid()) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Scope is invalid: " << scope;
    return nullptr;
  }
  if (!base::StartsWith(install_info->start_url.path(),
                        install_info->scope.path(),
                        base::CompareCase::SENSITIVE)) {
    LOG(ERROR) << "Failed to convert app " << GetName()
               << " into WebAppInstallInfo. Start URL ("
               << install_info->start_url << ") is not within scope ("
               << install_info->scope.spec();
    return nullptr;
  }

  install_info->display_mode =
      GetDisplayMode(app_proto_.web_extras().display_mode());

  // Always install the app as a web app in a window.
  install_info->user_display_mode = web_app::UserDisplayMode::kStandalone;

  return install_info;
}

std::string PreloadAppDefinition::GetWebAppManifestId() const {
  DCHECK_EQ(GetPlatform(), AppType::kWeb);

  return app_proto_.web_extras().manifest_id();
}

std::ostream& operator<<(std::ostream& os, const PreloadAppDefinition& app) {
  os << "- Name: " << app.GetName();
  os << "- Platform: " << EnumToString(app.GetPlatform());
  os << "- OEM: " << app.IsOemApp();
  return os;
}

}  // namespace apps
