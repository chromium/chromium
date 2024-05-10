// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_SCROLLABLE_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_SCROLLABLE_APPS_GRID_VIEW_H_

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/app_list_item_view.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "base/timer/timer.h"
#include "ui/base/metadata/metadata_header_macros.h"

namespace views {
class ScrollView;
}

namespace ash {

class AppListKeyboardController;
class AppListViewDelegate;

// An apps grid that shows all the apps in a long scrolling list. Used for
// the clamshell mode bubble launcher. Implemented as a single "page" of apps.
// GridIndex in this class always has a page of 0. Supports "auto-scroll", a
// feature where the user can drag an app icon to the top or bottom of the
// containing ScrollView and the view will be scrolled automatically.
class ASH_EXPORT ScrollableAppsGridView : public AppsGridView {
  METADATA_HEADER(ScrollableAppsGridView, AppsGridView)

 public:
  ScrollableAppsGridView(AppListA11yAnnouncer* a11y_announcer,
                         AppListViewDelegate* view_delegate,
                         AppsGridViewFolderDelegate* folder_delegate,
                         views::ScrollView* scroll_view,
                         AppListFolderController* folder_controller,
                         AppListKeyboardController* keyboard_controller);
  ScrollableAppsGridView(const ScrollableAppsGridView&) = delete;
  ScrollableAppsGridView& operator=(const ScrollableAppsGridView&) = delete;
  ~ScrollableAppsGridView() override;

  // Sets the max number of columns the grid can have.
  // See `AppsGridView::SetMaxColumnsInternal()` for details.
  void SetMaxColumns(int max_cols);

  // views::View:
  void Layout(PassKey) override;

  // AppsGridView:
  gfx::Size GetTileViewSize() const override;
  gfx::Insets GetTilePadding(int page) const override;
  gfx::Size GetTileGridSize() const override;
  int GetTotalPages() const override;
  int GetSelectedPage() const override;
  bool IsPageFull(size_t page_index) const override;
  GridIndex GetGridIndexFromIndexInViewModel(int index) const override;
  int GetNumberOfPulsingBlocksToShow(int item_count) const override;
  bool MaybeAutoScroll() override;
  void StopAutoScroll() override;
  void HandleScrollFromParentView(const gfx::Vector2d& offset,
                                  ui::EventType type) override;
  void SetFocusAfterEndDrag(AppListItem* drag_item) override;
  void RecordAppMovingTypeMetrics(AppListAppMovingType type) override;
  std::optional<int> GetMaxRowsInPage(int page) const override;
  gfx::Vector2d GetGridCenteringOffset(int page) const override;
  const gfx::Vector2d CalculateTransitionOffset(
      int page_of_view) const override;
  void EnsureViewVisible(const GridIndex& index) override;
  std::optional<VisibleItemIndexRange> GetVisibleItemIndexRange()
      const override;
  bool ShouldContainerHandleDragEvents() override;
  bool IsAboveTheFold(AppListItemView* item_view) override;

  views::ScrollView* scroll_view_for_test() { return scroll_view_; }
  base::OneShotTimer* auto_scroll_timer_for_test() {
    return &auto_scroll_timer_;
  }

 private:
  enum class ScrollDirection { kUp, kDown };

  // Returns true if a drag to `point_in_grid_view` should trigger auto-scroll.
  // If so, sets `scroll_direction`.
  bool IsPointInAutoScrollMargin(const gfx::Point& point_in_grid_view,
                                 ScrollDirection* direction) const;

  // Returns true if `scroll_view_` can be scrolled (i.e. it is not already at
  // the top or the bottom).
  bool CanAutoScrollView(ScrollDirection direction) const;

  // Returns the number of DIPs to auto-scroll.
  int GetAutoScrollOffset() const;

  // The scroll view that contains this view (and other views).
  const raw_ptr<views::ScrollView> scroll_view_;

  // Timer to scroll the `scroll_view_`.
  base::OneShotTimer auto_scroll_timer_;

  // When the last auto-scroll happened.
  base::TimeTicks last_auto_scroll_time_;
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_SCROLLABLE_APPS_GRID_VIEW_H_
