// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/crosapi/url_handler_ash.h"

#include "chrome/browser/apps/app_service/launch_utils.h"
#include "chrome/browser/profiles/profile_manager.h"
#include "chrome/browser/ui/settings_window_manager_chromeos.h"
#include "chrome/browser/ui/web_applications/system_web_app_ui_utils.h"
#include "chrome/browser/web_applications/web_app_utils.h"
#include "chrome/common/webui_url_constants.h"
#include "ui/display/display.h"
#include "ui/display/screen.h"

namespace {

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
  // Settings will be handled.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIOSSettingsURL).DeprecatedGetOriginAsURL()) {
    chrome::SettingsWindowManager* settings_window_manager =
        chrome::SettingsWindowManager::GetInstance();
    settings_window_manager->ShowChromePageForProfile(
        ProfileManager::GetPrimaryUserProfile(), url,
        display::kInvalidDisplayId);
    return;
  }

  // The following two handlers are mapping to system OS URls which have their
  // own favicon and with it their own place in the shelf.

  // Handle the os://flags and/or chrome://flags url as an app in Ash using a
  // special icon and shelf seat.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIFlagsURL).DeprecatedGetOriginAsURL()) {
    ShowOsAppForProfile(ProfileManager::GetPrimaryUserProfile(), url,
                        web_app::SystemAppType::OS_FLAGS);
    return;
  }

  // Handle the os://crosh and/or chrome-untrusted://crosh url as an app.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIOsCroshAppURL).DeprecatedGetOriginAsURL()) {
    ShowOsAppForProfile(ProfileManager::GetPrimaryUserProfile(),
                        GURL(chrome::kChromeUIUntrustedCroshURL),
                        web_app::SystemAppType::CROSH);
    return;
  }

  // Handle a list of os://<url> and/or chrome://<url> url's, combined in one
  // icon and shelf seat.
  // TODO(crbug/1256481): Only accept URL's from the Ash supplied allow list.
  if (url.DeprecatedGetOriginAsURL() ==
      GURL(chrome::kChromeUIVersionURL).DeprecatedGetOriginAsURL()) {
    ShowOsAppForProfile(ProfileManager::GetPrimaryUserProfile(), url,
                        web_app::SystemAppType::OS_URL_HANDLER);
    return;
  }
}

}  // namespace crosapi
