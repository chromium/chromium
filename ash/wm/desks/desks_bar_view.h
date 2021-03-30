// Copyright 2019 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_BAR_VIEW_H_
#define ASH_WM_DESKS_DESKS_BAR_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/macros.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ash {

class BentoDesksBarLayout;
class BentoDesksBarScrollViewLayout;
class DeskBarHoverObserver;
class DeskDragProxy;
class DeskMiniView;
class ExpandedStateNewDeskButton;
class GradientLayerDelegate;
class NewDeskButton;
class OverviewGrid;
class ScrollArrowButton;
class ZeroStateDefaultDeskButton;
class ZeroStateNewDeskButton;

// A bar that resides at the top portion of the overview mode's ShieldView,
// which contains the virtual desks mini_views, as well as the new desk button.
class ASH_EXPORT DesksBarView : public views::View,
                                public DesksController::Observer,
                                public views::ScrollView::Observer {
 public:
  explicit DesksBarView(OverviewGrid* overview_grid);
  ~DesksBarView() override;

  static constexpr int kZeroStateBarHeight = 40;

  // Returns the height of the desk bar view which is based on the given |width|
  // of the overview grid that exists on |root| (which is the same as the width
  // of the bar) and |desks_bar_view|'s content (since they may not fit the
  // given |width| forcing us to use the compact layout).
  // If |desks_bar_view| is nullptr, the height returned will be solely based on
  // the |width|.
  static int GetBarHeightForWidth(aura::Window* root,
                                  const DesksBarView* desks_bar_view,
                                  int width);

  // Creates and returns the widget that contains the DeskBarView in overview
  // mode. The returned widget has no content view yet, and hasn't been shown
  // yet.
  static std::unique_ptr<views::Widget> CreateDesksWidget(
      aura::Window* root,
      const gfx::Rect& bounds);

  views::View* background_view() const { return background_view_; }

  NewDeskButton* new_desk_button() const { return new_desk_button_; }

  ZeroStateDefaultDeskButton* zero_state_default_desk_button() const {
    return zero_state_default_desk_button_;
  }

  ZeroStateNewDeskButton* zero_state_new_desk_button() const {
    return zero_state_new_desk_button_;
  }

  ExpandedStateNewDeskButton* expanded_state_new_desk_button() const {
    return expanded_state_new_desk_button_;
  }

  const std::vector<DeskMiniView*>& mini_views() const { return mini_views_; }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  // Initializes and creates mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  void Init();

  // Returns true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

  // Returns the scale factor by which a window's size will be scaled down when
  // it is dragged and hovered on this desks bar.
  float GetOnHoverWindowSizeScaleFactor() const;

  // Get the index of a desk mini view in the |mini_views|.
  int GetMiniViewIndex(const DeskMiniView* mini_view) const;

  // Updates the visibility state of the close buttons on all the mini_views as
  // a result of mouse and gesture events.
  void OnHoverStateMayHaveChanged();
  void OnGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Called when an item is being dragged in overview mode to update whether it
  // is currently intersecting with this view, and the |screen_location| of the
  // current drag position.
  void SetDragDetails(const gfx::Point& screen_location,
                      bool dragged_item_over_bar);

  // Returns true if it is in zero state. It is the state of the desks bar when
  // there's only a single desk available, in which case the bar is shown in a
  // minimized state.
  bool IsZeroState() const;
  // Handle the mouse press event from a desk preview.
  void HandlePressEvent(DeskMiniView* mini_view, const ui::LocatedEvent& event);
  // Handle the gesture long press event from a desk preview.
  void HandleLongPressEvent(DeskMiniView* mini_view,
                            const ui::LocatedEvent& event);
  // Handle the drag event from a desk preview.
  void HandleDragEvent(DeskMiniView* mini_view, const ui::LocatedEvent& event);
  // Handle the release event from a desk preview. Return true if a drag event
  // is ended.
  bool HandleReleaseEvent(DeskMiniView* mini_view,
                          const ui::LocatedEvent& event);

  // Finalize any unfinished drag & drop. Initialize a new drag proxy.
  void InitDragDesk(DeskMiniView* mini_view,
                    const gfx::PointF& location_in_screen);
  // Start to drag. Scale up the drag proxy.
  void StartDragDesk(DeskMiniView* mini_view,
                     const gfx::PointF& location_in_screen);
  // Reorder desks according to the drag proxy's location.
  void ContinueDragDesk(DeskMiniView* mini_view,
                        const gfx::PointF& location_in_screen);
  // If the desk is dropped by user (|end_by_user| = true), scale down and snap
  // back the drag proxy. Otherwise, directly finalize the drag & drop. Note
  // that when we want to end the current drag immediately, if the drag is
  // initialized but did not start, |FinalizeDragDesk| should be use; if the
  // drag started, |EndDragDesk| should be used with |end_by_user| = false.
  void EndDragDesk(DeskMiniView* mini_view, bool end_by_user);
  // Reset the drag view and the drag proxy.
  void FinalizeDragDesk();
  // If a desk is in a drag & drop cycle.
  bool IsDraggingDesk() const;

  // views::View:
  const char* GetClassName() const override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;
  void OnThemeChanged() override;

  // Returns true if the width of the DesksBarView is below a defined
  // threshold or the contents no longer fit within this object's bounds in
  // default mode, suggesting a compact small screens layout should be used for
  // both itself and its children.
  bool UsesCompactLayout() const;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskSwitchAnimationLaunching() override;
  void OnDeskSwitchAnimationFinished() override;

  // views::ScrollView::Observer:
  void OnContentsScrolled() override;

  // This is called on initialization, creating a new desk through the
  // NewDeskButton or ExpandedStateNewDeskButton, or expanding from zero state
  // bar to the expanded desks bar when Bento is enabled. Performs the expanding
  // animation if |expanding_bar_view| is true, otherwise animates the
  // mini_views (also the ExpandedStateNewDeskButton if Bento is enabled) to
  // their final positions if |initializing_bar_view| is false.
  void UpdateNewMiniViews(bool initializing_bar_view, bool expanding_bar_view);

  // If the focused |mini_view| is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen.
  void ScrollToShowMiniViewIfNecessary(const DeskMiniView* mini_view);

  ScrollArrowButton* GetLeftScrollButtonForTesting() const {
    return left_scroll_button_;
  }
  ScrollArrowButton* GetRightScrollButtonForTesting() const {
    return right_scroll_button_;
  }
  views::ScrollView* GetScrollViewForTesting() const { return scroll_view_; }

  // Get drag view for test.
  const DeskMiniView* GetDragDeskMiniViewForTesting() const {
    return drag_view_;
  }

 private:
  friend class BentoDesksBarLayout;
  friend class BentoDesksBarScrollViewLayout;

  // Determine the new index of the dragged desk at the position of
  // |location_in_screen|.
  int DetermineMoveIndex(int location_in_screen) const;

  // If drag a desk over a scroll button (i.e., the desk intersects the button),
  // scroll the desk bar. If the desk is dropped or leaves the button, end
  // scroll. Return true if the scroll is triggered. Return false if the scroll
  // is ended.
  bool MaybeScrollByDraggedDesk();

  // Returns the mini_view associated with |desk| or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Returns the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  // Updates the cached minimum width required to fit all contents.
  void UpdateMinimumWidthToFitContents();

  // Adds |mini_view| as the DesksBarView's child or |scroll_view_content_|'s
  // child if Bento is enabled.
  DeskMiniView* AddMiniViewAsChild(std::unique_ptr<DeskMiniView> mini_view);

  // Updates the visibility of the two buttons inside the zero state desks bar
  // and the ExpandedStateNewDeskButton on the desk bar's state. Used only when
  // Bento is enabled.
  void UpdateBentoDeskButtonsVisibility();

  // Updates the visibility of |left_scroll_button_| and |right_scroll_button_|.
  // Show |left_scroll_button_| if there are contents outside of the left edge
  // of the |scroll_view_|, the same for |right_scroll_button_| based on the
  // right side of the |scroll_view_|.
  void UpdateScrollButtonsVisibility();

  // We will show a fade in gradient besides |left_scroll_button_| and a fade
  // out gradient besides |right_scroll_button_|. Show the gradient only when
  // the corresponding scroll button is visible.
  void UpdateGradientZone();

  // Scrolls the desks bar to the previous or next page. The page size is the
  // width of the scroll view, the contents that are outside of the scroll view
  // will be clipped and can not be seen.
  void ScrollToPreviousPage();
  void ScrollToNextPage();

  // A view that shows a dark gary transparent background that can be animated
  // when the very first mini_views are created.
  views::View* background_view_;

  // Used only in classic desks. Will be removed once Bento is fully launched.
  NewDeskButton* new_desk_button_ = nullptr;

  // The views representing desks mini_views. They're owned by views hierarchy.
  std::vector<DeskMiniView*> mini_views_;

  // Observes mouse events on the desks bar widget and updates the states of the
  // mini_views accordingly.
  std::unique_ptr<DeskBarHoverObserver> hover_observer_;

  // The screen location of the most recent drag position. This value is valid
  // only when the below `dragged_item_on_bar_` is true.
  gfx::Point last_dragged_item_screen_location_;

  // True when the drag location of the overview item is intersecting with this
  // view.
  bool dragged_item_over_bar_ = false;

  // The OverviewGrid that contains this object.
  OverviewGrid* overview_grid_;

  // Caches the calculated minimum width to fit contents.
  int min_width_to_fit_contents_ = 0;

  // Puts the contents in a ScrollView to support scrollable desks. Used only
  // when Bento is enabled.
  views::ScrollView* scroll_view_ = nullptr;

  // Contents of |scroll_view_|, which includes |mini_views_| and
  // |new_desk_button_| currently. Used only when Bento is enabled.
  views::View* scroll_view_contents_ = nullptr;

  // Used only when Bento is enabled.
  ZeroStateDefaultDeskButton* zero_state_default_desk_button_ = nullptr;
  ZeroStateNewDeskButton* zero_state_new_desk_button_ = nullptr;
  ExpandedStateNewDeskButton* expanded_state_new_desk_button_ = nullptr;
  ScrollArrowButton* left_scroll_button_ = nullptr;
  ScrollArrowButton* right_scroll_button_ = nullptr;
  // Mini view whose preview is being dragged.
  DeskMiniView* drag_view_ = nullptr;
  // Drag proxy for the dragged desk.
  std::unique_ptr<DeskDragProxy> drag_proxy_;

  // The layer delegate used for |scroll_view_|'s mask layer, with left and
  // right gradient asides the scroll buttons.
  std::unique_ptr<GradientLayerDelegate> gradient_layer_delegate_;

  DISALLOW_COPY_AND_ASSIGN(DesksBarView);
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_BAR_VIEW_H_
