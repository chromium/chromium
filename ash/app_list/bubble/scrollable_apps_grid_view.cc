// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"

#include <limits>
#include <memory>

#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "ui/views/view_model_utils.h"

namespace ash {
namespace {

// TODO(crbug.com/1211608): Add this to AppListConfig.
const int kVerticalTilePadding = 8;

gfx::Size GetTileViewSize(const AppListConfig& config) {
  return gfx::Size(config.grid_tile_width(), config.grid_tile_height());
}

}  // namespace

ScrollableAppsGridView::ScrollableAppsGridView(
    AppListViewDelegate* view_delegate,
    AppsGridViewFolderDelegate* folder_delegate)
    : AppsGridView(/*contents_view=*/nullptr, view_delegate, folder_delegate) {
  // TODO(crbug.com/1211608): Get rid of rows_per_page in this class.
  SetLayout(/*cols=*/5, /*rows_per_page=*/std::numeric_limits<int>::max());
}

ScrollableAppsGridView::~ScrollableAppsGridView() {
  EndDrag(/*cancel=*/true);
}

void ScrollableAppsGridView::Layout() {
  if (ignore_layout())
    return;

  if (bounds_animator()->IsAnimating())
    bounds_animator()->Cancel();

  if (GetContentsBounds().IsEmpty())
    return;

  // TODO(crbug.com/1211608): Use FillLayout on the items container.
  items_container()->SetBoundsRect(GetContentsBounds());

  CalculateIdealBoundsForFolder();
  for (int i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    if (view != drag_view()) {
      view->SetBoundsRect(view_model()->ideal_bounds(i));
    } else {
      // If the drag view size changes, make sure it has the same center.
      gfx::Rect bounds = view->bounds();
      bounds.ClampToCenteredSize(GetTileViewSize(GetAppListConfig()));
      view->SetBoundsRect(bounds);
    }
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

gfx::Insets ScrollableAppsGridView::GetTilePadding() const {
  int content_width = GetContentsBounds().width();
  int tile_width = GetAppListConfig().grid_tile_width();
  int width_to_distribute = content_width - cols() * tile_width;
  // Each column has padding on left and on right.
  int horizontal_tile_padding = width_to_distribute / (cols() * 2);
  return gfx::Insets(-kVerticalTilePadding, -horizontal_tile_padding);
}

gfx::Size ScrollableAppsGridView::GetTileGridSize() const {
  const int items = model()->top_level_item_list()->item_count();
  const bool is_last_row_full = (items % cols() == 0);
  const int rows = is_last_row_full ? items / cols() : items / cols() + 1;
  gfx::Size tile_size = GetTotalTileSize();
  gfx::Size grid_size(tile_size.width() * cols(), tile_size.height() * rows);
  return grid_size;
}

void ScrollableAppsGridView::CalculateIdealBounds() {
  DCHECK(!is_in_folder());

  int grid_index = 0;
  int model_index = 0;
  for (const auto& entry : view_model()->entries()) {
    views::View* view = entry.view;
    if (grid_index == reorder_placeholder_slot()) {
      // Create space by incrementing the grid index.
      ++grid_index;
    }
    if (view == drag_view()) {
      // Skip the drag view. The dragging code will set the bounds. Collapse
      // space in the grid by not incrementing grid_index.
      ++model_index;
      continue;
    }
    gfx::Rect tile_slot = GetExpectedTileBounds(GridIndex(0, grid_index));
    view_model()->set_ideal_bounds(model_index, tile_slot);
    ++model_index;
    ++grid_index;
  }
}

}  // namespace ash
