// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/bubble/scrollable_apps_grid_view.h"

#include <limits>
#include <memory>
#include <string>

#include "ash/app_list/app_list_util.h"
#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/model/app_list_model.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/metrics/histogram_macros.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/animation/bounds_animator.h"
#include "ui/views/view_model_utils.h"

namespace ash {
namespace {

// TODO(crbug.com/1211608): Add this to AppListConfig.
const int kVerticalTilePadding = 8;

}  // namespace

ScrollableAppsGridView::ScrollableAppsGridView(
    AppListA11yAnnouncer* a11y_announcer,
    AppListViewDelegate* view_delegate,
    AppsGridViewFolderDelegate* folder_delegate)
    : AppsGridView(/*contents_view=*/nullptr,
                   a11y_announcer,
                   view_delegate,
                   folder_delegate) {
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
      bounds.ClampToCenteredSize(GetTileViewSize());
      view->SetBoundsRect(bounds);
    }
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

gfx::Size ScrollableAppsGridView::GetTileViewSize() const {
  const AppListConfig& config = GetAppListConfig();
  return gfx::Size(config.grid_tile_width(), config.grid_tile_height());
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

int ScrollableAppsGridView::GetPaddingBetweenPages() const {
  // The scrollable apps grid does not use pages.
  return 0;
}

bool ScrollableAppsGridView::IsScrollAxisVertical() const {
  return true;
}

void ScrollableAppsGridView::CalculateIdealBounds() {
  DCHECK(!IsInFolder());

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

void ScrollableAppsGridView::RecordAppMovingTypeMetrics(
    AppListAppMovingType type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListBubbleAppMovingType", type,
                            kMaxAppListAppMovingType);
}

void ScrollableAppsGridView::OnAppListItemViewActivated(
    AppListItemView* pressed_item_view,
    const ui::Event& event) {
  if (IsDragging())
    return;

  if (IsFolderItem(pressed_item_view->item())) {
    // TODO(https://crbug.com/1214064): Implement showing folder contents.
    return;
  }
  // TODO(https://crbug.com/1218435): Implement metrics for app launch.

  // Avoid using |item->id()| as the parameter. In some rare situations,
  // activating the item may destruct it. Using the reference to an object
  // which may be destroyed during the procedure as the function parameter
  // may bring the crash like https://crbug.com/990282.
  const std::string id = pressed_item_view->item()->id();
  app_list_view_delegate()->ActivateItem(
      id, event.flags(), AppListLaunchedFrom::kLaunchedFromGrid);
}

BEGIN_METADATA(ScrollableAppsGridView, AppsGridView)
END_METADATA

}  // namespace ash
