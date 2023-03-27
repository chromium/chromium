// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"

#include "base/notreached.h"
#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/wallpaper_handlers/wallpaper_fetcher_delegate.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_wallpaper_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"
#include "components/user_manager/user_type.h"

namespace ash::personalization_app {

PersonalizationAppUI* CreatePersonalizationAppUI(content::WebUI* web_ui) {
  auto ambient_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppAmbientProviderImpl>(web_ui);
  auto keyboard_backlight_provider =
      std::make_unique<ash::personalization_app::
                           PersonalizationAppKeyboardBacklightProviderImpl>(
          web_ui);
  auto theme_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppThemeProviderImpl>(web_ui);
  auto user_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppUserProviderImpl>(web_ui);
  auto wallpaper_provider = std::make_unique<
      ash::personalization_app::PersonalizationAppWallpaperProviderImpl>(
      web_ui,
      std::make_unique<wallpaper_handlers::WallpaperFetcherDelegateImpl>());
  return new ash::personalization_app::PersonalizationAppUI(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(theme_provider),
      std::move(user_provider), std::move(wallpaper_provider));
}

const user_manager::User* GetUser(const Profile* profile) {
  auto* profile_helper = ProfileHelper::Get();
  DCHECK(profile_helper);
  const user_manager::User* user = profile_helper->GetUserByProfile(profile);
  return user;
}

AccountId GetAccountId(const Profile* profile) {
  const auto* user = GetUser(profile);
  if (!user) {
    return EmptyAccountId();
  }
  return user->GetAccountId();
}

bool CanSeeWallpaperOrPersonalizationApp(const Profile* profile) {
  const auto* user = GetUser(profile);
  if (!user) {
    return false;
  }
  switch (user->GetType()) {
    case user_manager::NUM_USER_TYPES:
      NOTREACHED() << "Invalid user type NUM_USER_TYPES";
      return false;
    case user_manager::USER_TYPE_KIOSK_APP:
    case user_manager::USER_TYPE_ARC_KIOSK_APP:
    case user_manager::USER_TYPE_WEB_KIOSK_APP:
      return false;
    case user_manager::USER_TYPE_REGULAR:
    case user_manager::USER_TYPE_CHILD:
    case user_manager::USER_TYPE_GUEST:
    // Public account users must be able to see personalization app since retail
    // demo mode is implemented as a public account.
    case user_manager::USER_TYPE_PUBLIC_ACCOUNT:
    // Active directory users should still be able to change wallpaper and other
    // personalization settings.
    case user_manager::USER_TYPE_ACTIVE_DIRECTORY:
      return true;
  }
}

}  // namespace ash::personalization_app
