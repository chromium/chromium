// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_test_api.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/bind.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

const WallpaperInfo kTestWallpaperInfo = {"", WALLPAPER_LAYOUT_CENTER, DEFAULT,
                                          base::Time::Now().LocalMidnight()};

gfx::ImageSkia CreateImageWithColor(const SkColor color) {
  gfx::Canvas canvas(gfx::Size(5, 5), 1.0f, true);
  canvas.DrawColor(color);
  return gfx::ImageSkia::CreateFrom1xBitmap(canvas.GetBitmap());
}

}  // namespace

WallpaperControllerTestApi::WallpaperControllerTestApi(
    WallpaperControllerImpl* controller)
    : controller_(controller) {}

WallpaperControllerTestApi::~WallpaperControllerTestApi() = default;

SkColor WallpaperControllerTestApi::ApplyColorProducingWallpaper() {
  // TODO(manucornet): Figure out where all those "magic numbers" come from
  // and document/compute them instead of just hard-coding them.
  controller_->ShowWallpaperImage(
      CreateImageWithColor(SkColorSetRGB(60, 40, 40)), kTestWallpaperInfo,
      /*preview_mode=*/false, /*always_on_top=*/false);
  return SkColorSetRGB(40, 35, 37);
}

void WallpaperControllerTestApi::StartWallpaperPreview() {
  // Preview mode is considered active when the two callbacks have non-empty
  // values. Their specific values don't matter for testing purpose.
  controller_->confirm_preview_wallpaper_callback_ =
      base::BindOnce(&WallpaperControllerImpl::SetWallpaperFromInfo,
                     controller_->weak_factory_.GetWeakPtr(),
                     AccountId::FromUserEmail("user@test.com"),
                     kTestWallpaperInfo, /*show_wallpaper=*/true);
  controller_->reload_preview_wallpaper_callback_ = base::BindRepeating(
      &WallpaperControllerImpl::ShowWallpaperImage,
      controller_->weak_factory_.GetWeakPtr(),
      CreateImageWithColor(SK_ColorBLUE), kTestWallpaperInfo,
      /*preview_mode=*/true, /*always_on_top=*/false);
  // Show the preview wallpaper.
  controller_->reload_preview_wallpaper_callback_.Run();
}

void WallpaperControllerTestApi::EndWallpaperPreview(
    bool confirm_preview_wallpaper) {
  if (confirm_preview_wallpaper)
    controller_->ConfirmPreviewWallpaper();
  else
    controller_->CancelPreviewWallpaper();
}

}  // namespace ash
