// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
#define ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_

#include <memory>
#include <optional>

#include "ash/ash_export.h"
#include "ash/drag_drop/drag_image_view.h"
#include "ash/public/cpp/shelf_config.h"
#include "ash/public/cpp/shelf_model.h"
#include "ash/shelf/scroll_arrow_view.h"
#include "ash/shelf/shelf.h"
#include "ash/shelf/shelf_button_delegate.h"
#include "ash/shelf/shelf_container_view.h"
#include "ash/shelf/shelf_tooltip_delegate.h"
#include "ash/shelf/shelf_view.h"
#include "base/cancelable_callback.h"
#include "base/functional/callback_helpers.h"
#include "base/memory/raw_ptr.h"
#include "base/memory/weak_ptr.h"
#include "base/time/time.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/compositor/layer.h"
#include "ui/compositor/layer_animation_observer.h"
#include "ui/gfx/geometry/linear_gradient.h"
#include "ui/views/animation/ink_drop_host.h"
#include "ui/views/context_menu_controller.h"
#include "ui/views/controls/button/button.h"
#include "ui/views/widget/unique_widget_ptr.h"
#include "ui/views/widget/widget.h"

namespace views {
class FocusSearch;
}

namespace ui {
class PresentationTimeRecorder;
}

namespace ash {

class ASH_EXPORT ScrollableShelfView : public views::AccessiblePaneView,
                                       public ShelfView::Delegate,
                                       public ShellObserver,
                                       public ShelfConfig::Observer,
                                       public ShelfButtonDelegate,
                                       public ShelfTooltipDelegate,
                                       public views::ContextMenuController,
                                       public ui::ImplicitAnimationObserver {
  METADATA_HEADER(ScrollableShelfView, views::AccessiblePaneView)

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

  ScrollableShelfView(const ScrollableShelfView&) = delete;
  ScrollableShelfView& operator=(const ScrollableShelfView&) = delete;

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

  // Returns whether the view should adapt to RTL.
  bool ShouldAdaptToRTL() const;

  // Returns whether the scrollable shelf's current size is equal to the target
  // size.
  bool NeedUpdateToTargetBounds() const;

  // Returns the icon's target bounds in screen. The returned bounds are
  // calculated with the hotseat's target bounds instead of the actual bounds.
  // It helps to get the icon's final location before the bounds animation on
  // hotseat ends.
  gfx::Rect GetTargetScreenBoundsOfItemIcon(const ShelfID& id) const;

  // Returns whether scrollable shelf should show app buttons with scrolling
  // when the view size is |target_size| and app button size is |button_size|.
  bool RequiresScrollingForItemSize(const gfx::Size& target_size,
                                    int button_size) const;

  // Sets padding insets. `padding_insets` should adapt to RTL for the
  // horizontal shelf.
  void SetEdgePaddingInsets(const gfx::Insets& padding_insets);

  // Returns the edge padding insets based on the scrollable shelf view's
  // target bounds or the current bounds, indicated by |use_target_bounds|. Note
  // that the returned value is mirrored for the horizontal shelf under RTL.
  gfx::Insets CalculateMirroredEdgePadding(bool use_target_bounds) const;

  // Returns whether the shelf will be overflown (i.e. it will show one or both
  // arrow buttons) if it is given the input length.
  bool CalculateShelfOverflowForAvailableLength(int available_length) const;

  views::View* GetShelfContainerViewForTest();
  bool ShouldAdjustForTest() const;

  void SetTestObserver(TestObserver* test_observer);

  // Returns true if any shelf corner button has ripple ring activated.
  bool IsAnyCornerButtonInkDropActivatedForTest() const;

  // Returns the maximum scroll distance for the current layout.
  float GetScrollUpperBoundForTest() const;

  // Returns whether `page_flip_timer_` is running.
  bool IsPageFlipTimerBusyForTest() const;

  ShelfView* shelf_view() { return shelf_view_; }
  ShelfContainerView* shelf_container_view() { return shelf_container_view_; }
  const ShelfContainerView* shelf_container_view() const {
    return shelf_container_view_;
  }
  ScrollArrowView* left_arrow() { return left_arrow_; }
  const ScrollArrowView* left_arrow() const { return left_arrow_; }
  ScrollArrowView* right_arrow() { return right_arrow_; }
  const ScrollArrowView* right_arrow() const { return right_arrow_; }

  LayoutStrategy layout_strategy_for_test() const { return layout_strategy_; }
  gfx::Vector2dF scroll_offset_for_test() const { return scroll_offset_; }

  std::optional<size_t> first_tappable_app_index() const {
    return first_tappable_app_index_;
  }
  std::optional<size_t> last_tappable_app_index() const {
    return last_tappable_app_index_;
  }

  void set_default_last_focusable_child(bool default_last_focusable_child) {
    default_last_focusable_child_ = default_last_focusable_child;
  }

  void set_page_flip_time_threshold(base::TimeDelta page_flip_time_threshold) {
    page_flip_time_threshold_ = page_flip_time_threshold;
  }

  const gfx::Rect& visible_space() const { return visible_space_; }

  const gfx::Insets& edge_padding_insets() const {
    return edge_padding_insets_;
  }

  void set_is_padding_configured_externally(
      bool is_padding_configured_externally) {
    is_padding_configured_externally_ = is_padding_configured_externally;
  }

 private:
  friend class ShelfTestApi;

  class ScrollableShelfArrowView;
  class ScopedActiveInkDropCountImpl;

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

  // Sum of the shelf button size and the gap between shelf buttons.
  int GetSumOfButtonSizeAndSpacing() const;

  // Decides whether the current first visible shelf icon of the scrollable
  // shelf should be hidden or fully shown when gesture scroll ends.
  int GetGestureDragThreshold() const;

  // Returns the maximum scroll distance based on the given space for icons.
  float CalculateScrollUpperBound(int available_space_for_icons) const;

  // Returns the clamped scroll offset.
  float CalculateClampedScrollOffset(float scroll,
                                     int available_space_for_icons) const;

  // Creates the animation for scrolling shelf by |scroll_distance|.
  void StartShelfScrollAnimation(float scroll_distance);

  // Calculates the layout strategy based on:
  // (1) scroll offset on the main axis.
  // (2) length of the available space to accommodate shelf icons.
  LayoutStrategy CalculateLayoutStrategy(float scroll_distance_on_main_axis,
                                         int available_length) const;

  Shelf* GetShelf();
  const Shelf* GetShelf() const;

  // views::View:
  gfx::Size CalculatePreferredSize(
      const views::SizeBounds& available_size) const override;
  void Layout(PassKey) override;
  void ChildPreferredSizeChanged(views::View* child) override;
  void OnScrollEvent(ui::ScrollEvent* event) override;
  void OnMouseEvent(ui::MouseEvent* event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void GetAccessibleNodeData(ui::AXNodeData* node_data) override;
  void OnBoundsChanged(const gfx::Rect& previous_bounds) override;
  void ViewHierarchyChanged(
      const views::ViewHierarchyChangedDetails& details) override;
  void ScrollRectToVisible(const gfx::Rect& rect) override;
  std::unique_ptr<ui::Layer> RecreateLayer() override;

  // ShelfView::Delegate:
  void ScheduleScrollForItemDragIfNeeded(
      const gfx::Rect& location_in_screen) override;
  void CancelScrollForItemDrag() override;
  bool AreBoundsWithinVisibleSpace(
      const gfx::Rect& bounds_in_screem) const override;

  // ShelfButtonDelegate:
  void OnShelfButtonAboutToRequestFocusFromTabTraversal(ShelfButton* button,
                                                        bool reverse) override;
  void ButtonPressed(views::Button* sender,
                     const ui::Event& event,
                     views::InkDrop* ink_drop) override;
  void HandleAccessibleActionScrollToMakeVisible(ShelfButton* button) override;
  std::unique_ptr<ScopedActiveInkDropCount> CreateScopedActiveInkDropCount(
      const ShelfButton* sender) override;
  void OnButtonWillBeRemoved() override;
  void OnAppButtonActivated(const ShelfButton* button) override;

  // ContextMenuController:
  void ShowContextMenuForViewImpl(views::View* source,
                                  const gfx::Point& point,
                                  ui::MenuSourceType source_type) override;

  // ShellObserver:
  void OnShelfAlignmentChanged(aura::Window* root_window,
                               ShelfAlignment old_alignment) override;

  // ShelfConfig::Observer:
  void OnShelfConfigUpdated() override;

  // ShelfTooltipDelegate:
  bool ShouldShowTooltipForView(const views::View* view) const override;
  bool ShouldHideTooltip(const gfx::Point& cursor_location,
                         views::View* delegate_view) const override;
  const std::vector<aura::Window*> GetOpenWindowsForView(
      views::View* view) override;
  std::u16string GetTitleForView(const views::View* view) const override;
  views::View* GetViewForEvent(const ui::Event& event) override;

  // ui::ImplicitAnimationObserver:
  void OnImplicitAnimationsCompleted() override;

  // Returns whether the left/right arrow button should show based on the
  // current layout strategy. Because the visibility of arrow buttons is updated
  // during layout, which may happen asynchronously, we should not use arrow
  // buttons' visibility directly.
  bool ShouldShowLeftArrow() const;
  bool ShouldShowRightArrow() const;

  // Returns the local bounds depending on which view bounds are used: actual
  // view bounds or target view bounds.
  gfx::Rect GetAvailableLocalBounds(bool use_target_bounds) const;

  // Calculates padding for display centering alignment depending on which view
  // bounds are used: actual view bounds or target view bounds. The returned
  // value is mirrored for the horizontal shelf under RTL.
  gfx::Insets CalculateMirroredPaddingForDisplayCentering(
      bool use_target_bounds) const;

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

  // Scrolls the view to the target offset. |animating| indicates whether the
  // scroll animation is needed. Note that |target_offset| may be an illegal
  // value and may get adjusted in CalculateClampedScrollOffset function.
  void ScrollToMainOffset(float target_offset, bool animating);

  // Calculates the scroll distance to show a new page of shelf icons for
  // the given layout strategy. |forward| indicates whether the next page or
  // previous page is shown.
  float CalculatePageScrollingOffset(bool forward,
                                     LayoutStrategy layout_strategy) const;

  // Calculates the absolute value of page scroll distance.
  float CalculatePageScrollingOffsetInAbs(LayoutStrategy layout_strategy) const;

  // Calculates the target offset on the main axis after scrolling by
  // |scroll_distance| while the offset before scroll is |start_offset|.
  float CalculateTargetOffsetAfterScroll(float start_offset,
                                         float scroll_distance) const;

  // Updates the bounds of the gradient zone before/after the shelf
  // container.
  void UpdateGradientMask();
  void CalculateHorizontalGradient(gfx::LinearGradient* gradient_mask);
  void CalculateVerticalGradient(gfx::LinearGradient* gradient_mask);

  // Updates the visibility of gradient zones.
  void UpdateGradientZoneState();

  // Updates the gradient zone if the gradient zone's target bounds are
  // different from the actual values.
  void MaybeUpdateGradientZone();

  bool ShouldApplyMaskLayerGradientZone() const;

  // Returns the actual scroll offset for the given scroll distance along the
  // main axis under the specific layout strategy. When the left arrow button
  // shows, |shelf_view_| is translated due to the change in
  // |shelf_container_view_|'s bounds. That translation offset is not included
  // in |scroll_offset_|.
  float GetActualScrollOffset(float main_axis_scroll_distance,
                              LayoutStrategy layout_strategy) const;

  // Updates |first_tappable_app_index_| and |last_tappable_app_index_|.
  void UpdateTappableIconIndices();

  // Calculates the indices of the first/last tappable app under the given
  // layout strategy and offset along the main axis (that is the x-axis when
  // shelf is horizontally aligned or the y-axis if the shelf is vertically
  // aligned).
  std::pair<std::optional<size_t>, std::optional<size_t>>
  CalculateTappableIconIndices(LayoutStrategy layout_strategy,
                               int scroll_distance_on_main_axis) const;

  views::View* FindFirstFocusableChild();
  views::View* FindLastFocusableChild();

  // Returns the available space on the main axis for shelf icons.
  int GetSpaceForIcons() const;

  // Returns whether |available_size| is able to accommodate all shelf icons
  // without scrolling. |icons_preferred_size| is the space required by shelf
  // icons.
  bool CanFitAllAppsWithoutScrolling(
      const gfx::Size& available_size,
      const gfx::Size& icons_preferred_size) const;

  // Returns whether scrolling should be handled. |is_gesture_fling| is true
  // when the scrolling is triggered by gesture fling event; when it is false,
  // the scrolling is triggered by touch pad or mouse wheel event.
  bool ShouldHandleScroll(const gfx::Vector2dF& offset,
                          bool is_gesture_fling) const;

  // May initiate the scroll animation to ensure that the app icons are shown
  // correctly. Returns whether the animation is created.
  bool AdjustOffset();

  // Returns the offset by which the scroll distance along the main axis should
  // be adjusted to ensure the correct UI under the specific layout strategy.
  // Three parameters are needed: (1) scroll offset on the main axis (2) layout
  // strategy (3) available space for shelf icons.
  float CalculateAdjustmentOffset(int main_axis_scroll_distance,
                                  LayoutStrategy layout_strategy,
                                  int available_space_for_icons) const;

  int CalculateScrollDistanceAfterAdjustment(
      int main_axis_scroll_distance,
      LayoutStrategy layout_strategy) const;

  // Updates the available space for child views (such as the arrow button,
  // shelf view) which is smaller than the view's bounds due to paddings.
  void UpdateAvailableSpace();

  // Returns the clip rectangle of |shelf_container_view_| for the given layout
  // strategy. Note that |shelf_container_view_|'s bounds are the same with
  // ScrollableShelfView's. It is why we can use |visible_space_| directly
  // without coordinate transformation.
  gfx::Rect CalculateVisibleSpace(LayoutStrategy layout_strategy) const;

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

  // Returns whether a scroll event should be handled by this view or delegated
  // to the shelf.
  bool ShouldDelegateScrollToShelf(const ui::ScrollEvent& event) const;

  // Calculates the scroll distance along the main axis.
  float CalculateMainAxisScrollDistance() const;

  // Updates |scroll_offset_| from |target_offset_| using shelf alignment.
  // |scroll_offset_| may need to update in following cases: (1) View bounds are
  // changed. (2) View is scrolled. (3) A shelf icon is added/removed.
  void UpdateScrollOffset(float target_offset);

  // Updates the available space, which may also trigger the change in scroll
  // offset and layout strategy.
  void UpdateAvailableSpaceAndScroll();

  // Returns the scroll offset assuming view bounds being the target bounds.
  int CalculateScrollOffsetForTargetAvailableSpace(
      const gfx::Rect& target_space) const;

  // Returns whether |sender|'s activated ink drop should be counted.
  bool ShouldCountActivatedInkDrop(const views::View* sender) const;

  // Enable/disable the rounded corners of the shelf container.
  void EnableShelfRoundedCorners(bool enable);

  // Update the number of corner buttons with ripple ring activated. |increase|
  // indicates whether the number increases or decreases.
  void OnActiveInkDropChange(bool increase);

  // Returns whether layer clip should be enabled.
  bool ShouldEnableLayerClip() const;

  // Enable/disable the layer clip on |shelf_container_view_|.
  void EnableLayerClipOnShelfContainerView(bool enable);

  // Calculates the length of space required by shelf icons to show without
  // scroll. Note that the return value includes the padding space between the
  // app icon and the end of scrollable shelf.
  int CalculateShelfIconsPreferredLength() const;

  LayoutStrategy layout_strategy_ = kNotShowArrowButtons;

  // Child views Owned by views hierarchy.
  raw_ptr<ScrollArrowView> left_arrow_ = nullptr;
  raw_ptr<ScrollArrowView> right_arrow_ = nullptr;
  raw_ptr<ShelfContainerView> shelf_container_view_ = nullptr;

  // Available space to accommodate child views. It is mirrored for the
  // horizontal shelf under RTL.
  gfx::Rect available_space_;

  raw_ptr<ShelfView> shelf_view_ = nullptr;

  // Defines the padding space inside the scrollable shelf. It is decided by the
  // current padding strategy. Note that `edge_padding_insets_` is mirrored
  // for the horizontal shelf under RTL.
  gfx::Insets edge_padding_insets_;

  // Indicates whether |edge_padding_insets_| is configured externally.
  // Usually |edge_padding_insets_| is calculated by ScrollableShelfView's
  // member function. However, in some animations, |edge_padding_insets_|
  // is set by animation progress to ensure the smooth bounds transition.
  bool is_padding_configured_externally_ = false;

  // Visible space of |shelf_container_view| in ScrollableShelfView's local
  // coordinates. Different from |available_space_|, |visible_space_| only
  // contains app icons and is mirrored for horizontal shelf under RTL.
  gfx::Rect visible_space_;

  gfx::Vector2dF scroll_offset_;

  ScrollStatus scroll_status_ = kNotInScroll;

  // Gesture states are preserved when the gesture scrolling along the main axis
  // (that is, whether it is scrolling horizontally for bottom shelf, or whether
  // it is scrolling horizontally for left/right shelf) gets started. They help
  // to handle the gesture fling event.
  gfx::Vector2dF scroll_offset_before_main_axis_scrolling_;
  LayoutStrategy layout_strategy_before_main_axis_scrolling_ =
      kNotShowArrowButtons;

  std::unique_ptr<views::FocusSearch> focus_search_;

  // The index of the first/last tappable app index.
  std::optional<size_t> first_tappable_app_index_ = std::nullopt;
  std::optional<size_t> last_tappable_app_index_ = std::nullopt;

  // The number of corner buttons whose ink drop is activated.
  int activated_corner_buttons_ = 0;

  // Whether this view should focus its last focusable child (instead of its
  // first) when focused.
  bool default_last_focusable_child_ = false;

  // Indicates whether the focus ring on shelf items contained by
  // ScrollableShelfView is enabled.
  bool focus_ring_activated_ = false;

  // Indicates that the view is during the scroll animation.
  bool during_scroll_animation_ = false;

  // Indicates whether the gradient zone before/after the shelf container view
  // should show.
  bool should_show_start_gradient_zone_ = false;
  bool should_show_end_gradient_zone_ = false;

  // Waiting time before flipping the page.
  base::TimeDelta page_flip_time_threshold_;

  raw_ptr<TestObserver> test_observer_ = nullptr;

  // If page flip timer is active for shelf item drag, the last known drag item
  // bounds in screen coordinates.
  std::optional<gfx::Rect> drag_item_bounds_in_screen_;

  base::OneShotTimer page_flip_timer_;

  // Indicates whether the layer clip should be applied to
  // |shelf_container_view_| in non-overflow mode.
  bool layer_clip_in_non_overflow_ = false;

  // Records the presentation time for the scrollable shelf dragging.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  base::ScopedClosureRunner force_show_hotseat_resetter_;

  base::WeakPtrFactory<ScrollableShelfView> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_SHELF_SCROLLABLE_SHELF_VIEW_H_
