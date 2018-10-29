// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_
#define ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/display/screen_orientation_controller.h"
#include "ash/public/interfaces/split_view.mojom.h"
#include "ash/shell_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_observer.h"
#include "ash/wm/window_state_observer.h"
#include "base/containers/flat_map.h"
#include "base/macros.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "mojo/public/cpp/bindings/binding_set.h"
#include "mojo/public/cpp/bindings/interface_ptr_set.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class Layer;
}  // namespace ui

namespace ash {

class SplitViewControllerTest;
class SplitViewDivider;
class SplitViewWindowSelectorTest;

// The controller for the split view. It snaps a window to left/right side of
// the screen. It also observes the two snapped windows and decides when to exit
// the split view mode.
// TODO(xdai): Make it work for multi-display non mirror environment.
class ASH_EXPORT SplitViewController : public mojom::SplitViewController,
                                       public aura::WindowObserver,
                                       public ash::wm::WindowStateObserver,
                                       public ::wm::ActivationChangeObserver,
                                       public ShellObserver,
                                       public display::DisplayObserver,
                                       public TabletModeObserver,
                                       public AccessibilityObserver {
 public:
  enum State { NO_SNAP, LEFT_SNAPPED, RIGHT_SNAPPED, BOTH_SNAPPED };

  // "LEFT" and "RIGHT" are the snap positions corresponding to "primary
  // landscape" screen orientation. In other screen orientation, we still use
  // "LEFT" and "RIGHT" but it doesn't literally mean left side of the screen or
  // right side of the screen. For example, if the screen orientation is
  // "portait primary", snapping a window to LEFT means snapping it to the
  // top of the screen.
  enum SnapPosition { NONE, LEFT, RIGHT };

  // Why splitview was ended. For now, all reasons will be kNormal except when
  // the home launcher button is pressed or an unsnappable window just got
  // activated.
  enum class EndReason {
    kNormal = 0,
    kHomeLauncherPressed,
    kUnsnappableWindowActivated,
  };

  class Observer {
   public:
    // Called when split view state changed from |previous_state| to |state|.
    virtual void OnSplitViewStateChanged(
        SplitViewController::State previous_state,
        SplitViewController::State state) {}

    // Called when split view divider's position has changed.
    virtual void OnSplitViewDividerPositionChanged() {}
  };

  SplitViewController();
  ~SplitViewController() override;

  // Returns true if split view mode is supported. Currently the split view
  // mode is only supported in tablet mode.
  static bool ShouldAllowSplitView();

  // Binds the mojom::SplitViewController interface to this object.
  void BindRequest(mojom::SplitViewControllerRequest request);

  // Returns true if |window| can be activated and snapped.
  bool CanSnap(aura::Window* window);

  // Returns true if split view mode is active.
  bool IsSplitViewModeActive() const;

  OrientationLockType GetCurrentScreenOrientation() const;

  // Returns true if |screen_orientation_| is a landscape orientation.
  bool IsCurrentScreenOrientationLandscape() const;

  // Returns true if |screen_orientation_| is a primary orientation. Note,
  // |left_window_| should be placed on the left or top side if the screen is
  // primary orientation.
  bool IsCurrentScreenOrientationPrimary() const;

  // Snaps window to left/right. It will try to remove |window| from the
  // overview window grid first before snapping it if |window| is currently
  // showing in the overview window grid.
  void SnapWindow(aura::Window* window, SnapPosition snap_position);

  // Swaps the left and right windows. This will do nothing if one of the
  // windows is not snapped.
  void SwapWindows();

  // Returns the default snapped window. It's the window that remains open until
  // the split mode ends. It's decided by |default_snap_position_|. E.g., If
  // |default_snap_position_| equals LEFT, then the default snapped window is
  // |left_window_|. All the other window will open on the right side.
  aura::Window* GetDefaultSnappedWindow();

  // Gets the window bounds according to the snap state |snap_state| and the
  // divider position |divider_position_|. The returned snapped window bounds
  // are adjusted to its minimum size if the desired bounds are smaller than
  // its minumum bounds. Note: the snapped window bounds can't be pushed
  // outside of the workspace area.
  gfx::Rect GetSnappedWindowBoundsInParent(aura::Window* window,
                                           SnapPosition snap_position);
  gfx::Rect GetSnappedWindowBoundsInScreen(aura::Window* window,
                                           SnapPosition snap_position);
  gfx::Rect GetDisplayWorkAreaBoundsInParent(aura::Window* window) const;
  gfx::Rect GetDisplayWorkAreaBoundsInScreen(aura::Window* window) const;

  // Gets the desired snapped window bounds accoridng to the snap state
  // |snap_state| and the divider pistion |divider_position_|.
  gfx::Rect GetSnappedWindowBoundsInScreenUnadjusted(aura::Window* window,
                                                     SnapPosition snap_postion);

  // Gets the default value of |divider_position_|.
  int GetDefaultDividerPosition(aura::Window* window) const;

  void StartResize(const gfx::Point& location_in_screen);
  void Resize(const gfx::Point& location_in_screen);
  void EndResize(const gfx::Point& location_in_screen);

  // Displays a toast notifying users the application selected for split view is
  // not compatible.
  void ShowAppCannotSnapToast();

  // Ends the split view mode.
  void EndSplitView(EndReason end_reason = EndReason::kNormal);

  // Called when a window (either it's browser window or an app window) start/
  // end being dragged.
  void OnWindowDragStarted(aura::Window* dragged_window);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);

