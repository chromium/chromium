// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WALLPAPER_WALLPAPER_VIEW_H_
#define ASH_WALLPAPER_WALLPAPER_VIEW_H_

#include "ash/wallpaper/wallpaper_base_view.h"
#include "ash/wallpaper/wallpaper_property.h"
#include "ui/views/context_menu_controller.h"

namespace aura {
class Window;
}

namespace ash {

// The desktop wallpaper view that, in addition to painting the wallpaper, can
// also add blur and dimming effects, as well as handle context menu requests.
class WallpaperView : public WallpaperBaseView,
                      public views::ContextMenuController {
 public:
  explicit WallpaperView(const WallpaperProperty& property);
  ~WallpaperView() override;

  // Clears cached image. Must be called when wallpaper image is changed.
  void ClearCachedImage();

  // Enables/Disables the lock shield layer.
  void SetLockShieldEnabled(bool enabled);

  void set_wallpaper_property(const WallpaperProperty& property) {
    property_ = property;
  }
  const WallpaperProperty& property() const { return property_; }

 private:
  // views::View:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;

  // views::ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // WallpaperBaseView:
  void DrawWallpaper(const gfx::ImageSkia& wallpaper,
                     const gfx::Rect& src,
                     const gfx::Rect& dst,
                     const cc::PaintFlags& flags,
                     gfx::Canvas* canvas) override;

  // Paint parameters (blur sigma and opacity) to draw wallpaper.
  WallpaperProperty property_{wallpaper_constants::kClear};

  // A view to hold solid color layer to hide desktop, in case compositor
  // failed to draw its content due to memory shortage.
  views::View* shield_view_ = nullptr;

  // A cached downsampled image of the wallpaper image. It will help wallpaper
  // blur/brightness animations be more performant.
  base::Optional<gfx::ImageSkia> small_image_;

  DISALLOW_COPY_AND_ASSIGN(WallpaperView);
};

std::unique_ptr<views::Widget> CreateWallpaperWidget(
    aura::Window* root_window,
    const WallpaperProperty& property,
    bool locked,
    WallpaperView** out_wallpaper_view);

}  // namespace ash

#endif  // ASH_WALLPAPER_WALLPAPER_VIEW_H_
