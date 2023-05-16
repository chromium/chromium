// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "ash/webui/camera_app_ui/url_constants.h"
#include "ash/webui/connectivity_diagnostics/url_constants.h"
#include "ash/webui/diagnostics_ui/url_constants.h"
#include "ash/webui/firmware_update_ui/url_constants.h"
#include "ash/webui/help_app_ui/url_constants.h"
#include "ash/webui/print_management/url_constants.h"
#include "ash/webui/scanning/url_constants.h"
#include "ash/webui/system_apps/public/system_web_app_type.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/ash/system_web_apps/system_web_app_ui_utils.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
#include "components/services/app_service/public/cpp/app_launch_util.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"
#include "url/url_constants.h"

namespace {

// Various schemes which we use and which are not defined anywhere.
const char kChromeScheme[] = "chrome";
const char kChromeUrlPrefix[] = "chrome://";
const char kChromeUntrustedScheme[] = "chrome-untrusted";

const char kFileManagerHost[] = "file-manager";

// Checks if a given URL is a valid file manager URL (trusted or untrusted).
bool IsFileManagerUrl(const GURL& url) {
  return url.has_host() && url.host() == kFileManagerHost && url.has_scheme() &&
         (url.scheme() == kChromeScheme ||
          url.scheme() == kChromeUntrustedScheme);
}

}  // namespace

namespace crosapi {

UrlHandlerAsh::UrlHandlerAsh() = default;
UrlHandlerAsh::~UrlHandlerAsh() = default;

void UrlHandlerAsh::BindReceiver(
    mojo::PendingReceiver<mojom::UrlHandler> receiver) {
  receivers_.Add(this, std::move(receiver));
}

void UrlHandlerAsh::OpenUrl(const GURL& url) {
  OpenUrlInternal(url);
}

bool UrlHandlerAsh::OpenUrlInternal(const GURL& url) {
  GURL target_url =
      crosapi::gurl_os_handler_utils::GetTargetURLFromLacrosURL(url);
  GURL short_target_url = crosapi::gurl_os_handler_utils::SanitizeAshURL(
      target_url, /*include_path=*/false);

  // Settings will be handled.
  if (short_target_url == GURL(chrome::kChromeUIOSSettingsURL)) {
    chrome::SettingsWindowManager* settings_window_manager =
        chrome::SettingsWindowManager::GetInstance();
    settings_window_manager->ShowChromePageForProfile(
        ProfileManager::GetPrimaryUserProfile(), target_url,
        display::kInvalidDisplayId);
    return true;
  }

  ash::SystemWebAppType app_id;

  // As there are different apps which need to be driven by some URLs, the
  // following code does pick the proper app for a given URL.
  // TODO: As Chrome_web_ui_controller_factory gets refactored, this function
  // should get refactored as well to improve long term stability.
  if (target_url == GURL(chrome::kChromeUIFlagsURL) ||
      target_url == GURL(chrome::kOsUIFlagsURL)) {
    app_id = ash::SystemWebAppType::OS_FLAGS;
    target_url = GURL(chrome::kChromeUIFlagsURL);
  } else if (target_url == GURL(chrome::kChromeUIUntrustedCroshURL)) {
    app_id = ash::SystemWebAppType::CROSH;
  } else if (IsFileManagerUrl(target_url)) {
    app_id = ash::SystemWebAppType::FILE_MANAGER;
  } else if (target_url == GURL(chrome::kChromeUIScanningAppURL)) {
    app_id = ash::SystemWebAppType::SCANNING;
  } else if (target_url == GURL(chrome::kOsUIHelpAppURL)) {
    app_id = ash::SystemWebAppType::HELP;
    target_url = GURL(ash::kChromeUIHelpAppURL);
  } else if (target_url == GURL(chrome::kOsUIPrintManagementAppURL)) {
    app_id = ash::SystemWebAppType::PRINT_MANAGEMENT;
    target_url = GURL(ash::kChromeUIPrintManagementAppUrl);
  } else if (target_url == GURL(chrome::kOsUIConnectivityDiagnosticsAppURL)) {
    app_id = ash::SystemWebAppType::CONNECTIVITY_DIAGNOSTICS;
    target_url = GURL(ash::kChromeUIConnectivityDiagnosticsUrl);
  } else if (target_url == GURL(chrome::kOsUIScanningAppURL)) {
    app_id = ash::SystemWebAppType::SCANNING;
    target_url = GURL(ash::kChromeUIScanningAppUrl);
  } else if (target_url == GURL(chrome::kOsUIDiagnosticsAppURL)) {
    app_id = ash::SystemWebAppType::DIAGNOSTICS;
    target_url = GURL(ash::kChromeUIDiagnosticsAppUrl);
  } else if (target_url == GURL(chrome::kOsUIFirmwareUpdaterAppURL)) {
    app_id = ash::SystemWebAppType::FIRMWARE_UPDATE;
    target_url = GURL(ash::kChromeUIFirmwareUpdateAppURL);
  } else if (short_target_url == GURL(ash::kChromeUICameraAppURL)) {
    app_id = ash::SystemWebAppType::CAMERA;
    target_url = url;
  } else if (ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(
                 target_url)) {
    app_id = ash::SystemWebAppType::OS_URL_HANDLER;
    if (crosapi::gurl_os_handler_utils::IsAshOsUrl(target_url)) {
      target_url =
          GURL(kChromeUrlPrefix +
               crosapi::gurl_os_handler_utils::AshOsUrlHost(target_url));
    }
    absl::optional<ash::SystemWebAppType> swa_type =
        ash::GetCapturingSystemAppForURL(
            ProfileManager::GetPrimaryUserProfile(), target_url);
    if (swa_type.has_value()) {
      return false;
    }
  } else {
    LOG(ERROR) << "Invalid URL passed to UrlHandlerAsh::OpenUrl:" << url;
    return false;
  }

  auto* profile = ProfileManager::GetPrimaryUserProfile();
  ash::SystemAppLaunchParams params;
  params.url = target_url;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  ash::LaunchSystemWebAppAsync(profile, app_id, params,
                               std::make_unique<apps::WindowInfo>(display_id));
  return true;
}

}  // namespace crosapi
