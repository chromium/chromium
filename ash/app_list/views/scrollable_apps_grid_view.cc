// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "ash/app_list/views/scrollable_apps_grid_view.h"

#include <memory>
#include <string>

#include "ash/app_list/app_list_view_delegate.h"
#include "ash/app_list/model/app_list_item.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/public/cpp/app_list/app_list_config.h"
#include "base/functional/bind.h"
#include "base/metrics/histogram_macros.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_impl_macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/focus/focus_manager.h"
#include "ui/views/view_model_utils.h"
#include "ui/views/widget/widget.h"

namespace ash {
namespace {

// TODO(crbug.com/40182999): Add this to AppListConfig.
const int kVerticalTilePadding = 8;

// Vertical margin in DIPs inside the top and bottom of scroll view where
// auto-scroll will be triggered during drags.
constexpr int kAutoScrollViewMargin = 32;

// Vertical margin in DIPs outside the top and bottom of the widget where
// auto-scroll will trigger. Points outside this margin will not auto-scroll.
constexpr int kAutoScrollWidgetMargin = 8;

// How often to auto-scroll when the mouse is held in the auto-scroll margin.
constexpr base::TimeDelta kAutoScrollInterval = base::Hertz(60.0);

// How much to auto-scroll the view per second. Empirically chosen.
const int kAutoScrollDipsPerSecond = 400;

}  // namespace

ScrollableAppsGridView::ScrollableAppsGridView(
    AppListA11yAnnouncer* a11y_announcer,
    AppListViewDelegate* view_delegate,
    AppsGridViewFolderDelegate* folder_delegate,
    views::ScrollView* parent_scroll_view,
    AppListFolderController* folder_controller,
    AppListKeyboardController* keyboard_controller)
    : AppsGridView(a11y_announcer,
                   view_delegate,
                   folder_delegate,
                   folder_controller,
                   keyboard_controller),
      scroll_view_(parent_scroll_view) {
  DCHECK(scroll_view_);
}

ScrollableAppsGridView::~ScrollableAppsGridView() {
  EndDrag(/*cancel=*/true);
}

void ScrollableAppsGridView::SetMaxColumns(int max_cols) {
  SetMaxColumnsInternal(max_cols);
}

void ScrollableAppsGridView::Layout(PassKey) {
  if (ignore_layout())
    return;

  if (GetContentsBounds().IsEmpty())
    return;

  // TODO(crbug.com/40182999): Use FillLayout on the items container.
  items_container()->SetBoundsRect(GetContentsBounds());

  CalculateIdealBounds();
  for (size_t i = 0; i < view_model()->view_size(); ++i) {
    AppListItemView* view = GetItemViewAt(i);
    view->SetBoundsRect(view_model()->ideal_bounds(i));
  }
  views::ViewModelUtils::SetViewBoundsToIdealBounds(pulsing_blocks_model());
}

gfx::Size ScrollableAppsGridView::GetTileViewSize() const {
  const AppListConfig* config = app_list_config();
  return gfx::Size(config->grid_tile_width(), config->grid_tile_height());
}

gfx::Insets ScrollableAppsGridView::GetTilePadding(int page) const {
  if (has_fixed_tile_padding_)
    return gfx::Insets::VH(-vertical_tile_padding_, -horizontal_tile_padding_);

  int content_width = GetContentsBounds().width();
  int tile_width = app_list_config()->grid_tile_width();
  int width_to_distribute = content_width - cols() * tile_width;

  // While calculating tile padding, assume no padding between a tile and a
  // container bounds.
  DCHECK_GT(cols(), 1);
  const int spaces_between_items = cols() - 1;
  // Each column has padding on left and on right, so a space between two tiles
  // is double the tile padding size.
  const int horizontal_tile_padding =
      width_to_distribute / (spaces_between_items * 2);
  return gfx::Insets::VH(-kVerticalTilePadding, -horizontal_tile_padding);
}

bool ScrollableAppsGridView::ShouldContainerHandleDragEvents() {
  // Apps grid folder view handles its own drag and drop events, otherwise, it
  // should delegate to the apps grid container.
  return !IsInFolder();
}

bool ScrollableAppsGridView::IsAboveTheFold(AppListItemView* item_view) {
  gfx::Rect item_bounds_in_scroll_view = views::View::ConvertRectToTarget(
      item_view, scroll_view_->contents(), item_view->GetLocalBounds());
  return item_bounds_in_scroll_view.bottom() <
         scroll_view_->GetVisibleRect().height();
}

gfx::Size ScrollableAppsGridView::GetTileGridSize() const {
  // AppListItemList may contain page break items, so use the view_model().
  size_t items = view_model()->view_size() + pulsing_blocks_model().view_size();
  // Tests sometimes start with 0 items. Ensure space for at least 1 item.
  if (items == 0) {
    items = 1;
  }

  if (HasExtraSlotForReorderPlaceholder())
    ++items;

  const bool is_last_row_full = (items % cols() == 0);
  const int rows = is_last_row_full ? items / cols() : items / cols() + 1;
  gfx::Size tile_size = GetTotalTileSize(/*page=*/0);
  gfx::Rect grid(tile_size.width() * cols(), tile_size.height() * rows);
  grid.Inset(-GetTilePadding(/*page=*/0));
  return grid.size();
}

int ScrollableAppsGridView::GetTotalPages() const {
  return 1;
}

int ScrollableAppsGridView::GetSelectedPage() const {
  return 0;
}

bool ScrollableAppsGridView::IsPageFull(size_t page_index) const {
  return false;
}

GridIndex ScrollableAppsGridView::GetGridIndexFromIndexInViewModel(
    int index) const {
  return GridIndex(0, index);
}

int ScrollableAppsGridView::GetNumberOfPulsingBlocksToShow(
    int item_count) const {
  const int residue = item_count % cols();
  return cols() + (residue ? cols() - residue : 0);
}

bool ScrollableAppsGridView::MaybeAutoScroll() {
  ScrollDirection direction;
  if (!IsPointInAutoScrollMargin(last_drag_point(), &direction)) {
    // Drag isn't in auto-scroll margin.
    StopAutoScroll();
    return false;
  }

  if (!CanAutoScrollView(direction)) {
    // Scroll view already at top or bottom.
    StopAutoScroll();
    return false;
  }

  if (auto_scroll_timer_.IsRunning()) {
    // The user triggered a drag update while the mouse was in the auto-scroll
    // zone. Don't scroll for this drag update, but keep auto-scroll going.
    return true;
  }

  // Scroll at a constant rate, regardless of when the timer actually fired.
  const base::TimeTicks now = base::TimeTicks::Now();
  const base::TimeDelta time_delta = last_auto_scroll_time_.is_null()
                                         ? kAutoScrollInterval
                                         : now - last_auto_scroll_time_;
  const int y_offset = time_delta.InSecondsF() * kAutoScrollDipsPerSecond;

  // Scroll by `y_offset` in the appropriate direction.
  const int old_scroll_y = scroll_view_->GetVisibleRect().y();
  const int target_scroll_y = direction == ScrollDirection::kUp
                                  ? old_scroll_y - y_offset
                                  : old_scroll_y + y_offset;
  scroll_view_->ScrollToPosition(scroll_view_->vertical_scroll_bar(),
                                 target_scroll_y);

  // The final scroll position may not match the target scroll position because
  // the scroll might have been clamped to the top or bottom.
  int final_scroll_y = scroll_view_->GetVisibleRect().y();

  // Adjust the last drag point because scrolling has changed the position of
  // the apps grid. This ensures that auto-scrolling continues to happen even if
  // the user doesn't move the mouse.
  gfx::Point drag_point = last_drag_point();
  drag_point.Offset(0, final_scroll_y - old_scroll_y);
  set_last_drag_point(drag_point);

  // Auto-scroll again after `kAutoScrollInterval`.
  last_auto_scroll_time_ = now;
  auto_scroll_timer_.Start(
      FROM_HERE, kAutoScrollInterval,
      base::BindOnce(
          base::IgnoreResult(&ScrollableAppsGridView::MaybeAutoScroll),
          base::Unretained(this)));
  return true;
}

void ScrollableAppsGridView::StopAutoScroll() {
  auto_scroll_timer_.Stop();
  last_auto_scroll_time_ = {};
}

bool ScrollableAppsGridView::IsPointInAutoScrollMargin(
    const gfx::Point& point_in_grid_view,
    ScrollDirection* direction) const {
  gfx::Point point_in_scroll_view = point_in_grid_view;
  ConvertPointToTarget(this, scroll_view_, &point_in_scroll_view);

  // Points to the left or right of the scroll view do not autoscroll.
  if (point_in_scroll_view.x() < 0 ||
      point_in_scroll_view.x() > scroll_view_->width()) {
    return false;
  }

  // Points too far above or below the widget do not autoscroll. This helps
  // prevent scrolling when the user is dragging into the shelf.
  gfx::Point point_in_screen = point_in_grid_view;
  ConvertPointToScreen(this, &point_in_screen);
  gfx::Rect widget_bounds = GetWidget()->GetWindowBoundsInScreen();
  if (point_in_screen.y() < widget_bounds.y() - kAutoScrollWidgetMargin ||
      point_in_screen.y() > widget_bounds.bottom() + kAutoScrollWidgetMargin) {
    return false;
  }

  if (point_in_scroll_view.y() < kAutoScrollViewMargin) {
    *direction = ScrollDirection::kUp;
    return true;
  }
  const int view_bottom = scroll_view_->height();
  if (point_in_scroll_view.y() > view_bottom - kAutoScrollViewMargin) {
    *direction = ScrollDirection::kDown;
    return true;
  }
  return false;
}

bool ScrollableAppsGridView::CanAutoScrollView(
    ScrollDirection direction) const {
  const gfx::Rect visible_rect = scroll_view_->GetVisibleRect();
  if (direction == ScrollDirection::kUp) {
    // Can scroll up if the visible rect is not at the top of the contents.
    return visible_rect.y() > 0;
  }
  // Can scroll down if the visible rect is not at the bottom of the contents.
  return visible_rect.bottom() < scroll_view_->contents()->height();
}

void ScrollableAppsGridView::HandleScrollFromParentView(
    const gfx::Vector2d& offset,
    ui::EventType type) {
  // AppListView uses a paged apps grid view, so this must be a folder opened
  // in the fullscreen launcher.
  DCHECK(IsInFolder());

  // Scroll events in the folder view title area should scroll the view.
  scroll_view_->vertical_scroll_bar()->OnScroll(/*dx=*/0, offset.y());
}

void ScrollableAppsGridView::SetFocusAfterEndDrag(AppListItem* drag_item) {
  auto* focus_manager = GetFocusManager();
  if (!focus_manager)  // Does not exist during widget close.
    return;

  // Release focus from the dragged item (so it won't stay selected).
  focus_manager->ClearFocus();

  // When a folder is open, don't move focus to search box, since it may be
  // behind the folder.
  if (IsInFolder())
    return;

  // Focus the first focusable view in the widget (the search box).
  focus_manager->AdvanceFocus(/*reverse=*/false);
}

void ScrollableAppsGridView::RecordAppMovingTypeMetrics(
    AppListAppMovingType type) {
  UMA_HISTOGRAM_ENUMERATION("Apps.AppListBubbleAppMovingType", type,
                            kMaxAppListAppMovingType);
}

std::optional<int> ScrollableAppsGridView::GetMaxRowsInPage(int page) const {
  return std::nullopt;
}

gfx::Vector2d ScrollableAppsGridView::GetGridCenteringOffset(int page) const {
  return gfx::Vector2d();
}

void ScrollableAppsGridView::EnsureViewVisible(const GridIndex& index) {
  // If called after user action that changes the grid size, make sure grid
  // view ancestor layout is up to date before attempting scroll.
  GetWidget()->LayoutRootViewIfNecessary();

  AppListItemView* view = GetViewAtIndex(index);
  if (view)
    view->ScrollViewToVisible();
}

std::optional<ScrollableAppsGridView::VisibleItemIndexRange>
ScrollableAppsGridView::GetVisibleItemIndexRange() const {
  // Indicate the first row on which item views are visible.
  std::optional<int> first_visible_row;

  // Indicate the first invisible row that is right after the last visible row.
  std::optional<int> first_invisible_row;

  const gfx::Rect scroll_view_visible_rect = scroll_view_->GetVisibleRect();
  for (size_t view_index = 0; view_index < view_model()->view_size();
       view_index += cols()) {
    // Calculate an item view's bounds in the scroll content's coordinates.
    gfx::Point item_view_local_origin;
    views::View* item_view = view_model()->view_at(view_index);
    views::View::ConvertPointToTarget(item_view, scroll_view_->contents(),
                                      &item_view_local_origin);
    gfx::Rect item_view_bounds_in_scroll_view =
        gfx::Rect(item_view_local_origin, item_view->size());

    // Calculate the overlapped area between the item view's bounds and the
    // visible area.
    item_view_bounds_in_scroll_view.InclusiveIntersect(
        scroll_view_visible_rect);

    // An item is deemed to visible if the overlapped area is not empty.
    const bool is_current_row_visible =
        !item_view_bounds_in_scroll_view.IsEmpty();

    const int current_row = view_index / cols();
    if (is_current_row_visible) {
      // Already find the first visible row so continue.
      if (first_visible_row)
        continue;

      first_visible_row = current_row;
    } else if (first_visible_row) {
      DCHECK(!first_invisible_row);
      first_invisible_row = current_row;
      break;
    }
  }

  if (!first_visible_row)
    return std::nullopt;

  VisibleItemIndexRange result;
  result.first_index = *first_visible_row * cols();

  // If `first_invisible_row` is not found, it means that the last item view
  // in the view model is visible.
  result.last_index = first_invisible_row ? *first_invisible_row * cols() - 1
                                          : view_model()->view_size() - 1;

  return result;
}

const gfx::Vector2d ScrollableAppsGridView::CalculateTransitionOffset(
    int page_of_view) const {
  // The ScrollableAppsGridView has no page transitions.
  return gfx::Vector2d();
}

BEGIN_METADATA(ScrollableAppsGridView)
END_METADATA

}  // namespace ash
