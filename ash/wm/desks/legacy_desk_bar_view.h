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
#include "base/memory/raw_ptr.h"
#include "ui/views/controls/scroll_view.h"
#include "ui/views/view.h"

namespace ash {

class DesksBarScrollViewLayout;
class DeskBarHoverObserver;
class DeskDragProxy;
class DeskMiniView;
class OverviewGrid;

// A bar that resides at the top portion of the overview, which contains desk
// mini views, the new desk button, the library button, and the scroll arrow
// buttons.
class ASH_EXPORT LegacyDeskBarView : public DeskBarViewBase {
 public:
  explicit LegacyDeskBarView(OverviewGrid* overview_grid);

  LegacyDeskBarView(const LegacyDeskBarView&) = delete;
  LegacyDeskBarView& operator=(const LegacyDeskBarView&) = delete;

  ~LegacyDeskBarView() override;

  void Init() override;

  // Updates the visibility state of the close buttons on all the mini_views as
  // a result of mouse and gesture events.
  void OnHoverStateMayHaveChanged();
  void OnGestureTap(const gfx::Rect& screen_rect, bool is_long_gesture);

  // Called when an item is being dragged in overview mode to update whether it
  // is currently intersecting with this view, and the |screen_location| of the
  // current drag position.
  // TODO(b/278946648): Migrate drag related stuff to base class.
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

  // views::View:
  const char* GetClassName() const override;

  // DesksController::Observer:
  // TODO(b/278945929): Migrate the following functions to base class.
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

  // Animates the bar from the expanded state to the zero state. It refreshes
  // the bounds of the desk bar widget, and also updates child UI components,
  // including desk mini views, the new desk button, and the library button.
  void SwitchToZeroState() override;

  // Animates the bar from the zero state to the expanded state.
  void SwitchToExpandedState() override;

  void UpdateDeskIconButtonState(
      CrOSNextDeskIconButton* button,
      CrOSNextDeskIconButton::State target_state) override;

 private:
  friend class DesksBarScrollViewLayout;
  friend class DesksTestApi;

  // If drag a desk over a scroll button (i.e., the desk intersects the button),
  // scroll the desk bar. If the desk is dropped or leaves the button, end
  // scroll. Return true if the scroll is triggered. Return false if the scroll
  // is ended.
  bool MaybeScrollByDraggedDesk();

  // Observes mouse events on the desk bar widget and updates the states of the
  // mini_views accordingly.
  std::unique_ptr<DeskBarHoverObserver> hover_observer_;

  // Drag proxy for the dragged desk.
  std::unique_ptr<DeskDragProxy> drag_proxy_;
};

}  // namespace ash

#endif  // ASH_WM_DESKS_LEGACY_DESK_BAR_VIEW_H_
