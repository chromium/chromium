// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
#define ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_

#include <memory>
#include <optional>
#include <vector>

#include "ash/app_list/app_list_metrics.h"
#include "ash/app_list/views/apps_grid_view.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/pagination/pagination_model.h"
#include "ash/public/cpp/pagination/pagination_model_observer.h"
#include "base/memory/raw_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/presentation_time_recorder.h"
#include "ui/compositor/throughput_tracker.h"
#include "ui/events/types/event_type.h"
#include "ui/gfx/geometry/point_f.h"
#include "ui/views/animation/animation_abort_handle.h"
#include "ui/views/view_targeter_delegate.h"

namespace gfx {
class Vector2d;
}  // namespace gfx

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class AppListKeyboardController;
class ContentsView;
class PaginationController;

// An apps grid that shows the apps on a series of fixed-size pages.
// Used for the peeking/fullscreen launcher, home launcher and folders.
// Created by and is a child of AppsContainerView. Observes layer animations
// for the transition into and out of the "cardified" state.
class ASH_EXPORT PagedAppsGridView : public AppsGridView,
                                     public PaginationModelObserver,
                                     public views::ViewTargeterDelegate {
  METADATA_HEADER(PagedAppsGridView, AppsGridView)

 public:
  class ContainerDelegate {
   public:
    virtual ~ContainerDelegate() = default;

    // Returns true if |point| lies within the bounds of this grid view plus a
    // buffer area surrounding it that can trigger page flip.
    virtual bool IsPointWithinPageFlipBuffer(const gfx::Point& point) const = 0;

    // Returns whether |point| is in the bottom drag buffer, and not over the
    // shelf.
    virtual bool IsPointWithinBottomDragBuffer(
        const gfx::Point& point,
        int page_flip_zone_size) const = 0;

    // Triggered when cardified state begins before animations start.
    virtual void OnCardifiedStateStarted() {}

    // Triggered when cardified state ends and the bounds animations for leaving
    // cardified state have completed.
    virtual void OnCardifiedStateEnded() {}
  };

  PagedAppsGridView(ContentsView* contents_view,
                    AppListA11yAnnouncer* a11y_announcer,
                    AppListFolderController* folder_controller,
                    ContainerDelegate* container_delegate,
                    AppListKeyboardController* keyboard_controller);
  PagedAppsGridView(const PagedAppsGridView&) = delete;
  PagedAppsGridView& operator=(const PagedAppsGridView&) = delete;
  ~PagedAppsGridView() override;

  // Sets the number of max rows and columns in grid pages. Special-cases the
  // first page, which may allow smaller number of rows in certain cases (to
  // make room for other UI elements like continue section).
  // For non-folder item grid, this generally describes the number of slots
  // shown in the page. For folders, the number of displayed slots will also
  // depend on number of items in the grid (e.g. folder with 4 items will have
  // 2x2 grid).
  void SetMaxColumnsAndRows(int max_columns,
                            int max_rows_on_first_page,
                            int max_rows);

  // ui::EventHandler:
  void OnGestureEvent(ui::GestureEvent* event) override;

  // views::View:
  void Layout(PassKey) override;
  void OnThemeChanged() override;

  // AppsGridView:
  gfx::Size GetTileViewSize() const override;
  gfx::Insets GetTilePadding(int page) const override;
  gfx::Size GetTileGridSize() const override;
  int GetTotalPages() const override;
  int GetSelectedPage() const override;
  bool IsPageFull(size_t page_index) const override;
  GridIndex GetGridIndexFromIndexInViewModel(int index) const override;
  int GetNumberOfPulsingBlocksToShow(int item_count) const override;
  void MaybeStartCardifiedView() override;
  void MaybeEndCardifiedView() override;
  bool IsAnimatingCardifiedState() const override;
  bool MaybeStartPageFlip() override;
  void MaybeStopPageFlip() override;
  bool MaybeAutoScroll() override;
  void StopAutoScroll() override {}
  void HandleScrollFromParentView(const gfx::Vector2d& offset,
                                  ui::EventType type) override;
  void SetFocusAfterEndDrag(AppListItem* drag_item) override;
  void RecordAppMovingTypeMetrics(AppListAppMovingType type) override;
  std::optional<int> GetMaxRowsInPage(int page) const override;
  gfx::Vector2d GetGridCenteringOffset(int page) const override;
  void UpdatePaging() override;
  void RecordPageMetrics() override;
  const gfx::Vector2d CalculateTransitionOffset(
      int page_of_view) const override;
  void EnsureViewVisible(const GridIndex& index) override;
  std::optional<VisibleItemIndexRange> GetVisibleItemIndexRange()
      const override;
  bool ShouldContainerHandleDragEvents() override;
  bool IsAboveTheFold(AppListItemView* item_view) override;

  // PaginationModelObserver:
  void SelectedPageChanged(int old_selected, int new_selected) override;
  void TransitionStarting() override;
  void TransitionStarted() override;
  void TransitionChanged() override;
  void TransitionEnded() override;
  void ScrollStarted() override;
  void ScrollEnded() override;

  // views::ViewTargeterDelegate:
  bool DoesIntersectRect(const views::View* target,
                         const gfx::Rect& rect) const override;

  bool FirePageFlipTimerForTest();
  bool cardified_state_for_testing() const { return cardified_state_; }
  int BackgroundCardCountForTesting() const { return background_cards_.size(); }
  // Returns bounds within the apps grid view for the background card layer
  // with provided card index.
  gfx::Rect GetBackgroundCardBoundsForTesting(size_t card_index);
  ui::Layer* GetBackgroundCardLayerForTesting(size_t card_index) const;
  void set_page_flip_delay_for_testing(base::TimeDelta page_flip_delay) {
    page_flip_delay_ = page_flip_delay;
  }

  // Gets the PaginationModel used for the grid view.
  PaginationModel* pagination_model() { return &pagination_model_; }

  // Sets `first_page_offset_` and `shown_under_recent_apps_`, which are used to
  // calculate the first apps grid page layout (number of rows and the padding
  // between them).
  // `offset` is reserved space for continue section in the apps
  // container (which is shown above the grid on the first app list page with
  // productivity launcher).
  // `shown_under_recent_apps` indicates whether the
  // continue section contains list of recent apps. If this is the case, the
  // apps grid will add additional padding above the apps grid (i.e. treat the
  // recent apps row as additional row of apps).
  // Returns whether the first page configuration changed.
  bool ConfigureFirstPagePadding(int offset, bool shown_under_recent_apps);

  // Calculates the maximum number of rows on the first page. Relies on tile
  // size, `first_page_offset_`, `shown_under_recent_apps_` and the bounds of
  // the apps grid.
  int CalculateFirstPageMaxRows(int available_height, int preferred_rows);

  // Calculates the maximum number of rows. Relies on tile size and the bounds
  // of the apps grid.
  int CalculateMaxRows(int available_height, int preferred_rows);

  int GetFirstPageRowsForTesting() const { return max_rows_on_first_page_; }
  int GetRowsForTesting() const { return max_rows_; }

  void set_margin_for_gradient_mask(int margin) {
    margin_for_gradient_mask_ = margin;
  }

  // Gets the first page vertical tile padding, ignoring scaling for cardified
  // state.
  int GetUnscaledFirstPageTilePadding() {
    return unscaled_first_page_vertical_tile_padding_;
  }

  // Animates items to their ideal bounds when the reorder nudge gets removed.
  void AnimateOnNudgeRemoved();

  // Set the callback that runs when cardified state has ended.
  void SetCardifiedStateEndedTestCallback(
      base::RepeatingClosure cardified_ended_callback);

 private:
  friend class test::AppsGridViewTest;

  class BackgroundCardLayer;

  // Gets the leading padding for app list item grid on the first app list page.
  // Includes the space reserved for the continue seaction of the app list UI,
  // and additional vertical tile padding before the first row of apps when
  // needed (i.e. if the grid is shown under a row of recent apps).
  int GetTotalTopPaddingOnFirstPage() const;

  // Returns the size reserved for a single apps grid page. May not match the
  // tile grid size when the first page selected, as the first page may have
  // reduced number of tiles.
  gfx::Size GetPageSize() const;

  // Gets the tile grid size on the provided apps grid page.
  gfx::Size GetTileGridSizeForPage(int page) const;

  // Returns true if the page is the right target to flip to.
  bool IsValidPageFlipTarget(int page) const;

  // Obtains the target page to flip for |drag_point|.
  int GetPageFlipTargetForDrag(const gfx::Point& drag_point);

  // Starts the page flip timer if |drag_point| is in left/right side page flip
  // zone or is over page switcher.
  void MaybeStartPageFlipTimer(const gfx::Point& drag_point);

  // Invoked when |page_flip_timer_| fires.
  void OnPageFlipTimer();

  // Stops the timer that triggers a page flip during a drag.
  void StopPageFlipTimer();

  // Aborts cardified animations if any are running.
  void MaybeAbortExistingCardifiedAnimations();

  // Helper functions to start the Apps Grid Cardified state.
  // The cardified state scales down apps and is shown when the user drags an
  // app in the AppList.
  void StartAppsGridCardifiedView();

  // Ends the Apps Grid Cardified state and sets it to normal.
  void EndAppsGridCardifiedView();

  // Animates individual elements of the apps grid to and from cardified state.
  void AnimateCardifiedState();

  // Animate app list items in the app grid to and from cardified state.
  void AnimateAppListItemsForCardifiedState(
      views::AnimationSequenceBlock* animation_sequence,
      const gfx::Vector2d& translate_offset);

  // Called when all cardified layer animations finish.
  void OnCardifiedStateAnimationDone();

  // Called when app item animations are completed for ending cardified state.
  void OnCardifiedStateEnded();

  // Translates the items container view to center the current page in the apps
  // grid.
  void RecenterItemsContainer();

  // Calculates the background bounds for the grid depending on the value of
  // |cardified_state_|
  gfx::Rect BackgroundCardBounds(int new_page_index);

  // Appends a background card to the back of |background_cards_|.
  void AppendBackgroundCard();

  // Removes the background card at the end of |background_cards_|.
  void RemoveBackgroundCard();

  // Masks the apps grid container to background cards bounds.
  void MaskContainerToBackgroundBounds();

  // Removes all background cards from |background_cards_|.
  void RemoveAllBackgroundCards();

  // Updates the highlighted background card. Used only for cardified state.
  void SetHighlightedBackgroundCard(int new_highlighted_page);

  // Update the padding of tile view based on the contents bounds.
  void UpdateTilePadding();

  // Returns the padding between each page of the apps grid, or zero if the grid
  // does not use pages.
  int GetPaddingBetweenPages() const;

  // Created by AppListMainView, owned by views hierarchy.
  const raw_ptr<ContentsView> contents_view_;

  // Used to get information about whether a point is within the page flip drag
  // buffer area around this view.
  const raw_ptr<ContainerDelegate> container_delegate_;

  // Depends on |pagination_model_|.
  std::unique_ptr<PaginationController> pagination_controller_;

  // Timer to auto flip page when dragging an item near the left/right edges.
  base::OneShotTimer page_flip_timer_;

  // Target page to switch to when |page_flip_timer_| fires.
  int page_flip_target_ = -1;

  // Delay for when |page_flip_timer_| should fire after user drags an item near
  // the edge.
  base::TimeDelta page_flip_delay_;

  // Records smoothness of pagination animation.
  std::optional<ui::ThroughputTracker> pagination_metrics_tracker_;

  // Records the presentation time for apps grid dragging.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // The highlighted page during cardified state.
  int highlighted_page_ = -1;

  // Layer array for apps grid background cards. Used to display the background
  // card during cardified state.
  std::vector<std::unique_ptr<BackgroundCardLayer>> background_cards_;

  // Maximum number of rows on the first grid page.
  int max_rows_on_first_page_ = 0;

  // Maximum number of rows allowed in apps grid pages.
  int max_rows_ = 0;

  PaginationModel pagination_model_{this};

  // The amount that tiles need to be offset on the y-axis to avoid overlap
  // with the recent apps and continue section.
  int first_page_offset_ = 0;

  // Whether the apps grid is shown underneath recent apps container. If this is
  // the case, layout will add additional vertical tile padding before the first
  // apps grid row on the first page.
  bool shown_under_recent_apps_ = false;

  // Vertical tile spacing between the tile views on the first page.
  int first_page_vertical_tile_padding_ = 0;

  // Vertical tile spacing between the tile views on the first page, without
  // scaling applied from cardified state.
  int unscaled_first_page_vertical_tile_padding_ = 0;

  // A margin added to the height of the clip rect used for clipping the
  // cardified state's background cards.
  int margin_for_gradient_mask_ = 0;

  void StackCardsAtBottom() override;

  // Whether the apps grid is currently animating  the cardified state.
  bool is_animating_cardified_state_ = false;

  // The callback that runs once cardified state is ended.
  base::RepeatingClosure cardified_state_ended_test_callback_;

  // Used to abort cardified state enter and exit animations.
  std::unique_ptr<views::AnimationAbortHandle>
      cardified_animation_abort_handle_;

  base::WeakPtrFactory<PagedAppsGridView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_APP_LIST_VIEWS_PAGED_APPS_GRID_VIEW_H_
