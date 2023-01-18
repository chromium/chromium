// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_controller_test_api.h"

#include <memory>

#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_calculated_colors.h"
#include "ash/wallpaper/wallpaper_utils/wallpaper_color_calculator.h"
#include "base/functional/bind.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/size.h"
#include "ui/gfx/image/image_skia.h"

namespace ash {

namespace {

const WallpaperInfo kTestWallpaperInfo = {"", WALLPAPER_LAYOUT_CENTER,
                                          WallpaperType::kDefault,
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

void WallpaperControllerTestApi::SetCalculatedColors(
    const WallpaperCalculatedColors& calculated_colors) {
  if (controller_->color_calculator_) {
    controller_->color_calculator_.reset();
  }
  controller_->SetCalculatedColors(calculated_colors);
}

}  // namespace ash
