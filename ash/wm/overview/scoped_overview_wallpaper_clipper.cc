// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/wm/overview/scoped_overview_wallpaper_clipper.h"
#include "ash/root_window_controller.h"
#include "ash/wallpaper/views/wallpaper_view.h"
#include "ash/wallpaper/views/wallpaper_widget_controller.h"
#include "ash/wm/drag_window_controller.h"
#include "ash/wm/overview/overview_constants.h"
#include "ash/wm/overview/overview_grid.h"
#include "ash/wm/overview/overview_utils.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ui/chromeos/styles/cros_tokens_color_mappings.h"
#include "ui/compositor/layer.h"
#include "ui/display/screen.h"

namespace ash {

ScopedOverviewWallpaperClipper::ScopedOverviewWallpaperClipper(
    OverviewGrid* overview_grid)
    : wallpaper_underlay_layer_(ui::LAYER_SOLID_COLOR),
      overview_grid_(overview_grid) {
  const gfx::Rect current_grid_bounds = overview_grid_->bounds();
  wallpaper_underlay_layer_.SetBounds(current_grid_bounds);

  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(overview_grid_->root_window())
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  auto* wallpaper_view_layer_parent = wallpaper_view_layer->parent();

  // TODO(http://b/327663905): Implement on theme change for the
  // `wallpaper_underlay_layer_`.
  wallpaper_underlay_layer_.SetColor(
      wallpaper_widget_controller->GetWidget()->GetColorProvider()->GetColor(
          cros_tokens::kCrosSysSystemBase));
  wallpaper_view_layer->SetRoundedCornerRadius(
      kWallpaperClipRoundedCornerRadii);
  wallpaper_view_layer->SetClipRect(overview_grid_->GetGridEffectiveBounds());
  wallpaper_view_layer_parent->Add(&wallpaper_underlay_layer_);
  wallpaper_view_layer_parent->StackBelow(&wallpaper_underlay_layer_,
                                          wallpaper_view_layer);
}

ScopedOverviewWallpaperClipper::~ScopedOverviewWallpaperClipper() {
  aura::Window* root_window = overview_grid_->root_window();
  auto* wallpaper_view_layer = RootWindowController::ForWindow(root_window)
                                   ->wallpaper_widget_controller()
                                   ->wallpaper_view()
                                   ->layer();
  auto* wallpaper_view_layer_parent = wallpaper_view_layer->parent();
  CHECK_EQ(wallpaper_underlay_layer_.parent(), wallpaper_view_layer_parent);
  wallpaper_view_layer->SetClipRect(gfx::Rect());
  wallpaper_view_layer->SetBounds(display::Screen::GetScreen()
                                      ->GetDisplayNearestWindow(root_window)
                                      .bounds());
  wallpaper_view_layer->SetRoundedCornerRadius(gfx::RoundedCornersF());
}

void ScopedOverviewWallpaperClipper::RefreshWallpaperClipBounds() {
  wallpaper_underlay_layer_.SetBounds(overview_grid_->bounds());

  WallpaperWidgetController* wallpaper_widget_controller =
      RootWindowController::ForWindow(overview_grid_->root_window())
          ->wallpaper_widget_controller();
  auto* wallpaper_view_layer =
      wallpaper_widget_controller->wallpaper_view()->layer();
  wallpaper_view_layer->SetClipRect(overview_grid_->GetGridEffectiveBounds());
}

}  // namespace ash