  void AddObserver(Observer* observer);
  void RemoveObserver(Observer* observer);

  void FlushForTesting();

  // mojom::SplitViewObserver:
  void AddObserver(mojom::SplitViewObserverPtr observer) override;

  // aura::WindowObserver:
  void OnWindowDestroyed(aura::Window* window) override;
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;

  // ash::wm::WindowStateObserver:
  void OnPostWindowStateTypeChange(
      ash::wm::WindowState* window_state,
      ash::mojom::WindowStateType old_type) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // ShellObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // TabletModeObserver:
  void OnTabletModeEnding() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;

  aura::Window* left_window() { return left_window_; }
  aura::Window* right_window() { return right_window_; }
  int divider_position() const { return divider_position_; }
  State state() const { return state_; }
  SnapPosition default_snap_position() const { return default_snap_position_; }
  SplitViewDivider* split_view_divider() { return split_view_divider_.get(); }
  bool is_resizing() const { return is_resizing_; }
  EndReason end_reason() const { return end_reason_; }

 private:
  friend class SplitViewControllerTest;
  friend class SplitViewWindowSelectorTest;
  class TabDraggedWindowObserver;

  // Start observing |window|.
  void StartObserving(aura::Window* window);
  // Stop observing the window at associated with |snap_position|. Also updates
  // shadows and sets |left_window_| or |right_window_| to nullptr.
  void StopObserving(SnapPosition snap_position);

  // Update split view state and notify its observer about the change.
  void UpdateSplitViewStateAndNotifyObservers();

  // Notifies observers that the split view divider position has been changed.
  void NotifyDividerPositionChanged();

  // Updates the black scrim layer's bounds and opacity while dragging the
  // divider. The opacity increases as the split divider gets closer to the edge
  // of the screen.
  void UpdateBlackScrim(const gfx::Point& location_in_screen);

  // Updates the bounds for the snapped windows and divider according to the
  // current snap direction.
  void UpdateSnappedWindowsAndDividerBounds();

  // Gets the position where the black scrim should show.
  SnapPosition GetBlackScrimPosition(const gfx::Point& location_in_screen);

  // Updates |divider_position_| according to the current event location during
  // resizing.
  void UpdateDividerPosition(const gfx::Point& location_in_screen);

  // Get the window bounds for left_or_top and right_or_bottom snapped windows.
  // Note the bounds returned by this function doesn't take the snapped windows
  // minimum sizes into account.
  void GetSnappedWindowBoundsInScreenInternal(aura::Window* window,
                                              gfx::Rect* left_or_top_rect,
                                              gfx::Rect* right_or_bottom_rect);

