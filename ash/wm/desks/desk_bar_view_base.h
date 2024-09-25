// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
#define ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/desks/default_desk_button.h"
#include "ash/wm/desks/desk_drag_proxy.h"
#include "ash/wm/desks/desk_icon_button.h"
#include "ash/wm/desks/desk_mini_view.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/scroll_arrow_button.h"
#include "base/memory/raw_ptr.h"
#include "base/scoped_observation.h"
#include "ui/aura/window_occlusion_tracker.h"
#include "ui/base/metadata/metadata_header_macros.h"
#include "ui/events/event.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"
#include "ui/views/widget/widget.h"

namespace gfx {
class Rect;
}  // namespace gfx

namespace ash {

class DeskBarHoverObserver;
class OverviewGrid;
class WindowOcclusionCalculator;

// Base class for desk bar views, including desk bar view within overview and
// desk bar view for the desk button.
class ASH_EXPORT DeskBarViewBase : public views::View,
                                   public DesksController::Observer {
  METADATA_HEADER(DeskBarViewBase, views::View)

 public:
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
  static State GetPreferredState(Type type);

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

  const std::vector<raw_ptr<DeskMiniView, VectorExperimental>>& mini_views()
      const {
    return mini_views_;
  }

  views::View* background_view() { return background_view_; }

  DefaultDeskButton* default_desk_button() { return default_desk_button_; }
  const DefaultDeskButton* default_desk_button() const {
    return default_desk_button_;
  }

  DeskIconButton* new_desk_button() { return new_desk_button_; }
  const DeskIconButton* new_desk_button() const { return new_desk_button_; }

  // May return null. See comments above `GetOrCreateLibraryButton()`.
  DeskIconButton* library_button() { return library_button_; }
  const DeskIconButton* library_button() const { return library_button_; }
  views::Label* library_button_label() { return library_button_label_; }
  const views::Label* library_button_label() const {
    return library_button_label_;
  }

  views::Label* new_desk_button_label() { return new_desk_button_label_; }
  const views::Label* new_desk_button_label() const {
    return new_desk_button_label_;
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
  void Layout(PassKey) override;
  bool OnMousePressed(const ui::MouseEvent& event) override;
  void OnGestureEvent(ui::GestureEvent* event) override;

  // Initialize and create mini_views for any pre-existing desks, before the
  // bar was created. `desk_bar_widget_window` is desk bar `Widget`'s
  // corresponding "native window".
  void Init(aura::Window* desk_bar_widget_window);

  // Return true if it is currently in zero state.
  bool IsZeroState() const;

  // If a desk is in a drag & drop cycle.
  bool IsDraggingDesk() const;

  // Return true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

  // If the focused `view` is outside of the scroll view's visible bounds,
  // scrolls the bar to make sure it can always be seen. Please note, `view`
  // must be a child of `contents_view_`.
  void ScrollToShowViewIfNecessary(const views::View* view);

  // Return the mini_view associated with `desk` or nullptr if no mini_view
  // has been created for it yet.
  DeskMiniView* FindMiniViewForDesk(const Desk* desk) const;

  // Get the index of a desk mini view in the `mini_views`.
  int GetMiniViewIndex(const DeskMiniView* mini_view) const;

  void OnNewDeskButtonPressed(
      DesksCreationRemovalSource desks_creation_removal_source);

  // Bring focus to the name view of the desk with `desk_index`.
  void NudgeDeskName(int desk_index);

  // If in expanded state, updates the border color of the `library_button_` and
  // the active desk's mini view`.
  void UpdateButtonsForSavedDeskGrid();

  // Updates the visibility of all buttons in the desk bar and schedules a
  // layout of the desk bar if any button's visibility changes.
  void UpdateDeskButtonsVisibility();

  // Update the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled and the user has any saved desks.
  void UpdateLibraryButtonVisibility();

  // Updates visibility of the label under the new desk button to
  // `new_visibility`. If `layout_if_changed` is true and the label's visibility
  // changes, the desk bar get asynchronously laid out after this call.
  void UpdateNewDeskButtonLabelVisibility(bool new_visibility,
                                          bool layout_if_changed);

  // Called to update state of `button` and apply the scale animation to the
  // button. For the new desk button, this is called when the make the new desk
  // button a drop target for the window being dragged or at the end of the
  // drag. For the library button, this is called when the library is clicked at
  // the expanded state. Please note this will only be used to switch the states
  // of the `button` between the expanded and active.
  void UpdateDeskIconButtonState(DeskIconButton* button,
                                 DeskIconButton::State target_state);

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

  // Animate the bar from the zero state to the expanded state.
  void SwitchToExpandedState();

  // Triggered when the bar UI update is done. This is triggered when the bar is
  // done with its animation or when `desk_activation_timer_` fires.
  void OnUiUpdateDone();

  // Accessors for UI elements that are lazily constructed for performance
  // reasons. Creates them if they don't exist. Only use if they should be
  // visible.
  DeskIconButton& GetOrCreateLibraryButton();
  views::Label& GetOrCreateNewDeskButtonLabel();

  // Gets full available bounds for the desk bar widget.
  virtual gfx::Rect GetAvailableBounds() const = 0;

  // Updates bar widget and bar view bounds as preferred. This is needed for
  // dynamic width for the bar.
  virtual void UpdateBarBounds();

 protected:
  friend class DeskBarScrollViewLayout;
  friend class DesksTestApi;
  class AddDeskAnimation;
  class DeskIconButtonScaleAnimation;
  class LibraryButtonVisibilityAnimation;
  class NewDeskButtonPressedScroll;
  class PostLayoutOperation;
  class RemoveDeskAnimation;
  class ReorderDeskAnimation;
  class ScrollForActiveMiniView;

  DeskBarViewBase(
      aura::Window* root,
      Type type,
      base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator);
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
  // mini view's desk action buttons.
  void MaybeUpdateDeskActionButtonTooltips();

  // Initializes `scroll_view_` if `IsScrollingRequired()`. No-op if
  // `scroll_view_` is already initialized. Should be called any time the width
  // of the desk bar may have increased, as that may necessitate scrolling.
  void InitScrollingIfRequired();
  // Initializes `scroll_view_` and re-parents `contents_view_` if it's
  // set.
  void InitScrolling();

  // Whether scrolling is an actual possibility (i.e. the width of the
  // `contents_view_` is close to, or exceeds the width of the screen). In
  // practice, this happens when the user has many desks, which field metrics
  // suggest is very rare.
  bool IsScrollingRequired() const;

  // Whether `scroll_view_` and all other scrolling UI elements are initialized.
  bool IsScrollingInitialized() const;

  // Parent view that holds all of the contents. Either the `scroll_view_` or
  // the `contents_view_`, depending on whether scrolling is active.
  views::View& GetTopLevelViewWithContents();

  // Scrollview callbacks.
  void OnContentsScrolled();
  void OnContentsScrollEnded();

  // If drag a desk over a scroll button (i.e., the desk intersects the button),
  // scroll the desk bar. If the desk is dropped or leaves the button, end
  // scroll. Return true if the scroll is triggered. Return false if the scroll
  // is ended.
  bool MaybeScrollByDraggedDesk();

  // Maybe refreshes `overview_grid_` bounds on desk bar `state_` changed.
  void MaybeRefreshOverviewGridBounds();

  // Records UMA histograms on desk profile adoption.
  void RecordDeskProfileAdoption();

  const Type type_ = Type::kOverview;

  State state_ = State::kZero;

  // True if it needs to hold `Layout` until the bounds animation is completed.
  // `Layout` is expensive and will be called on bounds changes, which means it
  // will be called lots of times during the bounds changes animation. This is
  // done to eliminate the unnecessary `Layout` calls during the animation.
  bool pause_layout_ = false;

  // Mini view whose preview is being dragged.
  raw_ptr<DeskMiniView> drag_view_ = nullptr;

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
  std::vector<raw_ptr<DeskMiniView, VectorExperimental>> mini_views_;

  // The view representing the desk bar background view. It's owned by views
  // hierarchy. It exists only in the shelf desk bar as it's needed for
  // animation.
  raw_ptr<views::View> background_view_ = nullptr;

  // Put the contents in a `ScrollView` to support scrollable desks. Due to
  // `ScrollView`'s added latency even when scrolling is a non-factor, this is
  // only initialized if `IsScrollingRequired()` is true. If that's not the
  // case, `contents_view_` is a direct child of this view.
  raw_ptr<views::ScrollView> scroll_view_ = nullptr;

  // Parent of the desk bar's primary contents (mainly mini views and buttons).
  // Set as contents of `scroll_view_` if scrolling is enabled, otherwise a
  // direct child of `DeskBarViewBase`. Always non-null.
  raw_ptr<views::View> contents_view_ = nullptr;

  // The default desk button, the new desk button and the library button.
  raw_ptr<DefaultDeskButton> default_desk_button_ = nullptr;
  raw_ptr<DeskIconButton> new_desk_button_ = nullptr;
  raw_ptr<DeskIconButton> library_button_ = nullptr;

  // Labels to be shown under the desk icon buttons when they're at the active
  // state.
  raw_ptr<views::Label> new_desk_button_label_ = nullptr;
  raw_ptr<views::Label> library_button_label_ = nullptr;

  // Scroll arrow buttons.
  raw_ptr<ScrollArrowButton> left_scroll_button_ = nullptr;
  raw_ptr<ScrollArrowButton> right_scroll_button_ = nullptr;

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

  const raw_ptr<aura::Window> root_;

  std::unique_ptr<views::AnimationAbortHandle> animation_abort_handle_;

  // Test closure that runs after the UI has been updated asynchronously.
  base::OnceClosure on_update_ui_closure_for_testing_;

  const base::WeakPtr<WindowOcclusionCalculator> window_occlusion_calculator_;

  const base::RepeatingClosure desk_icon_button_state_changed_cb_;

  // Ordered list of operations to run after the next `Layout()` call ends.
  // This is a short-lived "to-do" list of things that require a layout to
  // complete first before they can be run. The list is cleared after the layout
  // completes. In practice, there is usually 1 element in this list.
  std::vector<std::unique_ptr<PostLayoutOperation>>
      pending_post_layout_operations_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_DESK_BAR_VIEW_BASE_H_
