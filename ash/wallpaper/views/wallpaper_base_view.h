// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_VIEWS_WALLPAPER_BASE_VIEW_H_
#define ASH_WALLPAPER_VIEWS_WALLPAPER_BASE_VIEW_H_

#include "ui/gfx/geometry/vector2d_f.h"
#include "ui/views/view.h"

namespace cc {
class PaintFlags;
}

namespace ash {

// A view that paints the wallpaper according to its layout inside its bounds.
// This view doesn't add any dimming or blur to the painted wallpaper. Sub
// classes may override DrawWallpaper() to achieve these effects.
// This can be used directly (e.g. by DeskPreviewView) to paint just the
// wallpaper without any extra effects.
class WallpaperBaseView : public views::View {
  METADATA_HEADER(WallpaperBaseView, views::View)

 public:
  WallpaperBaseView() = default;
  WallpaperBaseView(const WallpaperBaseView&) = delete;
  WallpaperBaseView& operator=(const WallpaperBaseView&) = delete;
  ~WallpaperBaseView() override = default;

  void set_centered_layout_image_scale(const gfx::Vector2dF& value) {
    centered_layout_image_scale_ = value;
  }

  // views::View:
  void OnPaint(gfx::Canvas* canvas) override;
  void OnThemeChanged() override;

 protected:
  virtual void DrawWallpaper(const gfx::ImageSkia& wallpaper,
                             const gfx::Rect& src,
                             const gfx::Rect& dst,
                             const cc::PaintFlags& flags,
                             gfx::Canvas* canvas);

 private:
  // Factor by which we scale the size of the wallpaper image when the wallpaper
  // layout is WALLPAPER_LAYOUT_CENTER. This is used by DeskPreviewView, which
  // acts as a minified view of the desk, in which case a centered image in the
  // big bounds of the display, will not look the same way if we don't scale it
  // down by the same factor by which we scale down the desk in the its
  // mini_view.
  gfx::Vector2dF centered_layout_image_scale_ = gfx::Vector2dF(1.0f, 1.0f);
};

}  // namespace ash

#endif  // ASH_WALLPAPER_VIEWS_WALLPAPER_BASE_VIEW_H_