  // Splits the |work_area_rect| by |divider_rect| and outputs the two halves.
  // |left_or_top_rect|, |divider_rect| and |right_or_bottom_rect| should align
  // vertically or horizontally depending on |is_split_vertically|.
  void SplitRect(const gfx::Rect& work_area_rect,
                 const gfx::Rect& divider_rect,
                 const bool is_split_vertically,
                 gfx::Rect* left_or_top_rect,
                 gfx::Rect* right_or_bottom_rect);

  // Finds the closest fix location for |divider_position_| and updates its
  // value.
  void MoveDividerToClosestFixedPosition();

  // Returns true if we should end split view mode after resizing, i.e., the
  // split view divider is near to the edge of the screen.
  bool ShouldEndSplitViewAfterResizing();

  // After resizing, if we should end split view mode, returns the window that
  // needs to be activated. Returns nullptr if there is no such window.
  aura::Window* GetActiveWindowAfterResizingUponExit();

  // Returns the maximum value of the |divider_position_|. It is the width of
  // the current display's work area bounds in landscape orientation, or height
  // of the current display's work area bounds in portrait orientation.
  int GetDividerEndPosition();

  // Called after a to-be-snapped window |window| got snapped. It updates the
  // split view states and notifies observers about the change. It also restore
  // the snapped window's transform if it's not identity and activate it.
  void OnWindowSnapped(aura::Window* window);

  // If there are two snapped windows, closing/minimizing/tab-dragging one of
  // them will open overview window grid on the closed/minimized/tab-dragged
  // window side of the screen. If there is only one snapped windows, closing/
  // minimizing/tab-dragging the sanpped window will end split view mode and
  // adjust the overview window grid bounds if the overview mode is active at
  // that moment.
  void OnSnappedWindowDetached(aura::Window* window);

  // If the desired bounds of the snapped windows bounds |left_or_top_rect| and
  // |right_or_bottom_rect| are smaller than the minimum bounds of the snapped
  // windows, adjust the desired bounds to the minimum bounds. Note the snapped
  // windows can't be pushed out of the work area display area.
  void AdjustSnappedWindowBounds(gfx::Rect* left_or_top_rect,
                                 gfx::Rect* right_or_bottom_rect);

  // Returns the closest position ratio based on |distance| and |length|.
  float FindClosestPositionRatio(float distance, float length);

  // Gets the divider optional position ratios. The divider can always be
  // moved to the positions in |kFixedPositionRatios|. Whether the divider can
  // be moved to |kOneThirdPositionRatio| or |kTwoThirdPositionRatio| depends
  // on the minimum size of current snapped windows.
  void GetDividerOptionalPositionRatios(std::vector<float>* positionRatios);

  // Gets the expected window component depending on current screen orientation
  // for resizing purpose.
  int GetWindowComponentForResize(aura::Window* window);
  // Gets the expected end drag position for |window| depending on current
  // screen orientation and split divider position.
  gfx::Point GetEndDragLocationInScreen(aura::Window* window,
                                        const gfx::Point& location_in_screen);

  // Restores |window| transform to identity transform if applicable.
  void RestoreTransformIfApplicable(aura::Window* window);

  // Activates the snapped window |window| and stacks the other snapped window
  // below |window| so that the two snapped windows are always the top two
  // windows when split view mode is active.
  void ActivateAndStackSnappedWindow(aura::Window* window);

  // During resizing, it's possible that the resizing bounds of the snapped
  // window is smaller than its minimum bounds, in this case we apply a
  // translation to the snapped window to make it visually be placed outside of
  // the workspace area.
  void SetWindowsTransformDuringResizing();
  // Restore the snapped windows transform to identity transform after resizing.
  void RestoreWindowsTransformAfterResizing();

  // Animates to |target_transform| for |window| and its transient descendants.
  // |window| will be applied |start_transform| first and then animate to
  // |target_transform|. Note |start_transform| and |end_transform| are for
  // |window| and need to be adjusted for its transient child windows.
  void SetTransformWithAnimation(aura::Window* window,
                                 const gfx::Transform& start_transform,
                                 const gfx::Transform& target_transform);

