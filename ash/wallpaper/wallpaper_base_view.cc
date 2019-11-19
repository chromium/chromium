// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/wallpaper_base_view.h"

#include "ash/public/cpp/login_constants.h"
#include "ash/public/cpp/wallpaper_types.h"
#include "ash/shell.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "ash/wm/overview/overview_controller.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/color_analysis.h"
#include "ui/gfx/color_utils.h"
#include "ui/gfx/geometry/safe_integer_conversions.h"

namespace ash {

namespace {

// The value used for alpha to apply a dark filter to the wallpaper in tablet
// mode. A higher number up to 255 results in a darker wallpaper.
constexpr int kTabletModeWallpaperAlpha = 102;

// Returns the color used to dim the wallpaper.
SkColor GetWallpaperDarkenColor() {
  SkColor darken_color =
      Shell::Get()->wallpaper_controller()->GetProminentColor(
          color_utils::ColorProfile(color_utils::LumaRange::DARK,
                                    color_utils::SaturationRange::MUTED));
  if (darken_color == kInvalidWallpaperColor)
    darken_color = login_constants::kDefaultBaseColor;

  darken_color = color_utils::GetResultingPaintColor(
      SkColorSetA(login_constants::kDefaultBaseColor,
                  login_constants::kTranslucentColorDarkenAlpha),
      SkColorSetA(darken_color, 0xFF));

  int alpha = login_constants::kTranslucentAlpha;
  if (Shell::Get()->tablet_mode_controller()->InTabletMode()) {
    alpha = kTabletModeWallpaperAlpha;
  } else if (Shell::Get()->overview_controller()->InOverviewSession()) {
    // Overview mode will apply its own brightness filter on a downscaled image,
    // so color with full opacity here.
    alpha = 255;
  }

  return SkColorSetA(darken_color, alpha);
}

}  // namespace

const char* WallpaperBaseView::GetClassName() const {
  return "WallpaperBaseView";
}

void WallpaperBaseView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as necessary
  // to fill the wallpaper. Ideally the image should be larger than the largest
  // display supported, if not we will scale and center it if the layout is
  // WALLPAPER_LAYOUT_CENTER_CROPPED.
  auto* controller = Shell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout layout = controller->GetWallpaperLayout();

  // Wallpapers with png format could be partially transparent. Fill the canvas
  // with black to make it opaque before painting the wallpaper.
  canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);

  if (wallpaper.isNull())
    return;

  cc::PaintFlags flags;
  if (controller->ShouldApplyDimming()) {
    flags.setColorFilter(
        SkColorFilters::Blend(GetWallpaperDarkenColor(), SkBlendMode::kDarken));
  }

  switch (layout) {
    case WALLPAPER_LAYOUT_CENTER_CROPPED: {
      // The dimension with the smallest ratio must be cropped, the other one
      // is preserved. Both are set in gfx::Size cropped_size.
      double horizontal_ratio =
          static_cast<double>(width()) / static_cast<double>(wallpaper.width());
      double vertical_ratio = static_cast<double>(height()) /
                              static_cast<double>(wallpaper.height());

      gfx::Size cropped_size;
      if (vertical_ratio > horizontal_ratio) {
        cropped_size = gfx::Size(
            gfx::ToFlooredInt(static_cast<double>(width()) / vertical_ratio),
            wallpaper.height());
      } else {
        cropped_size = gfx::Size(
            wallpaper.width(), gfx::ToFlooredInt(static_cast<double>(height()) /
                                                 horizontal_ratio));
      }

      gfx::Rect wallpaper_cropped_rect(wallpaper.size());
      wallpaper_cropped_rect.ClampToCenteredSize(cropped_size);
      DrawWallpaper(wallpaper, wallpaper_cropped_rect, gfx::Rect(size()), flags,
                    canvas);
      break;
    }
    case WALLPAPER_LAYOUT_TILE: {
      canvas->TileImageInt(wallpaper, 0, 0, 0, 0, width(), height(), 1.0f,
                           SkTileMode::kRepeat, SkTileMode::kRepeat, &flags);
      break;
    }
    case WALLPAPER_LAYOUT_STRETCH: {
      // This is generally not recommended as it may show artifacts.
      DrawWallpaper(wallpaper, gfx::Rect(wallpaper.size()), gfx::Rect(size()),
                    flags, canvas);
      break;
    }
    case WALLPAPER_LAYOUT_CENTER: {
      const float image_scale = canvas->image_scale();
      // Simply centered and not scaled (but may be clipped).
      gfx::Rect wallpaper_rect = gfx::ScaleToRoundedRect(
          gfx::Rect(wallpaper.size()),
          centered_layout_image_scale_.x() / image_scale,
          centered_layout_image_scale_.y() / image_scale);
      wallpaper_rect.set_x((width() - wallpaper_rect.width()) / 2);
      wallpaper_rect.set_y((height() - wallpaper_rect.height()) / 2);
      DrawWallpaper(wallpaper, gfx::Rect(wallpaper.size()), wallpaper_rect,
                    flags, canvas);
      break;
    }
    default: {
      NOTREACHED();
      break;
    }
  }
}

void WallpaperBaseView::DrawWallpaper(const gfx::ImageSkia& wallpaper,
                                      const gfx::Rect& src,
                                      const gfx::Rect& dst,
                                      const cc::PaintFlags& flags,
                                      gfx::Canvas* canvas) {
  canvas->DrawImageInt(wallpaper, src.x(), src.y(), src.width(), src.height(),
                       dst.x(), dst.y(), dst.width(), dst.height(),
                       /*filter=*/true, flags);
}

}  // namespace ash
