// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wallpaper/views/wallpaper_base_view.h"

#include "ash/public/cpp/wallpaper/wallpaper_types.h"
#include "ash/session/session_controller_impl.h"
#include "ash/shell.h"
#include "ash/style/ash_color_id.h"
#include "ash/wallpaper/wallpaper_controller_impl.h"
#include "base/numerics/safe_conversions.h"
#include "cc/paint/paint_flags.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/gfx/canvas.h"

namespace ash {

namespace {

// Gets the shield color based on the state. This is used for the login, lock,
// overview and tablet mode.
SkColor GetWallpaperShieldColor(const views::Widget* widget) {
  ui::ColorId color;

  auto* controller = Shell::Get()->wallpaper_controller();
  if (controller->IsOobeWallpaper()) {
    color = cros_tokens::kCrosSysScrim2;
  } else if (Shell::Get()->session_controller()->IsUserSessionBlocked()) {
    color = kColorAshShieldAndBase80;
  } else {
    color = cros_tokens::kCrosSysScrim2;
  }

  DCHECK(widget);
  return widget->GetColorProvider()->GetColor(color);
}

}  // namespace

void WallpaperBaseView::OnPaint(gfx::Canvas* canvas) {
  // Scale the image while maintaining the aspect ratio, cropping as necessary
  // to fill the wallpaper. Ideally the image should be larger than the largest
  // display supported, if not we will scale and center it if the layout is
  // WALLPAPER_LAYOUT_CENTER_CROPPED.
  auto* controller = Shell::Get()->wallpaper_controller();
  gfx::ImageSkia wallpaper = controller->GetWallpaper();
  WallpaperLayout layout = controller->GetWallpaperLayout();

  if (wallpaper.isNull()) {
    return;
  }

  cc::PaintFlags flags;
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
            base::ClampFloor(static_cast<double>(width()) / vertical_ratio),
            wallpaper.height());
      } else {
        cropped_size = gfx::Size(
            wallpaper.width(),
            base::ClampFloor(static_cast<double>(height()) / horizontal_ratio));
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
      // Fill background with black if wallpaper does not cover the entire view.
      if (!wallpaper_rect.Contains(GetLocalBounds())) {
        canvas->FillRect(GetLocalBounds(), SK_ColorBLACK);
      }
      DrawWallpaper(wallpaper, gfx::Rect(wallpaper.size()), wallpaper_rect,
                    flags, canvas);
      break;
    }
    default: {
      NOTREACHED();
    }
  }

  if (controller->ShouldApplyShield()) {
    canvas->FillRect(GetLocalBounds(), GetWallpaperShieldColor(GetWidget()));
  }
}

void WallpaperBaseView::OnThemeChanged() {
  views::View::OnThemeChanged();
  SchedulePaint();
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

BEGIN_METADATA(WallpaperBaseView)
END_METADATA

}  // namespace ash
