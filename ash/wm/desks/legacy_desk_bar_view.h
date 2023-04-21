// Copyright 2019 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_
#define ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_

#include <memory>
#include <vector>

#include "ash/ash_export.h"
#include "ash/wm/desks/cros_next_default_desk_button.h"
#include "ash/wm/desks/cros_next_desk_icon_button.h"
#include "ash/wm/desks/desk_bar_view_base.h"
#include "ash/wm/desks/desks_controller.h"
#include "ash/wm/desks/templates/saved_desk_metrics_util.h"
#include "base/callback_list.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ash {

class CrOSNextDefaultDeskButton;
class CrOSNextDeskIconButton;
class DesksBarScrollViewLayout;
class DeskBarHoverObserver;
class DeskDragProxy;
class DeskMiniView;
class ExpandedDesksBarButton;
class OverviewGrid;
class ScrollArrowButton;
class ZeroStateDefaultDeskButton;
class ZeroStateIconButton;

// A bar that resides at the top portion of the overview, which contains desk
// mini views, the new desk button, the library button, and the scroll arrow
// buttons.
class ASH_EXPORT LegacyDeskBarView : public DeskBarViewBase {
 public:
  explicit LegacyDeskBarView(OverviewGrid* overview_grid);

  LegacyDeskBarView(const LegacyDeskBarView&) = delete;
  LegacyDeskBarView& operator=(const LegacyDeskBarView&) = delete;

  ~LegacyDeskBarView() override;

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

  // Initializes and creates mini_views for any pre-existing desks, before the
  // bar was created. This should only be called after this view has been added
  // to a widget, as it needs to call `GetWidget()` when it's performing a
  // layout.
  void Init();

  // Returns true if a desk name is being modified using its mini view's
  // DeskNameView on this bar.
  bool IsDeskNameBeingModified() const;

  // Updates the visibility state of the close buttons on all the mini_views as
  // a result of mouse and gesture events.
  void OnHoverStateMayHaveChanged();
  void OnGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Called when an item is being dragged in overview mode to update whether it
  // is currently intersecting with this view, and the |screen_location| of the
  // current drag position.
  void SetDragDetails(const gfx::Point& screen_location,
                      bool dragged_item_over_bar);

  // Handle the mouse press event from a desk preview.
  void HandlePressEvent(DeskMiniView* mini_view,
                        const ui::LocatedEvent& event) override;
  // Handle the gesture long press event from a desk preview.
  void HandleLongPressEvent(DeskMiniView* mini_view,
                            const ui::LocatedEvent& event) override;
  // Handle the drag event from a desk preview.
  void HandleDragEvent(DeskMiniView* mini_view,
                       const ui::LocatedEvent& event) override;
  // Handle the release event from a desk preview. Return true if a drag event
  // is ended.
  bool HandleReleaseEvent(DeskMiniView* mini_view,
                          const ui::LocatedEvent& event) override;

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

  // Called when the saved desk library is hidden. Transitions the desk bar
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
  void UpdateNewMiniViews(bool initializing_bar_view,
                          bool expanding_bar_view) override;

  void OnNewDeskButtonPressed(
      DesksCreationRemovalSource desks_creation_removal_source);

  // If in expanded state, updates the border color of the
  // `expanded_state_library_button_` and the active desk's mini view
  // after the saved desk library has been shown. If not in expanded state,
  // updates the background color of the `zero_state_library_button_`
  // and the `zero_state_default_desk_button_`.
  void UpdateButtonsForSavedDeskGrid();

  // Updates the visibility of the two buttons inside the zero state desk bar
  // and the `ExpandedDesksBarButton` on the desk bar's state.
  void UpdateDeskButtonsVisibility() override;

  // Udates the visibility of the `default_desk_button_` on the desk bar's
  // state.
  // TODO(conniekxu): Remove `UpdateDeskButtonsVisibility`, replace it with this
  // function, and rename this function by removing the prefix CrOSNext.
  void UpdateDeskButtonsVisibilityCrOSNext() override;

  // Updates the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled, the user has any saved desks and the
  // state of the desk bar.
  void UpdateLibraryButtonVisibility();

