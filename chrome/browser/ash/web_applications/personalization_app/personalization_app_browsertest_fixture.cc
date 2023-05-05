// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_browsertest_fixture.h"

#include <memory>

#include "ash/shell.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/webui/personalization_app/personalization_app_ui.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_ambient_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_keyboard_backlight_provider.h"
#include "ash/webui/personalization_app/test/fake_personalization_app_user_provider.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_theme_provider_impl.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/web_applications/personalization_app/personalization_app_wallpaper_provider_impl.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/mojo_web_ui_browser_test.h"
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
      std::move(keyboard_backlight_provider), std::move(theme_provider),
      std::move(user_provider), std::move(wallpaper_provider));
}

PersonalizationAppBrowserTestFixture::PersonalizationAppBrowserTestFixture() =
    default;

PersonalizationAppBrowserTestFixture::~PersonalizationAppBrowserTestFixture() =
    default;

void PersonalizationAppBrowserTestFixture::SetUpInProcessBrowserTestFixture() {
  auto wallpaper_image_downloader =
      std::make_unique<TestWallpaperImageDownloader>();
  test_wallpaper_image_downloader_ = wallpaper_image_downloader.get();
  WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
      std::move(wallpaper_image_downloader));
}

void PersonalizationAppBrowserTestFixture::SetUpOnMainThread() {
  WallpaperControllerClientImpl::Get()->SetWallpaperFetcherDelegateForTesting(
      std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
  MojoWebUIBrowserTest::SetUpOnMainThread();
  test_factory_.AddFactoryOverride(kChromeUIPersonalizationAppHost,
                                   &test_web_ui_provider_);

  auto wallpaper_controller_test_api =
      std::make_unique<WallpaperControllerTestApi>(
          ::ash::Shell::Get()->wallpaper_controller());
  wallpaper_controller_test_api->SetDefaultWallpaper(
      GetAccountId(browser()->profile()));
}

}  // namespace ash::personalization_app
