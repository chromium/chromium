// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/test_personalization_app_webui_provider.h"

#include <memory>

#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_keyboard_backlight_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_user_provider.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_sea_pen_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_theme_provider_impl.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_wallpaper_provider_impl.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "content/public/browser/web_ui.h"
#include "url/gurl.h"

namespace ash::personalization_app {

std::unique_ptr<content::WebUIController>
TestPersonalizationAppWebUIProvider::NewWebUI(content::WebUI* web_ui,
                                              const GURL& url) {
  auto ambient_provider =
      std::make_unique<FakePersonalizationAppAmbientProvider>(web_ui);
  auto keyboard_backlight_provider =
      std::make_unique<FakePersonalizationAppKeyboardBacklightProvider>(web_ui);
  auto sea_pen_provider =
      std::make_unique<PersonalizationAppSeaPenProviderImpl>(
          web_ui,
          std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
  auto theme_provider =
      std::make_unique<PersonalizationAppThemeProviderImpl>(web_ui);
  auto wallpaper_provider =
      std::make_unique<PersonalizationAppWallpaperProviderImpl>(
          web_ui,
          std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
  auto user_provider =
      std::make_unique<FakePersonalizationAppUserProvider>(web_ui);
  return std::make_unique<PersonalizationAppUI>(
      web_ui, std::move(ambient_provider),
      std::move(keyboard_backlight_provider), std::move(sea_pen_provider),
      std::move(theme_provider), std::move(user_provider),
      std::move(wallpaper_provider));
}

}  // namespace ash::personalization_app
