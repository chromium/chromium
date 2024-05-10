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
#include "ui/wm/core/coordinate_conversion.h"

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
  // TODO(http://b/333952534): Remove this check once `wallpaper_underlay_layer`
  // is always created.
  CHECK(wallpaper_underlay_layer);
  wallpaper_underlay_layer->SetVisible(true);

  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  views::AnimationBuilder()
      .SetPreemptionStrategy(
          ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
      .Once()
      .SetDuration(kWallpaperClippingAnimationDuration)
      .SetClipRect(wallpaper_view_layer, GetTargetClipBounds(),
                   gfx::Tween::ACCEL_20_DECEL_100)
      .SetRoundedCorners(wallpaper_view_layer, kWallpaperClipRoundedCornerRadii,
                         gfx::Tween::ACCEL_20_DECEL_100);
}

ScopedOverviewWallpaperClipper::~ScopedOverviewWallpaperClipper() {
  aura::Window* root_window = overview_grid_->root_window();
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  // Switching to tablet mode will force mirroring displays which would destroy
  // non primary root windows. THe wallpaper widget controller would already be
  // destroyed at this point.
  if (!wallpaper_widget_controller) {
    return;
  }
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  // Convert display bounds to the parent's coordinates, as layer bounds are
  // always relative to their parent.
  gfx::Rect target_restore_rect = display::Screen::GetScreen()
                                      ->GetDisplayNearestWindow(root_window)
                                      .bounds();
  wm::ConvertRectFromScreen(root_window, &target_restore_rect);

  views::AnimationBuilder()
      .OnEnded(base::BindOnce(
          [](WallpaperWidgetController* wallpaper_widget_controller) {
            // `WallpaperWidgetController` owns the wallpaper view layer and
            // wallpaper underlay layer, so it's guaranteed to outlive it.
            if (wallpaper_widget_controller->wallpaper_underlay_layer()) {
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
      .SetClipRect(wallpaper_view_layer, target_restore_rect,
                   gfx::Tween::ACCEL_20_DECEL_100)
      .SetRoundedCorners(wallpaper_view_layer, gfx::RoundedCornersF(),
                         gfx::Tween::ACCEL_20_DECEL_100);
}

void ScopedOverviewWallpaperClipper::RefreshWallpaperClipBounds() {
  aura::Window* root_window = overview_grid_->root_window();
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  wallpaper_view_layer->SetClipRect(GetTargetClipBounds());
}

gfx::Rect ScopedOverviewWallpaperClipper::GetTargetClipBounds() const {
  // `GetWallpaperClipBounds()` returns the bounds in screen coordinates.
  // Convert these to the parent's coordinates, as layer bounds are always
  // relative to their parent.
  gfx::Rect target_clip_rect = overview_grid_->GetWallpaperClipBounds();
  wm::ConvertRectFromScreen(overview_grid_->root_window(), &target_clip_rect);
  return target_clip_rect;
}

}  // namespace ash
