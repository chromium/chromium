// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"

#include "chrome/browser/ash/profiles/profile_helper.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_ambient_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_keyboard_backlight_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_user_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_wallpaper_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "components/account_id/account_id.h"
#include "components/user_manager/user.h"

namespace ash {
namespace personalization_app {

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
      web_ui);
  return new ash::personalization_app::PersonalizationAppUI(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(theme_provider),
      std::move(user_provider), std::move(wallpaper_provider));
}

const user_manager::User* GetUser(const Profile* profile) {
  auto* profile_helper = chromeos::ProfileHelper::Get();
  DCHECK(profile_helper);
  const user_manager::User* user = profile_helper->GetUserByProfile(profile);
  DCHECK(user);
  return user;
}

AccountId GetAccountId(const Profile* profile) {
  return GetUser(profile)->GetAccountId();
}

}  // namespace personalization_app
}  // namespace ash
