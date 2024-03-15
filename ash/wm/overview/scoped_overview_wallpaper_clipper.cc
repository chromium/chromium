// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_wallpaper_clipper.h"

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/functional/bind.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/views/animation/animation_builder.h"

namespace ash {

namespace {

// Duration of wallpaper clipping animation when entering overview.
constexpr base::TimeDelta kWallpaperClippingAnimationDuration =
    base::Milliseconds(350);

// Duration of wallpaper restoration animation when exiting overview.
constexpr base::TimeDelta kWallpaperRestoreAnimationDuration =
    base::Milliseconds(200);

}  // namespace

ScopedOverviewWallpaperClipper::ScopedOverviewWallpaperClipper(
    OverviewGrid* overview_grid)
    : overview_grid_(overview_grid) {
  aura::Window* root_window = overview_grid_->root_window();
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  wallpaper_underlay_layer->SetVisible(true);

  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kWallpaperClippingAnimationDuration)
      .SetClipRect(wallpaper_view_layer,
                   overview_grid_->GetGridEffectiveBounds(),
                   gfx::Tween::ACCEL_20_DECEL_100)
      .SetRoundedCorners(wallpaper_view_layer, kWallpaperClipRoundedCornerRadii,
                         gfx::Tween::ACCEL_20_DECEL_100);
}

ScopedOverviewWallpaperClipper::~ScopedOverviewWallpaperClipper() {
  aura::Window* root_window = overview_grid_->root_window();
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](WallpaperWidgetController* wallpaper_widget_controller) {
            // `WallpaperWidgetController` owns the wallpaper view layer and
            // wallpaper underlay layer, so it's guaranteed to outlive it.
            if (auto* wallpaper_underlay_layer =
                    wallpaper_widget_controller->wallpaper_underlay_layer()) {
              wallpaper_widget_controller->wallpaper_underlay_layer()
                  ->SetVisible(false);
            }
            if (auto* wallpaper_view_layer =
                    wallpaper_widget_controller->wallpaper_view()->layer()) {
              wallpaper_view_layer->SetClipRect(gfx::Rect());
            }
          },
          base::Unretained(wallpaper_widget_controller)))
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kWallpaperRestoreAnimationDuration)
      .SetClipRect(wallpaper_view_layer,
                   display::Screen::GetScreen()
                       ->GetDisplayNearestWindow(root_window)
                       .bounds(),
                   gfx::Tween::ACCEL_20_DECEL_100)
      .SetRoundedCorners(wallpaper_view_layer, gfx::RoundedCornersF(),
                         gfx::Tween::ACCEL_20_DECEL_100);
}

void ScopedOverviewWallpaperClipper::RefreshWallpaperClipBounds() {
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(overview_grid_->root_window())
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  wallpaper_view_layer->SetClipRect(overview_grid_->GetGridEffectiveBounds());
}

}  // namespace ash
