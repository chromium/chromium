// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_mocha_test_base.h"

#include <memory>

#include "ash/shell.h"
#include "ash/constants/ash_features.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"

namespace ash::personalization_app {

PersonalizationAppMochaTestBase::PersonalizationAppMochaTestBase() {
scoped_feature_list_.InitWithFeatures(
      {
          ::ash::features::kFeatureManagementTimeOfDayScreenSaver,
          ::ash::features::kFeatureManagementTimeOfDayWallpaper,
      },
      {});
  set_test_loader_host(
      ::ash::personalization_app::kChromeUIPersonalizationAppHost);
}

PersonalizationAppMochaTestBase::~PersonalizationAppMochaTestBase() =
    default;

void PersonalizationAppMochaTestBase::SetUpInProcessBrowserTestFixture() {
  WallpaperControllerImpl::SetWallpaperImageDownloaderForTesting(
      std::make_unique<TestWallpaperImageDownloader>());
}

void PersonalizationAppMochaTestBase::SetUpOnMainThread() {
  WallpaperControllerClientImpl::Get()->SetWallpaperFetcherDelegateForTesting(
      std::make_unique<wallpaper_handlers::TestWallpaperFetcherDelegate>());
  WebUIMochaBrowserTest::SetUpOnMainThread();
  test_factory_.AddFactoryOverride(kChromeUIPersonalizationAppHost,
                                   &test_webui_provider_);

  auto wallpaper_controller_test_api =
      std::make_unique<WallpaperControllerTestApi>(
          ::ash::Shell::Get()->wallpaper_controller());
  wallpaper_controller_test_api->SetDefaultWallpaper(
      GetAccountId(browser()->profile()));
}

}  // namespace ash::personalization_app
