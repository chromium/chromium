// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "ash/webui/help_app_ui/url_constants.h"
#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/browser_window.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/ui/webui/chrome_web_ui_controller_factory.h"
#include "chrome/browser/web_applications/system_web_apps/system_web_app_types.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "chromeos/crosapi/cpp/gurl_os_handler_utils.h"
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

// Show a chrome:// (os://) app for a given URL.
void ShowOsAppForProfile(Profile* profile,
                         const GURL& gurl,
                         web_app::SystemAppType app_type) {
  // Use the original (non off-the-record) profile for a Chrome URL unless
  // this is a guest session.
  if (!profile->IsGuestSession() && profile->IsOffTheRecord())
    profile = profile->GetOriginalProfile();

  // If this profile isn't allowed to create browser windows (e.g. the login
  // screen profile) then bail out.
  if (Browser::GetCreationStatusForProfile(profile) !=
      Browser::CreationStatus::kOk) {
    return;
  }

  Browser* browser = web_app::FindSystemWebAppBrowser(profile, app_type,
                                                      Browser::TYPE_APP, gurl);
  if (browser) {
    // If there is a matching browser we simply activate it and be done!
    browser->window()->Activate();
    return;
  }

  web_app::SystemAppLaunchParams params;
  params.url = gurl;
  int64_t display_id =
      display::Screen::GetScreen()->GetDisplayForNewWindows().id();
  web_app::LaunchSystemWebAppAsync(profile, app_type, params,
                                   apps::MakeWindowInfo(display_id));
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

  web_app::SystemAppType app_id;

  // As there are different apps which need to be driven by some URLs, the
  // following code does pick the proper app for a given URL.
  // TODO: As Chrome_web_ui_controller_factory gets refactored, this function
  // should get refactored as well to improve long term stability.
  if (target_url == GURL(chrome::kChromeUIFlagsURL) ||
      target_url == GURL(chrome::kOsUIFlagsURL)) {
    app_id = web_app::SystemAppType::OS_FLAGS;
    target_url = GURL(chrome::kChromeUIFlagsURL);
  } else if (target_url == GURL(chrome::kChromeUIUntrustedCroshURL)) {
    app_id = web_app::SystemAppType::CROSH;
  } else if (IsFileManagerUrl(target_url)) {
    app_id = web_app::SystemAppType::FILE_MANAGER;
  } else if (target_url == GURL(chrome::kChromeUIScanningAppURL)) {
    app_id = web_app::SystemAppType::SCANNING;
  } else if (target_url == GURL(chrome::kOsUIHelpAppURL)) {
    app_id = web_app::SystemAppType::HELP;
    target_url = GURL(ash::kChromeUIHelpAppURL);
  } else if (ChromeWebUIControllerFactory::GetInstance()->CanHandleUrl(
                 target_url)) {
    app_id = web_app::SystemAppType::OS_URL_HANDLER;
    if (crosapi::gurl_os_handler_utils::IsAshOsUrl(target_url)) {
      target_url =
          GURL(kChromeUrlPrefix +
               crosapi::gurl_os_handler_utils::AshOsUrlHost(target_url));
    }
  } else {
    LOG(ERROR) << "Invalid URL passed to UrlHandlerAsh::OpenUrl:" << url;
    return false;
  }
  ShowOsAppForProfile(ProfileManager::GetPrimaryUserProfile(), target_url,
                      app_id);
  return true;
}

}  // namespace crosapi
