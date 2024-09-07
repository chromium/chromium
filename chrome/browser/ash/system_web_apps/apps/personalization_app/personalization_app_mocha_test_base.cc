// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_mocha_test_base.h"

#include <memory>

#include "ash/constants/ash_features.h"
#include "ash/constants/ash_switches.h"
#include "ash/shell.h"
#include "ash/wallpaper/test_wallpaper_image_downloader.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/webui/personalization_app/personalization_app_url_constants.h"
#include "base/files/file_util.h"
#include "chrome/browser/ash/system_web_apps/apps/personalization_app/personalization_app_utils.h"
#include "chrome/browser/ash/wallpaper_handlers/test_wallpaper_fetcher_delegate.h"
#include "chrome/browser/profiles/profile.h"
#include "chrome/browser/ui/ash/wallpaper/wallpaper_controller_client_impl.h"
#include "chrome/browser/ui/browser.h"
#include "chrome/test/base/test_chrome_web_ui_controller_factory.h"
#include "chrome/test/base/web_ui_mocha_browser_test.h"
#include "components/manta/features.h"
#include "third_party/skia/include/core/SkColor.h"
#include "ui/gfx/codec/jpeg_codec.h"

namespace ash::personalization_app {

namespace {

// Writes a JPEG image of the specified size and color to |path|. Returns true
// on success.
bool WriteJPEGFile(const base::FilePath& path,
                   int width,
                   int height,
                   SkColor color) {
  base::ScopedAllowBlockingForTesting allow_blocking;
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  std::vector<unsigned char> output;
  gfx::JPEGCodec::Encode(bitmap, /*quality=*/80, &output);

  if (!base::WriteFile(path, output)) {
    LOG(ERROR) << "Writing to " << path.value() << " failed.";
    return false;
  }
  return true;
}

}  // namespace

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

  CreateDefaultWallpapers();
  auto wallpaper_controller_test_api =
      std::make_unique<WallpaperControllerTestApi>(
          ::ash::Shell::Get()->wallpaper_controller());
  wallpaper_controller_test_api->SetDefaultWallpaper(
      GetAccountId(browser()->profile()));
}

// Initializes default wallpaper paths for regular users and writes JPEG
// wallpaper images to them.
void PersonalizationAppMochaTestBase::CreateDefaultWallpapers() {
  base::ScopedAllowBlockingForTesting allow_blocking;
  ASSERT_TRUE(default_wallpaper_dir_.CreateUniqueTempDir());
  const base::FilePath default_wallpaper_path =
      default_wallpaper_dir_.GetPath();

  base::CommandLine* command_line = base::CommandLine::ForCurrentProcess();
  const base::FilePath small_file = default_wallpaper_path.Append("small.jpg");
  command_line->AppendSwitchASCII(switches::kDefaultWallpaperSmall,
                                  small_file.value());
  const base::FilePath large_file = default_wallpaper_path.Append("large.jpg");
  command_line->AppendSwitchASCII(switches::kDefaultWallpaperLarge,
                                  large_file.value());

  const int kWallpaperSize = 2;
  ASSERT_TRUE(WriteJPEGFile(small_file, kWallpaperSize, kWallpaperSize,
                            SK_ColorMAGENTA));
  ASSERT_TRUE(WriteJPEGFile(large_file, kWallpaperSize, kWallpaperSize,
                            SK_ColorMAGENTA));
}

}  // namespace ash::personalization_app