  // Updates the visibility of the saved desk library button based on whether
  // the saved desk feature is enabled and the user has any saved desks.
  // TODO(conniekxu): Remove `UpdateLibraryButtonVisibility`, replace it with
  // this function, and rename this function by removing the prefix CrOSNext.
  void UpdateLibraryButtonVisibilityCrOSNext() override;

  // Animates the bar from the expanded state to the zero state. It refreshes
  // the bounds of the desk bar widget, and also updates child UI components,
  // including desk mini views, the new desk button, and the library button.
  void SwitchToZeroState();

  // Animates the bar from the zero state to the expanded state.
  void SwitchToExpandedState() override;

  // Bring focus to the name view of the desk with `desk_index`.
  void NudgeDeskName(int desk_index) override;

  // Called to update state of `button` and apply the scale animation to the
  // button. For the new desk button, this is called when the make the new desk
  // button a drop target for the window being dragged or at the end of the
  // drag. For the library button, this is called when the library is clicked at
  // the expanded state. Please note this will only be used to switch the states
  // of the `button` between the expanded and active.
  void UpdateDeskIconButtonState(CrOSNextDeskIconButton* button,
                                 CrOSNextDeskIconButton::State target_state);

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

  // Updates the visibility of |left_scroll_button_| and |right_scroll_button_|.
  // Show |left_scroll_button_| if there are contents outside of the left edge
  // of the |scroll_view_|, the same for |right_scroll_button_| based on the
  // right side of the |scroll_view_|.
  void UpdateScrollButtonsVisibility() override;

  // We will show a fade in gradient besides |left_scroll_button_| and a fade
  // out gradient besides |right_scroll_button_|. Show the gradient only when
  // the corresponding scroll button is visible.
  void UpdateGradientMask() override;

  // Scrolls the desk bar to the previous or next page. The page size is the
  // width of the scroll view, the contents that are outside of the scroll view
  // will be clipped and can not be seen.
  void ScrollToPreviousPage();
  void ScrollToNextPage();

  // Gets the adjusted scroll position based on |position| to make sure no desk
  // preview is cropped at the start position of the scrollable bar.
  int GetAdjustedUncroppedScrollPosition(int position) const;

  void OnLibraryButtonPressed();

  // This function cycles through `mini_views_` and updates the tooltip for each
  // mini view's combine desks button.
  void MaybeUpdateCombineDesksTooltips();

  // Scrollview callbacks.
  void OnContentsScrolled();
  void OnContentsScrollEnded();

  // Observes mouse events on the desk bar widget and updates the states of the
  // mini_views accordingly.
  std::unique_ptr<DeskBarHoverObserver> hover_observer_;

  ZeroStateDefaultDeskButton* zero_state_default_desk_button_ = nullptr;
  ZeroStateIconButton* zero_state_new_desk_button_ = nullptr;
  ExpandedDesksBarButton* expanded_state_new_desk_button_ = nullptr;

  // Buttons to show the saved desk grid.
  ZeroStateIconButton* zero_state_library_button_ = nullptr;
  ExpandedDesksBarButton* expanded_state_library_button_ = nullptr;

  // Buttons for the CrOS Next updated UI. They're added behind the feature flag
  // Jellyroll.
  // TODO(conniekxu): After CrOS Next is launched, replace
  // `zero_state_default_desk_button_`, `zero_state_default_desk_button_`,
  // `expanded_state_new_desk_button_`, `zero_state_library_button_` and
  // `expanded_state_library_button_` with the buttons below.
  CrOSNextDefaultDeskButton* default_desk_button_ = nullptr;
  CrOSNextDeskIconButton* new_desk_button_ = nullptr;
  CrOSNextDeskIconButton* library_button_ = nullptr;

  // Labels to be shown under the desk icon buttons when they're at the active
  // state.
  views::Label* new_desk_button_label_ = nullptr;
  views::Label* library_button_label_ = nullptr;

  ScrollArrowButton* left_scroll_button_ = nullptr;
  ScrollArrowButton* right_scroll_button_ = nullptr;
  // Drag proxy for the dragged desk.
  std::unique_ptr<DeskDragProxy> drag_proxy_;

  // ScrollView callback subscriptions.
  base::CallbackListSubscription on_contents_scrolled_subscription_;
  base::CallbackListSubscription on_contents_scroll_ended_subscription_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_
