// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESKS_BAR_VIEW_H_
#define ASH_WM_DESKS_DESKS_BAR_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/callback_list.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ash {

class DesksBarScrollViewLayout;
class DeskBarHoverObserver;
class DeskDragProxy;
class DeskMiniView;
class ExpandedDesksBarButton;
class OverviewGrid;
class PersistentDesksBarVerticalDotsButton;
class PillButton;
class ScrollArrowButton;
class ZeroStateDefaultDeskButton;
class ZeroStateIconButton;

// A bar that resides at the top portion of the overview mode's ShieldView,
// which contains the virtual desks mini_views, as well as the new desk button.
class ASH_EXPORT DesksBarView : public views::View,
                                public DesksController::Observer {
 public:
  explicit DesksBarView(OverviewGrid* overview_grid);

  DesksBarView(const DesksBarView&) = delete;
  DesksBarView& operator=(const DesksBarView&) = delete;

  ~DesksBarView() override;

  static constexpr int kZeroStateBarHeight = 40;

  // Returns the height of the expanded desks bar that exists on `root`. The
  // height of zero state desks bar is `kZeroStateBarHeight`.
  static int GetExpandedBarHeight(aura::Window* root);

  // Creates and returns the widget that contains the DeskBarView in overview
  // mode. The returned widget has no content view yet, and hasn't been shown
  // yet.
  static std::unique_ptr<views::Widget> CreateDesksWidget(
      aura::Window* root,
      const gfx::Rect& bounds);

  void set_is_bounds_animation_on_going(bool value) {
    is_bounds_animation_on_going_ = value;
  }

  PillButton* up_next_button() const { return up_next_button_; }

  ZeroStateDefaultDeskButton* zero_state_default_desk_button() const {
    return zero_state_default_desk_button_;
  }

  ZeroStateIconButton* zero_state_new_desk_button() const {
    return zero_state_new_desk_button_;
  }

  ExpandedDesksBarButton* expanded_state_new_desk_button() const {
    return expanded_state_new_desk_button_;
  }

  ZeroStateIconButton* zero_state_desks_templates_button() const {
    return zero_state_desks_templates_button_;
  }

  ExpandedDesksBarButton* expanded_state_desks_templates_button() const {
    return expanded_state_desks_templates_button_;
  }

  const std::vector<DeskMiniView*>& mini_views() const { return mini_views_; }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  OverviewGrid* overview_grid() const { return overview_grid_; }

  // Initializes and creates mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  void Init();

  // Returns true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

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
  // Start to drag. Scale up the drag proxy. `is_mouse_dragging` is true when
  // triggered by mouse/trackpad, false when triggered by touch.
  void StartDragDesk(DeskMiniView* mini_view,
                     const gfx::PointF& location_in_screen,
                     bool is_mouse_dragging);
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

  // Called when the saved desk library is hidden. Transitions the desks bar
  // view to zero state if necessary.
  void OnSavedDeskLibraryHidden();

  // views::View:
  const char* GetClassName() const override;
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

  // This is called on initialization, creating a new desk through the
  // NewDeskButton or ExpandedDesksBarButton, or expanding from zero state
  // bar to the expanded desks bar. Performs the expanding animation if
  // |expanding_bar_view| is true, otherwise animates the mini_views (also the
  // ExpandedDesksBarButton) to their final positions if
  // |initializing_bar_view| is false.
  void UpdateNewMiniViews(bool initializing_bar_view, bool expanding_bar_view);

  // If the focused |mini_view| is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen.
  void ScrollToShowMiniViewIfNecessary(const DeskMiniView* mini_view);

  void OnNewDeskButtonPressed(
      DesksCreationRemovalSource desks_creation_removal_source);

  // If in expanded state, updates the border color of the
  // `expanded_state_desks_templates_button_` and the active desk's mini view
  // after the saved desk library has been shown. If not in expanded state,
  // updates the background color of the `zero_state_desks_templates_button_`
  // and the `zero_state_default_desk_button_`.
  void UpdateButtonsForSavedDeskGrid();

  // Updates the visibility of the two buttons inside the zero state desks bar
  // and the ExpandedDesksBarButton on the desk bar's state.
  void UpdateDeskButtonsVisibility();

  // Updates the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled, the user has any saved desks and the
  // state of the desks bar.
  void UpdateLibraryButtonVisibility();

  // Returns the mini_view associated with `desk` or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Animates the bar from expanded state to zero state. Clears `mini_views_`.
  void SwitchToZeroState();

  // Bring focus to the name view of the desk with `desk_index`.
  void NudgeDeskName(int desk_index);

 private:
  friend class DesksBarScrollViewLayout;
  friend class DesksTestApi;

  // Determine the new index of the dragged desk at the position of
  // |location_in_screen|.
  int DetermineMoveIndex(int location_in_screen) const;

  // If drag a desk over a scroll button (i.e., the desk intersects the button),
  // scroll the desk bar. If the desk is dropped or leaves the button, end
  // scroll. Return true if the scroll is triggered. Return false if the scroll
  // is ended.
  bool MaybeScrollByDraggedDesk();

  // Returns the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  // Updates the visibility of |left_scroll_button_| and |right_scroll_button_|.
  // Show |left_scroll_button_| if there are contents outside of the left edge
  // of the |scroll_view_|, the same for |right_scroll_button_| based on the
  // right side of the |scroll_view_|.
  void UpdateScrollButtonsVisibility();

  // We will show a fade in gradient besides |left_scroll_button_| and a fade
  // out gradient besides |right_scroll_button_|. Show the gradient only when
  // the corresponding scroll button is visible.
  void UpdateGradientMask();

  // Scrolls the desks bar to the previous or next page. The page size is the
  // width of the scroll view, the contents that are outside of the scroll view
  // will be clipped and can not be seen.
  void ScrollToPreviousPage();
  void ScrollToNextPage();

  // Gets the adjusted scroll position based on |position| to make sure no desk
  // preview is cropped at the start position of the scrollable bar.
  int GetAdjustedUncroppedScrollPosition(int position) const;

  void OnLibraryButtonPressed();

  // If the `DesksCloseAll` flag is enabled, this function cycles through
  // `mini_views_` and updates the tooltip for each mini view's combine desks
  // button.
  void MaybeUpdateCombineDesksTooltips();

  // Scrollview callbacks.
  void OnContentsScrolled();
  void OnContentsScrollEnded();

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

  // Puts the contents in a ScrollView to support scrollable desks.
  views::ScrollView* scroll_view_ = nullptr;

  // Contents of `scroll_view_`, which includes `mini_views_`,
  // `expanded_state_new_desk_button_` and optionally
  // `expanded_state_desks_templates_button_` currently.
  views::View* scroll_view_contents_ = nullptr;

  // True if the `DesksBarBoundsAnimation` is started and hasn't finished yet.
  // It will be used to hold `Layout` until the bounds animation is completed.
  // `Layout` is expensive and will be called on bounds changes, which means it
  // will be called lots of times during the bounds changes animation. This is
  // done to eliminate the unnecessary `Layout` calls during the animation.
  bool is_bounds_animation_on_going_ = false;

  // Button to return to the glanceables screen.
  PillButton* up_next_button_ = nullptr;

  ZeroStateDefaultDeskButton* zero_state_default_desk_button_ = nullptr;
  ZeroStateIconButton* zero_state_new_desk_button_ = nullptr;
  ExpandedDesksBarButton* expanded_state_new_desk_button_ = nullptr;

  // Buttons to show the desks templates grid.
  ZeroStateIconButton* zero_state_desks_templates_button_ = nullptr;
  ExpandedDesksBarButton* expanded_state_desks_templates_button_ = nullptr;

  ScrollArrowButton* left_scroll_button_ = nullptr;
  ScrollArrowButton* right_scroll_button_ = nullptr;
  // Mini view whose preview is being dragged.
  DeskMiniView* drag_view_ = nullptr;
  // Drag proxy for the dragged desk.
  std::unique_ptr<DeskDragProxy> drag_proxy_;

  // A circular button which when clicked will open the context menu of the
  // persistent desks bar. Note that this button will only be created when
  // persistent desks bar should be shown.
  PersistentDesksBarVerticalDotsButton* vertical_dots_button_ = nullptr;

  // ScrollView callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESKS_BAR_VIEW_H_
