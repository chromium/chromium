// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_wallpaper_clipper.h"

#include <optional>

#include "ash/root_window_controller.h"
#include "ash/shell.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/functional/bind.h"
#include "base/functional/callback_forward.h"
#include "base/functional/callback_helpers.h"
#include "base/notreached.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"
#include "ui/gfx/geometry/rounded_corners_f.h"
#include "ui/views/animation/animation_builder.h"
#include "ui/wm/core/coordinate_conversion.h"

namespace ash {

namespace {

struct AnimationSettings {
  base::TimeDelta duration;
  gfx::Tween::Type tween_type;
};

constexpr AnimationSettings kEnterInformedRestoreAnimationSettings{
    .duration = base::Milliseconds(800),
    .tween_type = gfx::Tween::EASE_IN_OUT_EMPHASIZED};

constexpr AnimationSettings kEnterOverviewAnimationSettings{
    .duration = base::Milliseconds(500),
    .tween_type = gfx::Tween::EASE_IN_OUT_EMPHASIZED};

constexpr AnimationSettings kShowBirchBarInOverviewAnimationSettings{
    .duration = base::Milliseconds(200),
    .tween_type = gfx::Tween::FAST_OUT_LINEAR_IN};

constexpr AnimationSettings kShowBirchBarByUserAnimationSettings{
    .duration = base::Milliseconds(250),
    .tween_type = gfx::Tween::ACCEL_LIN_DECEL_100_3};

constexpr AnimationSettings kHideBirchBarByUserAnimationSettings{
    .duration = base::Milliseconds(100),
    .tween_type = gfx::Tween::LINEAR};

constexpr AnimationSettings kRestoreAnimationSettings{
    .duration = base::Milliseconds(200),
    .tween_type = gfx::Tween::ACCEL_LIN_DECEL_100};

constexpr AnimationSettings kRelayoutBirchBarAnimationSettings{
    .duration = base::Milliseconds(100),
    .tween_type = gfx::Tween::LINEAR};

void RemoveWallpaperClipper(
    WallpaperWidgetController* wallpaper_widget_controller) {
  if (wallpaper_widget_controller->wallpaper_underlay_layer()) {
    wallpaper_widget_controller->wallpaper_underlay_layer()->SetVisible(false);
  }

  if (auto* wallpaper_view_layer =
          wallpaper_widget_controller->wallpaper_view()->layer()) {
    wallpaper_view_layer->SetClipRect(gfx::Rect());
  }
}

}  // namespace

ScopedOverviewWallpaperClipper::ScopedOverviewWallpaperClipper(
    OverviewGrid* overview_grid,
    AnimationType animation_type)
    : overview_grid_(overview_grid) {
  aura::Window* root_window = overview_grid_->root_window();
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();

  // Stop animating the wallpaper view layer. There may be an ongoing clip
  // and/or rounded corner animation from the last overview exit. This ensures
  // the callback completes before we make changes to the wallpaper again.
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  wallpaper_view_layer->GetAnimator()->StopAnimating();

  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  // TODO(http://b/333952534): Remove this check once `wallpaper_underlay_layer`
  // is always created.
  CHECK(wallpaper_underlay_layer);
  wallpaper_underlay_layer->SetVisible(true);

  RefreshWallpaperClipBounds(animation_type, base::DoNothing());
}

ScopedOverviewWallpaperClipper::~ScopedOverviewWallpaperClipper() {
  // Switching to tablet mode will force mirroring displays which would destroy
  // non primary root windows. The wallpaper widget controller would already be
  // destroyed at this point.
  if (auto* wallpaper_widget_controller =
          RootWindowController::ForWindow(overview_grid_->root_window())
              ->wallpaper_widget_controller()) {
    RefreshWallpaperClipBounds(
        AnimationType::kRestore,
        base::BindOnce(&RemoveWallpaperClipper,
                       base::Unretained(wallpaper_widget_controller)));
  }
}

void ScopedOverviewWallpaperClipper::RefreshWallpaperClipBounds(
    AnimationType animation_type,
    base::OnceClosure animation_end_callback) {
  aura::Window* root_window = overview_grid_->root_window();
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();

  gfx::Rect target_clip_rect = animation_type == AnimationType::kRestore
                                   ? display::Screen::GetScreen()
                                         ->GetDisplayNearestWindow(root_window)
                                         .bounds()
                                   : overview_grid_->GetWallpaperClipBounds();
  // Convert the clip bounds to the parent's coordinates, as layer bounds are
  // always relative to their parent.
  wm::ConvertRectFromScreen(root_window, &target_clip_rect);

  // If the animation type is none, directly set the clip rect and rounded
  // corners if the target properties are changed and run the callback.
  if (animation_type == AnimationType::kNone) {
    if (target_clip_rect != wallpaper_view_layer->GetTargetClipRect()) {
      wallpaper_view_layer->SetClipRect(target_clip_rect);
    }

    if (kWallpaperClipRoundedCornerRadii !=
        wallpaper_view_layer->GetTargetRoundedCornerRadius()) {
      wallpaper_view_layer->SetRoundedCornerRadius(
          kWallpaperClipRoundedCornerRadii);
    }

    if (animation_end_callback) {
      std::move(animation_end_callback).Run();
    }
    return;
  }

  // Otherwise, perform animation according to the given type.
  AnimationSettings animation_settings;
  std::optional<gfx::RoundedCornersF> rounded_corners;

  // Set animation settings according to animation type.
  switch (animation_type) {
    case AnimationType::kEnterInformedRestore:
      animation_settings = kEnterInformedRestoreAnimationSettings;
      rounded_corners = kWallpaperClipRoundedCornerRadii;
      break;
    case AnimationType::kEnterOverview:
      animation_settings = kEnterOverviewAnimationSettings;
      rounded_corners = kWallpaperClipRoundedCornerRadii;
      break;
    case AnimationType::kShowBirchBarInOverview:
      animation_settings = kShowBirchBarInOverviewAnimationSettings;
      break;
    case AnimationType::kShowBirchBarByUser:
      animation_settings = kShowBirchBarByUserAnimationSettings;
      break;
    case AnimationType::kHideBirchBarByUser:
      animation_settings = kHideBirchBarByUserAnimationSettings;
      break;
    case AnimationType::kRelayoutBirchBar:
      animation_settings = kRelayoutBirchBarAnimationSettings;
      break;
    case AnimationType::kRestore:
      animation_settings = kRestoreAnimationSettings;
      rounded_corners = gfx::RoundedCornersF();
      break;
    case AnimationType::kNone:
      NOTREACHED();
  }

  views::AnimationBuilder animation_builder;
  if (animation_end_callback) {
    animation_builder.OnEnded(std::move(animation_end_callback));
  }

  views::AnimationSequenceBlock& animation_sequence =
      animation_builder
          .SetPreemptionStrategy(
              ui::LayerAnimator::IMMEDIATELY_ANIMATE_TO_NEW_TARGET)
          .Once()
          .SetDuration(animation_settings.duration)
          .SetClipRect(wallpaper_view_layer, target_clip_rect,
                       animation_settings.tween_type);

  if (rounded_corners) {
    animation_sequence.SetRoundedCorners(wallpaper_view_layer, *rounded_corners,
                                         animation_settings.tween_type);
  }
}

}  // namespace ash