  // Removes the window item that contains |window| from the overview window
  // grid if |window| is currently showing in overview window grid. It should be
  // called before trying to snap the window.
  void RemoveWindowFromOverviewIfApplicable(aura::Window* window);

  // Updates the |snapping_window_transformed_bounds_map_| on |window|. It
  // should be called before trying to snap the window.
  void UpdateSnappingWindowTransformedBounds(aura::Window* window);

  // Inserts |window| into overview window grid if overview mode is active. Do
  // nothing if overview mode is inactive at the moment.
  void InsertWindowToOverview(aura::Window* window);

  // Starts/Ends overview mode if the overview mode is inactive/active.
  void StartOverview();
  void EndOverview();

  // Finalizes and cleans up after stopping dragging the divider bar to resize
  // snapped windows.
  void FinishWindowResizing(aura::Window* window);

  // Called by OnWindowDragEnded to do the actual work of finishing the window
  // dragging.
  void EndWindowDragImpl(aura::Window* window,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);

  // Gets the snap position of |window| according to last mouse/gesture event
  // location on |window|. Used when |desired_snap_position_| was NONE but
  // SplitViewController needs to snap the |window| after dragging.
  SplitViewController::SnapPosition GetSnapPosition(
      aura::Window* window,
      const gfx::Point& last_location_in_screen);

  // Bindings for the SplitViewController interface.
  mojo::BindingSet<mojom::SplitViewController> bindings_;

  // The current left/right snapped window.
  aura::Window* left_window_ = nullptr;
  aura::Window* right_window_ = nullptr;

  // Split view divider widget. It's a black bar stretching from one edge of the
  // screen to the other, containing a small white drag bar in the middle. As
  // the user presses on it and drag it to left or right, the left and right
  // window will be resized accordingly.
  std::unique_ptr<SplitViewDivider> split_view_divider_;

  // A black scrim layer that fades in over a window when it’s width drops under
  // 1/3 of the width of the screen, increasing in opacity as the divider gets
  // closer to the edge of the screen.
  std::unique_ptr<ui::Layer> black_scrim_layer_;

  // The window observer that obseves the tab-dragged window.
  std::unique_ptr<TabDraggedWindowObserver> dragged_window_observer_;

  // The distance between the origin of the divider and the origin of the
  // current display's work area in screen coordinates.
  //     |<---     divider_position_    --->|
  //     ----------------------------------------------------------
  //     |                                  | |                    |
  //     |        left_window_              | |   right_window_    |
  //     |                                  | |                    |
  //     ----------------------------------------------------------
  int divider_position_ = -1;

  // The closest position ratio of divider among kFixedPositionRatios,
  // kOneThirdPositionRatio and kTwoThirdPositionRatio based on current
  // |divider_position_|. Used to update |divider_position_| on work area
  // changes.
  float divider_closest_ratio_ = 0.f;

  // The location of the previous mouse/gesture event in screen coordinates.
  gfx::Point previous_event_location_;

  // Current snap state.
  State state_ = NO_SNAP;

  // The default snap position. It's decided by the first snapped window. If the
  // first window was snapped left, then |default_snap_position_| equals LEFT,
  // i.e., all the other windows will open snapped on the right side - and vice
  // versa.
  SnapPosition default_snap_position_ = NONE;

  // The previous orientation of the screen.
  OrientationLockType previous_screen_orientation_ = OrientationLockType::kAny;

  // If the divider is currently being dragging.
  bool is_resizing_ = false;

  // Stores the reason which cause splitview to end.
  EndReason end_reason_ = EndReason::kNormal;

  // The time when splitview starts. Used for metric collection purpose.
  base::Time splitview_start_time_;

  // The map from a to-be-snapped window to its transformed bounds.
  base::flat_map<aura::Window*, gfx::Rect>
      snapping_window_transformed_bounds_map_;

  base::ObserverList<Observer>::Unchecked observers_;
  mojo::InterfacePtrSet<mojom::SplitViewObserver> mojo_observers_;

  DISALLOW_COPY_AND_ASSIGN(SplitViewController);
};

}  // namespace ash

#endif  // ASH_WM_SPLITSVIEW_SPLIT_VIEW_CONTROLLER_H_
