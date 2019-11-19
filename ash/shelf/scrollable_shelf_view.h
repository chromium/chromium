// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
#define ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_

#include <memory>

#include "ash/app_list/views/app_list_drag_and_drop_host.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/scroll_arrow_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_container_view.h"
#include "ash/shelf/shelf_tooltip_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/views/animation/ink_drop_host_view.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"

namespace views {
class FocusSearch;
}

namespace ash {
class PresentationTimeRecorder;

class ASH_EXPORT ScrollableShelfView : public views::AccessiblePaneView,
                                       public ShellObserver,
                                       public ShelfButtonDelegate,
                                       public ShelfTooltipDelegate,
                                       public views::ContextMenuController,
                                       public ApplicationDragAndDropHost,
                                       public ui::ImplicitAnimationObserver {
 public:
  class TestObserver {
   public:
    virtual ~TestObserver() = default;
    virtual void OnPageFlipTimerFired() = 0;
  };

  enum LayoutStrategy {
    // The arrow buttons are not shown. It means that there is enough space to
    // accommodate all of shelf icons.
    kNotShowArrowButtons,

    // Only the left arrow button is shown.
    kShowLeftArrowButton,

    // Only the right arrow button is shown.
    kShowRightArrowButton,

    // Both buttons are shown.
    kShowButtons
  };

  ScrollableShelfView(ShelfModel* model, Shelf* shelf);
  ~ScrollableShelfView() override;

  void Init();

  // Called when the focus ring for ScrollableShelfView is enabled/disabled.
  // |activated| is true when enabling the focus ring.
  void OnFocusRingActivationChanged(bool activated);

  // Scrolls to a new page of shelf icons. |forward| indicates whether the next
  // page or previous page is shown.
  void ScrollToNewPage(bool forward);

  // AccessiblePaneView:
  views::FocusSearch* GetFocusSearch() override;
  views::FocusTraversable* GetFocusTraversableParent() override;
  views::View* GetFocusTraversableParentView() override;
  views::View* GetDefaultFocusableChild() override;

  // Returns the |available_space_|.
  gfx::Rect GetHotseatBackgroundBounds() const;

  views::View* GetShelfContainerViewForTest();
  bool ShouldAdjustForTest() const;

  void SetTestObserver(TestObserver* test_observer);

  ShelfView* shelf_view() { return shelf_view_; }
  ShelfContainerView* shelf_container_view() { return shelf_container_view_; }
  ScrollArrowView* left_arrow() { return left_arrow_; }
  ScrollArrowView* right_arrow() { return right_arrow_; }

  LayoutStrategy layout_strategy_for_test() const { return layout_strategy_; }
  gfx::Vector2dF scroll_offset_for_test() const { return scroll_offset_; }

  int first_tappable_app_index() { return first_tappable_app_index_; }
  int last_tappable_app_index() { return last_tappable_app_index_; }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  void set_page_flip_time_threshold(base::TimeDelta page_flip_time_threshold) {
    page_flip_time_threshold_ = page_flip_time_threshold;
  }

  const gfx::Rect& visible_space() const { return visible_space_; }

  // Size of the arrow button.
  static int GetArrowButtonSize();

  // Padding at the two ends of the shelf.
  static constexpr int kEndPadding = 4;

 private:
  class GradientLayerDelegate;
  class ScrollableShelfArrowView;

  struct FadeZone {
    // Bounds of the fade in/out zone.
    gfx::Rect zone_rect;

    // Specifies the type of FadeZone: fade in or fade out.
    bool fade_in = false;

    // Indicates the drawing direction.
    bool is_horizontal = false;
  };

  enum ScrollStatus {
    // Indicates whether the gesture scrolling is across the main axis.
    // That is, whether it is scrolling vertically for bottom shelf, or
    // whether it is scrolling horizontally for left/right shelf.
    kAcrossMainAxisScroll,

    // Indicates whether the gesture scrolling is along the main axis.
    // That is, whether it is scrolling horizontally for bottom shelf, or
    // whether it is scrolling vertically for left/right shelf.
    kAlongMainAxisScroll,

    // Not in scrolling.
    kNotInScroll
  };

  // Returns the maximum scroll distance.
  int CalculateScrollUpperBound() const;

  // Returns the clamped scroll offset.
  float CalculateClampedScrollOffset(float scroll) const;

  // Creates the animation for scrolling shelf by |scroll_distance|.
  void StartShelfScrollAnimation(float scroll_distance);

  // Calculates the layout strategy based on the available space and scroll
  // distance.
  LayoutStrategy CalculateLayoutStrategy(
      int scroll_distance_on_main_axis) const;

  // Returns whether the view should adapt to RTL.
  bool ShouldAdaptToRTL() const;

  // Returns whether the app icon layout should be centering alignment.
  bool ShouldApplyDisplayCentering() const;

  Shelf* GetShelf();
  const Shelf* GetShelf() const;

  // views::View:
  gfx::Size CalculatePreferredSize() const override;
  void Layout() override;
  void ChildPreferredSizeChanged(views::View* child) override;
  const char* GetClassName() const override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;
  void HandleAccessibleActionScrollToMakeVisible(ShelfButton* button) override;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // Overridden from ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window) override;

  // ShelfTooltipDelegate:
  bool ShouldShowTooltipForView(const views::View* view) const override;
  bool ShouldHideTooltip(const gfx::Point& cursor_location) const override;
  const std::vector<aura::Window*> GetOpenWindowsForView(
      views::View* view) override;
  base::string16 GetTitleForView(const views::View* view) const override;
  views::View* GetViewForEvent(const ui::Event& event) override;

  // ApplicationDragAndDropHost:
  void CreateDragIconProxyByLocationWithNoAnimation(
      const gfx::Point& origin_in_screen_coordinates,
      const gfx::ImageSkia& icon,
      views::View* replaced_view,
      float scale_factor,
      int blur_radius) override;
  void UpdateDragIconProxy(
      const gfx::Point& location_in_screen_coordinates) override;
  void DestroyDragIconProxy() override;
  bool StartDrag(const std::string& app_id,
                 const gfx::Point& location_in_screen_coordinates) override;
  bool Drag(const gfx::Point& location_in_screen_coordinates) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Returns the padding inset. Different Padding strategies for three scenarios
  // (1) display centering alignment
  // (2) scrollable shelf centering alignment
  // (3) overflow mode
  gfx::Insets CalculateEdgePadding() const;

  // Calculates padding for display centering alignment.
  gfx::Insets CalculatePaddingForDisplayCentering() const;

  // Returns whether the received gesture event should be handled here.
  bool ShouldHandleGestures(const ui::GestureEvent& event);

  // Resets the attributes related with gesture scroll to their default values.
  void ResetScrollStatus();

  // Handles events for scrolling the shelf. Returns whether the event has been
  // consumed.
  bool ProcessGestureEvent(const ui::GestureEvent& event);

  void HandleMouseWheelEvent(ui::MouseWheelEvent* event);

  // Scrolls the view by distance of |x_offset| or |y_offset|. |animating|
  // indicates whether the animation displays. |x_offset| or |y_offset| has to
  // be float. Otherwise the slow gesture drag is neglected.
  void ScrollByXOffset(float x_offset, bool animating);
  void ScrollByYOffset(float y_offset, bool animating);

  // Scrolls the view to the target offset. After scrolling, |scroll_offset_| is
  // |x_dst_offset| or |y_dst_offset|. |animating| indicates whether the
  // animation shows.
  void ScrollToXOffset(float x_target_offset, bool animating);
  void ScrollToYOffset(float y_target_offset, bool animating);

  // Calculates the scroll distance to show a new page of shelf icons for
  // the given layout strategy. |forward| indicates whether the next page or
  // previous page is shown.
  float CalculatePageScrollingOffset(bool forward,
                                     LayoutStrategy layout_strategy) const;

  // Updates the gradient zone.
  void UpdateGradientZone();

  // Calculates the bounds of the gradient zone before/after the shelf
  // container.
  FadeZone CalculateStartGradientZone() const;
  FadeZone CalculateEndGradientZone() const;

  // Updates the visibility of gradient zones.
  void UpdateGradientZoneState();

  // Returns the actual scroll offset on the view's main axis. When the left
  // arrow button shows, |shelf_view_| is translated due to the change in
  // |shelf_container_view_|'s bounds. That translation offset is not included
  // in |scroll_offset_|.
  int GetActualScrollOffset() const;

  // Updates |first_tappable_app_index_| and |last_tappable_app_index_|.
  void UpdateTappableIconIndices();

  // Calculates the indices of the first/last tappable app under the given
  // layout strategy and offset along the main axis (that is the x-axis when
  // shelf is horizontally aligned or the y-axis if the shelf is vertically
  // aligned).
  std::pair<int, int> CalculateTappableIconIndices(
      LayoutStrategy layout_strategy,
      int scroll_distance_on_main_axis) const;

  views::View* FindFirstFocusableChild();
  views::View* FindLastFocusableChild();

  // Returns the available space on the main axis for shelf icons.
  int GetSpaceForIcons() const;

  // Returns whether there is available space to accommodate all shelf icons.
  bool CanFitAllAppsWithoutScrolling() const;

  // Returns whether scrolling should be handled. |is_gesture_fling| is true
  // when the scrolling is triggered by gesture fling event; when it is false,
  // the scrolling is triggered by touch pad or mouse wheel event.
  bool ShouldHandleScroll(const gfx::Vector2dF& offset,
                          bool is_gesture_fling) const;

  // Ensures that the app icons are shown correctly.
  void AdjustOffset();

  // Returns the offset by which the shelf view should be translated to ensure
  // the correct UI.
  int CalculateAdjustedOffset() const;

  void UpdateVisibleSpace();

  // Calculates the padding insets which help to show the edging app icon's
  // ripple ring correctly.
  gfx::Insets CalculateRipplePaddingInsets() const;

  // Calculates the rounded corners for |shelf_container_view_|. It contributes
  // to cut off the child view out of the scrollable shelf's bounds, such as
  // ripple ring.
  gfx::RoundedCornersF CalculateShelfContainerRoundedCorners() const;

  // Scrolls to a new page if |drag_icon_| is dragged out of |visible_space_|
  // for enough time. The function is called when |page_flip_timer_| is fired.
  void OnPageFlipTimer();

  bool IsDragIconWithinVisibleSpace() const;

  // Returns whether a scroll event should be handled by this view or delegated
  // to the shelf.
  bool ShouldDelegateScrollToShelf(const ui::ScrollEvent& event) const;

  // Calculates the scroll distance along the main axis.
  float CalculateMainAxisScrollDistance() const;

  LayoutStrategy layout_strategy_ = kNotShowArrowButtons;

  // Child views Owned by views hierarchy.
  ScrollArrowView* left_arrow_ = nullptr;
  ScrollArrowView* right_arrow_ = nullptr;
  ShelfContainerView* shelf_container_view_ = nullptr;

  // Available space to accommodate child views. It is mirrored for horizontal
  // shelf under RTL.
  gfx::Rect available_space_;

  // Visible space of |shelf_container_view| in ScrollableShelfView's local
  // coordinates. Different from |available_space_|, |visible_space_| only
  // contains app icons and is mirrored for horizontal shelf under RTL.
  gfx::Rect visible_space_;

  ShelfView* shelf_view_ = nullptr;

  gfx::Vector2dF scroll_offset_;

  ScrollStatus scroll_status_ = kNotInScroll;

  // Gesture states are preserved when the gesture scrolling along the main axis
  // (that is, whether it is scrolling horizontally for bottom shelf, or whether
  // it is scrolling horizontally for left/right shelf) gets started. They help
  // to handle the gesture fling event.
  gfx::Vector2dF scroll_offset_before_main_axis_scrolling_;
  LayoutStrategy layout_strategy_before_main_axis_scrolling_ =
      kNotShowArrowButtons;

  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // The index of the first/last tappable app index.
  int first_tappable_app_index_ = -1;
  int last_tappable_app_index_ = -1;

  // Whether this view should focus its last focusable child (instead of its
  // first) when focused.
  bool default_last_focusable_child_ = false;

  // Indicates whether the focus ring on shelf items contained by
  // ScrollableShelfView is enabled.
  bool focus_ring_activated_ = false;

  // Indicates that the view is during the scrolling animation.
  bool during_scrolling_animation_ = false;

  // Indicates whether the gradient zone before/after the shelf container view
  // should show.
  bool should_show_start_gradient_zone_ = false;
  bool should_show_end_gradient_zone_ = false;

  // Waiting time before flipping the page.
  base::TimeDelta page_flip_time_threshold_;

  TestObserver* test_observer_ = nullptr;

  // Replaces the dragged app icon during drag procedure. It ensures that the
  // app icon can be dragged out of the shelf view.
  std::unique_ptr<DragImageView> drag_icon_;

  base::OneShotTimer page_flip_timer_;

  // Metric reporter for scrolling animations.
  const std::unique_ptr<ui::AnimationMetricsReporter>
      animation_metrics_reporter_;

  // Records the presentation time for the scrollable shelf dragging.
  std::unique_ptr<PresentationTimeRecorder> presentation_time_recorder_;

  DISALLOW_COPY_AND_ASSIGN(ScrollableShelfView);
};

}  // namespace ash

#endif  // ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
