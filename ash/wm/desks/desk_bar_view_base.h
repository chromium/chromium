// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/cros_next_default_desk_button.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_drag_proxy.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/expanded_desks_bar_button.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "ash/wm/desks/zero_state_button.h"
#include "ash/wm/overview/overview_grid.h"
#include "base/memory/raw_ptr.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace ash {

class DeskBarHoverObserver;

// Base class for desk bar views, including desk bar view within overview and
// desk bar view for the desk button.
class ASH_EXPORT DeskBarViewBase : public views::View,
                                   public DesksController::Observer {
 public:
  METADATA_HEADER(DeskBarViewBase);

  enum class Type {
    kOverview,
    kDeskButton,
  };

  enum class State {
    kZero,
    kExpanded,
  };

  enum class LibraryUiVisibility {
    // Library UI visibility is yet to checked or needs an update. The bar will
    // check all prerequisites and the desk model.
    kToBeChecked,

    // Library UI should be visible.
    kVisible,

    // Library UI should be hidden.
    kHidden,
  };

  DeskBarViewBase(const DeskBarViewBase&) = delete;
  DeskBarViewBase& operator=(const DeskBarViewBase&) = delete;

  // Return the preferred height of the desk bar that exists on `root` with
  // `state`.
  static int GetPreferredBarHeight(aura::Window* root, Type type, State state);

  // Return the preferred state for the desk bar given `type`.
  static State GetPerferredState(Type type);

  // Create and returns the widget that contains the desk bar view of `type`.
  // The returned widget has no contents view yet, and hasn't been shown yet.
  static std::unique_ptr<views::Widget>
  CreateDeskWidget(aura::Window* root, const gfx::Rect& bounds, Type type);

  Type type() const { return type_; }

  State state() const { return state_; }

  aura::Window* root() const { return root_; }

  bool pause_layout() const { return pause_layout_; }
  void set_pause_layout(bool value) { pause_layout_ = value; }

  const gfx::Point& last_dragged_item_screen_location() const {
    return last_dragged_item_screen_location_;
  }

  bool dragged_item_over_bar() const { return dragged_item_over_bar_; }

  OverviewGrid* overview_grid() const { return overview_grid_.get(); }

  const std::vector<DeskMiniView*>& mini_views() const { return mini_views_; }

  views::View* background_view() { return background_view_; }

  const views::View* scroll_view_contents() const {
    return scroll_view_contents_;
  }

  ZeroStateDefaultDeskButton* zero_state_default_desk_button() const {
    return zero_state_default_desk_button_;
  }

  ZeroStateIconButton* zero_state_new_desk_button() const {
    return zero_state_new_desk_button_;
  }

  ExpandedDesksBarButton* expanded_state_new_desk_button() const {
    return expanded_state_new_desk_button_;
  }

  ZeroStateIconButton* zero_state_library_button() const {
    return zero_state_library_button_;
  }

  ExpandedDesksBarButton* expanded_state_library_button() const {
    return expanded_state_library_button_;
  }

  CrOSNextDefaultDeskButton* default_desk_button() {
    return default_desk_button_;
  }
  const CrOSNextDefaultDeskButton* default_desk_button() const {
    return default_desk_button_;
  }

  CrOSNextDeskIconButton* new_desk_button() { return new_desk_button_; }
  const CrOSNextDeskIconButton* new_desk_button() const {
    return new_desk_button_;
  }

  CrOSNextDeskIconButton* library_button() { return library_button_; }
  const CrOSNextDeskIconButton* library_button() const {
    return library_button_;
  }

  views::Label* new_desk_button_label() { return new_desk_button_label_; }
  const views::Label* new_desk_button_label() const {
    return new_desk_button_label_;
  }

  views::Label* library_button_label() { return library_button_label_; }
  const views::Label* library_button_label() const {
    return library_button_label_;
  }

  void set_library_ui_visibility(LibraryUiVisibility library_ui_visibility) {
    library_ui_visibility_ = library_ui_visibility;
  }

  // Sets the animation abort handle. Please note, it will abort the existing
  // animation first (if there is one) when a new one comes.
  void set_animation_abort_handle(
      std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle) {
    animation_abort_handle_ = std::move(animation_abort_handle);
  }

  // views::View:
  void Layout() override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Initialize and create mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  void Init();

  // Return true if it is currently in zero state.
  bool IsZeroState() const;

  // If a desk is in a drag & drop cycle.
  bool IsDraggingDesk() const;

  // Return true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

  // If the focused `view` is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen. Please note, `view`
  // must be a child of `scroll_view_contents_`.
  void ScrollToShowViewIfNecessary(const views::View* view);

  // Return the mini_view associated with `desk` or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Get the index of a desk mini view in the `mini_views`.
  int GetMiniViewIndex(const DeskMiniView* mini_view) const;

  void OnNewDeskButtonPressed(
      DesksCreationRemovalSource desks_creation_removal_source);

  // Called when the saved desk library is hidden. Transitions the desk bar
  // view to zero state if necessary.
  void OnSavedDeskLibraryHidden();

  // Bring focus to the name view of the desk with `desk_index`.
  void NudgeDeskName(int desk_index);

  // If in expanded state, updates the border color of the
  // `expanded_state_library_button_` and the active desk's mini view
  // after the saved desk library has been shown. If not in expanded state,
  // updates the background color of the `zero_state_library_button_`
  // and the `zero_state_default_desk_button_`.
  void UpdateButtonsForSavedDeskGrid();

  // Update the visibility of the two buttons inside the zero state desk bar
  // and the `ExpandedDesksBarButton` on the desk bar's state.
  void UpdateDeskButtonsVisibility();

  // Udate the visibility of the `default_desk_button_` on the desk bar's
  // state.
  // TODO(b/291622042): Remove `UpdateDeskButtonsVisibility`, replace it with
  // this function, and rename this function by removing the suffix `CrOSNext`.
  void UpdateDeskButtonsVisibilityCrOSNext();

  // Update the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled, the user has any saved desks and the
  // state of the desk bar.
  void UpdateLibraryButtonVisibility();

  // Update the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled and the user has any saved desks.
  // TODO(b/291622042): Remove `UpdateLibraryButtonVisibility`, replace it with
  // this function, and rename this function by removing the suffix `CrOSNext`.
  void UpdateLibraryButtonVisibilityCrOSNext();

  // Called to update state of `button` and apply the scale animation to the
  // button. For the new desk button, this is called when the make the new desk
  // button a drop target for the window being dragged or at the end of the
  // drag. For the library button, this is called when the library is clicked at
  // the expanded state. Please note this will only be used to switch the states
  // of the `button` between the expanded and active.
  void UpdateDeskIconButtonState(CrOSNextDeskIconButton* button,
                                 CrOSNextDeskIconButton::State target_state);

  // Update the visibility state of the close buttons on all the mini_views as
  // a result of mouse and gesture events.
  void OnHoverStateMayHaveChanged();
  void OnGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Indicate if it should show the library UI in the bar. This will only query
  // the desk model when needed.
  bool ShouldShowLibraryUi();

  // Called when an item is being dragged in overview mode to update whether it
  // is currently intersecting with this view, and the `screen_location` of the
  // current drag position.
  void SetDragDetails(const gfx::Point& screen_location,
                      bool dragged_item_over_bar);

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
  // Handle the click event from a desk preview.
  void HandleClickEvent(DeskMiniView* mini_view);

  // Fires when `desk_activation_timer_` is over.
  void OnActivateDeskTimer(const base::Uuid& uuid);

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
  // If the desk is dropped by user (`end_by_user` = true), scale down and snap
  // back the drag proxy. Otherwise, directly finalize the drag & drop. Note
  // that when we want to end the current drag immediately, if the drag is
  // initialized but did not start, `FinalizeDragDesk` should be use; if the
  // drag started, `EndDragDesk` should be used with `end_by_user` = false.
  void EndDragDesk(DeskMiniView* mini_view, bool end_by_user);
  // Reset the drag view and the drag proxy.
  void FinalizeDragDesk();

  // DesksController::Observer:
  void OnDeskAdded(const Desk* desk, bool from_undo) override;
  void OnDeskRemoved(const Desk* desk) override;
  void OnDeskReordered(int old_index, int new_index) override;
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;
  void OnDeskNameChanged(const Desk* desk,
                         const std::u16string& new_name) override;

  // This is used for the initialization, the expansion, or just the update of
  // child components.
  // Given input parameter values of {`initializing_bar_view`,
  // `expanding_bar_view`}, this does different things as following.
  //    1. {false, false}
  //      When a desk is added from the expanded state, the bar remains
  //      expanded.
  //    2. {true, false}
  //      When the bar is being initialized, the bar will switch to either the
  //      zero state or the expanded state.
  //    3. {false, true}
  //      When the bar is being expanded, the bar will switch to the expanded
  //      state. This is when a desk is added from the zero state, or by the
  //      interaction of bar UI, such as clicking the default desk button,
  //      dragging overview item over new desk button, clicking the library
  //      button, etc.
  //    4. {true, true}
  //      Not a valid input.
  // TODO(b/277969403): Improve and simplify this overloaded function by moving
  // logic to `SwitchToZeroState` and `SwitchToExpandedState`.
  void UpdateNewMiniViews(bool initializing_bar_view, bool expanding_bar_view);

  // Animate the bar from the expanded state to the zero state. It refreshes
  // the bounds of the desk bar widget, and also updates child UI components,
  // including desk mini views, the new desk button, and the library button.
  void SwitchToZeroState();

  // Animate the bar from the zero state to the expanded state.
  void SwitchToExpandedState();

  // Triggered when the bar UI update is done. This is triggered when the bar is
  // done with its animation or when `desk_activation_timer_` fires.
  void OnUiUpdateDone();

  // Gets full available bounds for the desk bar widget.
  virtual gfx::Rect GetAvailableBounds() const = 0;

  // Updates bar widget and bar view bounds as preferred. This is needed for
  // dynamic width for the bar.
  virtual void UpdateBarBounds();

 protected:
  friend class DeskBarScrollViewLayout;
  friend class DesksTestApi;

  DeskBarViewBase(aura::Window* root, Type type);
  ~DeskBarViewBase() override;

  // Return the X offset of the first mini_view on the left (if there's one),
  // or the X offset of this view's center point when there are no mini_views.
  // This offset is used to calculate the amount by which the mini_views should
  // be moved when performing the mini_view creation or deletion animations.
  int GetFirstMiniViewXOffset() const;

  // Returns the descendant views of the desk bar which animate on desk addition
  // or removal, mapped to their current X screen coordinates.
  base::flat_map<views::View*, int> GetAnimatableViewsCurrentXMap() const;

  // Determine the new index of the dragged desk at the position of
  // `location_in_screen`.
  int DetermineMoveIndex(int location_in_screen) const;

  // Update the visibility of `left_scroll_button_` and `right_scroll_button_`.
  // Show `left_scroll_button_` if there are contents outside of the left edge
  // of the `scroll_view_`, the same for `right_scroll_button_` based on the
  // right side of the `scroll_view_`.
  void UpdateScrollButtonsVisibility();

  // We will show a fade in gradient besides `left_scroll_button_` and a fade
  // out gradient besides `right_scroll_button_`. Show the gradient only when
  // the corresponding scroll button is visible.
  void UpdateGradientMask();

  // Scroll the desk bar to the previous or next page. The page size is the
  // width of the scroll view, the contents that are outside of the scroll view
  // will be clipped and can not be seen.
  void ScrollToPreviousPage();
  void ScrollToNextPage();

  // Get the adjusted scroll position based on `position` to make sure no desk
  // preview is cropped at the start position of the scrollable bar.
  int GetAdjustedUncroppedScrollPosition(int position) const;

  void OnLibraryButtonPressed();

  // This function cycles through `mini_views_` and updates the tooltip for each
  // mini view's combine desks button.
  void MaybeUpdateCombineDesksTooltips();

  // Scrollview callbacks.
  void OnContentsScrolled();
  void OnContentsScrollEnded();

  // If drag a desk over a scroll button (i.e., the desk intersects the button),
  // scroll the desk bar. If the desk is dropped or leaves the button, end
  // scroll. Return true if the scroll is triggered. Return false if the scroll
  // is ended.
  bool MaybeScrollByDraggedDesk();

  const Type type_ = Type::kOverview;

  State state_ = State::kZero;

  // True if it needs to hold `Layout` until the bounds animation is completed.
  // `Layout` is expensive and will be called on bounds changes, which means it
  // will be called lots of times during the bounds changes animation. This is
  // done to eliminate the unnecessary `Layout` calls during the animation.
  bool pause_layout_ = false;

  // Mini view whose preview is being dragged.
  raw_ptr<DeskMiniView, ExperimentalAsh> drag_view_ = nullptr;

  // The screen location of the most recent drag position. This value is valid
  // only when the below `dragged_item_over_bar_` is true.
  gfx::Point last_dragged_item_screen_location_;

  // True when the drag location of the overview item is intersecting with this
  // view.
  bool dragged_item_over_bar_ = false;

  // This controls whether or not to show the library UI, e.g. the library
  // button.
  LibraryUiVisibility library_ui_visibility_ =
      LibraryUiVisibility::kToBeChecked;

  // The `OverviewGrid` that contains this object if this is a `Type::kOverview`
  // bar, nullptr otherwise.
  base::WeakPtr<OverviewGrid> overview_grid_;

  // The views representing desks mini_views. They're owned by views hierarchy.
  std::vector<DeskMiniView*> mini_views_;

  // The view representing the desk bar background view. It's owned by views
  // hierarchy. It exists only in the shelf desk bar as it's needed for
  // animation.
  raw_ptr<views::View> background_view_ = nullptr;

  // Put the contents in a `ScrollView` to support scrollable desks.
  raw_ptr<views::ScrollView, ExperimentalAsh> scroll_view_ = nullptr;

  // Contents of `scroll_view_`, which includes `mini_views_`,
  // `expanded_state_new_desk_button_` and optionally
  // `expanded_state_library_button_` currently.
  raw_ptr<views::View, ExperimentalAsh> scroll_view_contents_ = nullptr;

  // Default desk button and new desk buttons.
  raw_ptr<ZeroStateDefaultDeskButton, ExperimentalAsh>
      zero_state_default_desk_button_ = nullptr;
  raw_ptr<ZeroStateIconButton, ExperimentalAsh> zero_state_new_desk_button_ =
      nullptr;
  raw_ptr<ExpandedDesksBarButton, ExperimentalAsh>
      expanded_state_new_desk_button_ = nullptr;

  // Buttons to show the saved desk grid.
  raw_ptr<ZeroStateIconButton, ExperimentalAsh> zero_state_library_button_ =
      nullptr;
  raw_ptr<ExpandedDesksBarButton, ExperimentalAsh>
      expanded_state_library_button_ = nullptr;

  // Buttons for the CrOS Next updated UI. They're added behind the feature flag
  // Jellyroll.
  // TODO(b/291622042): After CrOS Next is launched, replace
  // `zero_state_default_desk_button_`, `zero_state_default_desk_button_`,
  // `expanded_state_new_desk_button_`, `zero_state_library_button_` and
  // `expanded_state_library_button_` with the buttons below.
  raw_ptr<CrOSNextDefaultDeskButton, ExperimentalAsh> default_desk_button_ =
      nullptr;
  raw_ptr<CrOSNextDeskIconButton, ExperimentalAsh> new_desk_button_ = nullptr;
  raw_ptr<CrOSNextDeskIconButton, ExperimentalAsh> library_button_ = nullptr;

  // Labels to be shown under the desk icon buttons when they're at the active
  // state.
  raw_ptr<views::Label, ExperimentalAsh> new_desk_button_label_ = nullptr;
  raw_ptr<views::Label, ExperimentalAsh> library_button_label_ = nullptr;

  // Scroll arrow buttons.
  raw_ptr<ScrollArrowButton, ExperimentalAsh> left_scroll_button_ = nullptr;
  raw_ptr<ScrollArrowButton, ExperimentalAsh> right_scroll_button_ = nullptr;

  // Observe mouse events on the desk bar widget and updates the states of the
  // mini_views accordingly.
  std::unique_ptr<DeskBarHoverObserver> hover_observer_;

  // Drag proxy for the dragged desk.
  std::unique_ptr<DeskDragProxy> drag_proxy_;

  // ScrollView callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;

  // A timer to wait on desk activation before desk bar animation is finished.
  base::OneShotTimer desk_activation_timer_;

  raw_ptr<aura::Window> root_;

  std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle_;

  // Test closure that runs after the UI has been updated asynchronously.
  base::OnceClosure on_update_ui_closure_for_testing_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
