// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_wallpaper_clipper.h"

#include "ash/root_window_controller.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ui/compositor/layer.h"

namespace ash {

ScopedOverviewWallpaperClipper::ScopedOverviewWallpaperClipper(
    OverviewGrid* overview_grid)
    : overview_grid_(overview_grid) {
  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(overview_grid_->root_window())
          ->wallpaper_widget_controller();
  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  wallpaper_underlay_layer->SetVisible(true);

  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  wallpaper_view_layer->SetRoundedCornerRadius(
      kWallpaperClipRoundedCornerRadii);
  wallpaper_view_layer->SetClipRect(overview_grid_->GetGridEffectiveBounds());
}

ScopedOverviewWallpaperClipper::~ScopedOverviewWallpaperClipper() {
  aura::Window* root_window = overview_grid_->root_window();
  auto* wallpaper_widget_controller =
      RootWindowController::ForWindow(root_window)
          ->wallpaper_widget_controller();

  auto* wallpaper_underlay_layer =
      wallpaper_widget_controller->wallpaper_underlay_layer();
  wallpaper_underlay_layer->SetVisible(false);

  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  wallpaper_view_layer->SetClipRect(gfx::Rect());
  wallpaper_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF());
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
