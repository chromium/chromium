// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/test/ash_pixel_diff_test_base.h"

#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/command_line.h"
#include "ui/compositor/compositor_switches.h"
#include "ui/display/display.h"

namespace ash {

namespace {

// The fake user account.
constexpr char kUser[] = "user1@test.com";

// The fake file ids for wallpaper setting.
constexpr char kFakeFileId[] = "file-hash";
constexpr char kWallpaperFileName[] = "test-file";

constexpr SkColor kWallPaperColor = SK_ColorMAGENTA;

// Creates a pure color image of the specified size.
gfx::ImageSkia CreateImage(int width, int height, SkColor color) {
  SkBitmap bitmap;
  bitmap.allocN32Pixels(width, height);
  bitmap.eraseColor(color);
  gfx::ImageSkia image = gfx::ImageSkia::CreateFrom1xBitmap(bitmap);
  return image;
}

}  // namespace

AshPixelDiffTestBase::AshPixelDiffTestBase(
    std::unique_ptr<base::test::TaskEnvironment> task_environment)
    : AshTestBase(std::move(task_environment)),
      kAccountId_(AccountId::FromUserEmailGaiaId(kUser, "test-hash")) {}

AshPixelDiffTestBase::~AshPixelDiffTestBase() = default;

void AshPixelDiffTestBase::SetUp() {
  // In ash_pixeltests, we want to take screenshots then compare them with the
  // benchmark images. Therefore, enable pixel output in tests.
  base::CommandLine::ForCurrentProcess()->AppendSwitch(
      switches::kEnablePixelOutputInTests);

  // Do not start the user session in `AshTestBase::SetUp()`. Instead, perform
  // user login with `kAccountId_`.
  set_start_session(false);

  AshTestBase::SetUp();

  SimulateUserLogin(kAccountId_);

  // Set variable UI components in explicit ways to stabilize screenshots.
  SetWallPaper();
}

void AshPixelDiffTestBase::SetWallPaper() {
  ASSERT_TRUE(user_data_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(online_wallpaper_dir_.CreateUniqueTempDir());
  ASSERT_TRUE(custom_wallpaper_dir_.CreateUniqueTempDir());

  auto* controller = Shell::Get()->wallpaper_controller();
  controller->Init(user_data_dir_.GetPath(), online_wallpaper_dir_.GetPath(),
                   custom_wallpaper_dir_.GetPath(),
                   /*policy_wallpaper=*/base::FilePath());
  controller->set_wallpaper_reload_no_delay_for_test();
  controller->SetClient(&client_);
  client_.set_fake_files_id_for_account_id(kAccountId_, kFakeFileId);

  const gfx::Size display_size = GetPrimaryDisplay().size();
  gfx::ImageSkia wallpaper_image =
      CreateImage(display_size.width(), display_size.height(), kWallPaperColor);
  controller->SetCustomWallpaper(kAccountId_, kWallpaperFileName,
                                 WALLPAPER_LAYOUT_STRETCH, wallpaper_image,
                                 /*preview_mode=*/false);
}

}  // namespace ash
