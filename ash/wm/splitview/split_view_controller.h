// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_CONTROLLER_H_

#include <limits>
#include <memory>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/wm_event.h"
#include "base/containers/flat_map.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/wm/public/activation_change_observer.h"

namespace ui {
class Layer;
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {
class OverviewSession;
class SplitViewControllerTest;
class SplitViewDivider;
class SplitViewMetricsController;
class SplitViewObserver;
class SplitViewOverviewSessionTest;

// `SplitViewController` handles how window snapping interacts with overview
// mode and tablet mode. There is an instance for each display. In clamshell
// mode, `SplitViewController` is relevant on displays with a snapped window
// on one side and an overview grid (empty or not) on the other side. In
// tablet mode, `SplitViewController` is relevant on all displays with snapped
// windows.
// TODO(xdai): Make it work for multi-display non mirror environment.
class ASH_EXPORT SplitViewController : public aura::WindowObserver,
                                       public WindowStateObserver,
                                       public ShellObserver,
                                       public OverviewObserver,
                                       public display::DisplayObserver,
                                       public TabletModeObserver,
                                       public AccessibilityObserver,
                                       public ash::KeyboardControllerObserver,
                                       public wm::ActivationChangeObserver {
 public:
  // |LEFT| and |RIGHT| are named for the positions to which they correspond in
  // clamshell mode or primary-landscape-oriented tablet mode. In portrait-
  // oriented tablet mode, we actually snap windows on the top and bottom, but
  // in clamshell mode, although the display orientation may sometimes be
  // portrait, we always snap windows on the left and right (see
  // |IsLayoutHorizontal|). The snap positions are swapped in secondary-oriented
  // tablet mode (see |IsLayoutPrimary|).
  enum class SnapPosition { kNone, kPrimary, kSecondary };

  // Why splitview was ended.
  enum class EndReason {
    kNormal = 0,
    kHomeLauncherPressed,
    kUnsnappableWindowActivated,
    kActiveUserChanged,
    kWindowDragStarted,
    kExitTabletMode,
    // Splitview is being ended due to a change in Virtual Desks, such as
    // switching desks or removing a desk.
    kDesksChange,
    // Splitview is being ended due to the `root_window_` is destroyed and the
    // SplitViewController is being destroyed.
    kRootWindowDestroyed,
  };

  // The behaviors of split view are very different when in tablet mode and in
  // clamshell mode. In tablet mode, split view mode will stay active until the
  // user explicitly ends it (e.g., by pressing home launcher, or long pressing
  // the overview button, or sliding the divider bar to the edge, etc). However,
  // in clamshell mode, there is no divider bar, and split view mode only stays
  // active during overview snapping, i.e., it's only possible that split view
  // is active when overview is active. Once the user has selected two windows
  // to snap to both side of the screen, split view mode is no longer active.
  enum class SplitViewType {
    kTabletType = 0,
    kClamshellType,
  };

  enum class State {
    kNoSnap,
    kPrimarySnapped,
    kSecondarySnapped,
    kBothSnapped,
  };

  // The split view resize behavior in tablet mode. The normal mode resizes
  // windows on drag events. In the fast mode, windows are instead moved. A
  // single drag "session" may involve both modes.
  enum class TabletResizeMode {
    kNormal,
    kFast,
  };

  // Gets the |SplitViewController| for the root window of |window|. |window| is
  // important in clamshell mode. In tablet mode, the working assumption for now
  // is mirror mode (or just one display), and so |window| can be almost any
  // window and it does not matter. For code that only applies to tablet mode,
  // you may simply use the primary root (see |Shell::GetPrimaryRootWindow|).
  // The user actually can go to the display settings while in tablet mode and
  // choose extend; we just are not yet trying to support it really well.
  static SplitViewController* Get(const aura::Window* window);

  // The return values of these two functions together indicate what actual
  // positions correspond to |PRIMARY| and |SECONDARY|:
  // |IsLayoutHorizontal|  |IsLayoutPrimary|    |PRIMARY|           |SECONDARY|
  // --------------------------------------------------------------------------
  // true                  true                   left                 right
  // true                  false                  right                left
  // false                 true                   top                  bottom
  // false                 false                  bottom               top
  // In both clamshell and tablet mode, these functions return values based on
  // display orientation. |window| is used to find the nearest display to check
  // if the display layout is horizontal and is primary or not.
  static bool IsLayoutHorizontal(aura::Window* window);
  static bool IsLayoutHorizontal(const display::Display& display);
  static bool IsLayoutPrimary(aura::Window* window);
  static bool IsLayoutPrimary(const display::Display& display);

  // Returns true if |position| actually signifies a left or top position,
  // according to the return values of |IsLayoutHorizontal| and
  // |IsLayoutPrimary|. Physical position refers to the position of the window
  // on the display that is held upward.
  static bool IsPhysicalLeftOrTop(SnapPosition position, aura::Window* window);
  static bool IsPhysicalLeftOrTop(SnapPosition position,
                                  const display::Display& display);

  explicit SplitViewController(aura::Window* root_window);

  SplitViewController(const SplitViewController&) = delete;
  SplitViewController& operator=(const SplitViewController&) = delete;

  ~SplitViewController() override;

  // Returns true if split view mode is active. Please see SplitViewType above
  // to see the difference between tablet mode and clamshell mode splitview
  // mode.
  bool InSplitViewMode() const;
  bool BothSnapped() const;
  bool InClamshellSplitViewMode() const;
  bool InTabletSplitViewMode() const;

  // Checks the following criteria:
  // 1. Split view mode is supported (see |ShouldAllowSplitView|).
  // 2. |window| can be activated (see |wm::CanActivateWindow|).
  // 3. The |WindowState| of |window| can snap (see |WindowState::CanSnap|).
  // 4. |window|'s minimum size, if any, fits into the left or top with the
  //    default divider position. (If the work area length is odd, then the
  //    right or bottom will be one pixel larger.)
  // See also the |DCHECK|s in |SnapWindow|.
  bool CanSnapWindow(aura::Window* window) const;

  // Snap `window` in the split view at `snap_position`. It will send snap
  // WMEvent to `window` and rely on WindowState to do the actual work to
  // change window state and bounds. Note this function does not guarantee
  // `window` can be snapped in the split view (e.g., an ARC++ window may
  // decide to ignore the state change request), and split view state will only
  // be updated after the window state is changed to the desired snap window
  // state. If `activate_window` is true, `window` will be activated after being
  // snapped in splitview. Please note if `activate_window` is false, it's still
  // possible that `window` will be activated after being snapped, see
  // `to_be_activated_window_` for details. `snap_ratio` may be provided if the
  // window requests a specific snap ratio, i.e. during clamshell <-> tablet
  // transition.
  void SnapWindow(aura::Window* window,
                  SnapPosition snap_position,
                  bool activate_window = false,
                  float snap_ratio = chromeos::kDefaultSnapRatio);

  // This is called by WindowState::State when receiving a snap WMEvent (i.e.,
  // WM_EVENT_SNAP_PRIMARY or WM_EVENT_SNAP_SECONDARY). SplitViewController will
  // decide if this window needs to be snapped in split view.
  void OnWMEvent(aura::Window* window, WMEventType event_type);

  // Attaches the to-be-snapped |window| to split view at |snap_position|. It
  // will try to remove |window| from the overview window grid first if |window|
  // is currently showing in the overview window grid. We'll add a finishing
  // touch to the snap animation of |window| if split view mode is not already
  // active, and if |window| is not minimized and has an non-identity transform.
  void AttachSnappingWindow(aura::Window* window, SnapPosition snap_position);

  // Swaps the left and right windows. This will do nothing if one of the
  // windows is not snapped.
  void SwapWindows();

  // |window| should be |primary_window_| or |secondary_window_|, and this
  // function returns |LEFT| or |RIGHT| accordingly.
  SnapPosition GetPositionOfSnappedWindow(const aura::Window* window) const;

  // |position| should be |LEFT| or |RIGHT|, and this function returns
  // |primary_window_| or |secondary_window_| accordingly.
  aura::Window* GetSnappedWindow(SnapPosition position);

  // Returns the default snapped window. It's the window that remains open until
  // the split mode ends. It's decided by |default_snap_position_|. E.g., If
  // |default_snap_position_| equals LEFT, then the default snapped window is
  // |primary_window_|. All the other window will open on the right side.
  aura::Window* GetDefaultSnappedWindow();

  // Gets snapped bounds based on |snap_position|, the side of the screen to
  // snap to, and |snap_ratio|, the ratio of the screen that the snapped window
  // will occupy, adjusted to accommodate the minimum size of
  // |window_for_minimum_size| if |window_for_minimum_size| is not null.
  gfx::Rect GetSnappedWindowBoundsInParent(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size,
      float snap_ratio);

  // Gets snapped bounds based on |snap_position|, |divider_position_|, and
  // |kDefaultSnapRatio|, adjusted to accommodate the minimum size of
  // |window_for_minimum_size| if |window_for_minimum_size| is not null.
  gfx::Rect GetSnappedWindowBoundsInParent(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size);

  // Gets snapped bounds in screen coordinates based on |snap_position| and
  // |snap_ratio|.
  gfx::Rect GetSnappedWindowBoundsInScreen(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size,
      float snap_ratio);

  // Gets snapped bounds in screen coordinates for |kDefaultSnapRatio|.
  gfx::Rect GetSnappedWindowBoundsInScreen(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size);

  // Returns true if we are resizing with the fast resize
  // mode. `GetSnappedWindowBoundsInScreen()` should then return the windows
  // current bounds in screen coordinates.
  bool ShouldUseWindowBoundsDuringFastResize();

  // Gets the default value of |divider_position_|.
  int GetDefaultDividerPosition() const;

  // Calculates the new divider position to move |divider_position_| to, such
  // that the primary window will occupy |snap_ratio| of the screen, and the
  // secondary window will occupy the rest.
  int GetDividerPosition(SnapPosition snap_position, float snap_ratio) const;

  // Returns true during the divider snap animation.
  bool IsDividerAnimating() const;

  void StartResize(const gfx::Point& location_in_screen);
  void Resize(const gfx::Point& location_in_screen);
  void EndResize(const gfx::Point& location_in_screen);

  // Ends the split view mode.
  void EndSplitView(EndReason end_reason = EndReason::kNormal);

  // Returns true if |window| is a snapped window in splitview.
  bool IsWindowInSplitView(const aura::Window* window) const;

  // This function is only supposed to be called during clamshell <-> tablet
  // transition or multi-user transition, when we need to carry over one/two
  // snapped windows into splitview, we calculate the divider position based on
  // the one or two to-be-snapped windows' bounds so that we can keep the
  // snapped windows' bounds after transition (instead of putting them always
  // on the middle split position).
  void InitDividerPositionForTransition(int divider_position);

  // Returns true if |window| is in a transitinal state which means that
  // |SplitViewController| has already changed its internal snapped state for
  // |window| but the snapped state has not been applied to |window|'s window
  // state yet. The transional state can be happen in some clients (e.g. ARC
  // app) which handle window states asynchronously.
  bool IsWindowInTransitionalState(const aura::Window* window) const;

  // Called when the overview button tray has been long pressed. Enters
  // splitview mode if the active window is snappable. Also enters overview mode
  // if device is not currently in overview mode.
  void OnOverviewButtonTrayLongPressed(const gfx::Point& event_location);

  // Called when a window (either it's browser window or an app window) start/
  // end being dragged.
  void OnWindowDragStarted(aura::Window* dragged_window);
  void OnWindowDragEnded(aura::Window* dragged_window,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);
  void OnWindowDragCanceled();

  // Computes the snap position for a dragged window, based on the last
  // mouse/gesture event location. Called by |EndWindowDragImpl| when
  // desired_snap_position is |NONE| but because split view is already active,
  // the dragged window needs to be snapped anyway.
  SplitViewController::SnapPosition ComputeSnapPosition(
      const gfx::Point& last_location_in_screen);

  // In portrait mode split view, if the virtual keyboard occludes the input
  // field in the bottom window. The bottom window will be pushed up above the
  // virtual keyboard. In this case, we allow window state to set bounds for
  // snapped window.
  bool BoundsChangeIsFromVKAndAllowed(aura::Window* window) const;

  void AddObserver(SplitViewObserver* observer);
  void RemoveObserver(SplitViewObserver* observer);

  // aura::WindowObserver:
  void OnWindowPropertyChanged(aura::Window* window,
                               const void* key,
                               intptr_t old) override;
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowDestroyed(aura::Window* window) override;
  void OnResizeLoopStarted(aura::Window* window) override;
  void OnResizeLoopEnded(aura::Window* window) override;

  // WindowStateObserver:
  void OnPostWindowStateTypeChange(WindowState* window_state,
                                   chromeos::WindowStateType old_type) override;

  // ShellObserver:
  void OnPinnedStateChanged(aura::Window* pinned_window) override;

  // OverviewObserver:
  void OnOverviewModeStarting() override;
  void OnOverviewModeEnding(OverviewSession* overview_session) override;
  void OnOverviewModeEnded() override;

  // display::DisplayObserver:
  void OnDisplayRemoved(const display::Display& old_display) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeStarted() override;
  void OnTabletModeEnding() override;
  void OnTabletModeEnded() override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;
  void OnAccessibilityControllerShutdown() override;

  // KeyboardControllerObserver
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  aura::Window* root_window() const { return root_window_; }
  aura::Window* primary_window() { return primary_window_; }
  aura::Window* secondary_window() { return secondary_window_; }
  int divider_position() const { return divider_position_; }
  State state() const { return state_; }
  SnapPosition default_snap_position() const { return default_snap_position_; }
  SplitViewDivider* split_view_divider() { return split_view_divider_.get(); }
  bool is_resizing() const { return is_resizing_; }
  EndReason end_reason() const { return end_reason_; }
  SplitViewMetricsController* split_view_metrics_controller() {
    return split_view_metrics_controller_.get();
  }
  aura::Window* to_be_activated_window() const {
    return to_be_activated_window_;
  }

 private:
  friend class SplitViewControllerTest;
  friend class SplitViewOverviewSessionTest;
  class TabDraggedWindowObserver;
  class DividerSnapAnimation;
  class AutoSnapController;
  class ToBeSnappedWindowsObserver;

  // Reason that a snapped window is detached from the splitview.
  enum class WindowDetachedReason {
    kWindowMinimized,
    kWindowDestroyed,
    kWindowDragged,
    kWindowFloated,
  };

  // These functions return |primary_window_| and |secondary_window_|, swapped
  // in nonprimary screen orientations. Note that they may return null.
  aura::Window* GetPhysicalLeftOrTopWindow();
  aura::Window* GetPhysicalRightOrBottomWindow();

  // Start observing |window|.
  void StartObserving(aura::Window* window);
  // Stop observing the window at associated with |snap_position|. Also updates
  // shadows and sets |primary_window_| or |secondary_window_| to nullptr.
  void StopObserving(SnapPosition snap_position);

  // Update split view state and notify its observer about the change.
  void UpdateStateAndNotifyObservers();

  // Notifies observers that the split view divider position has been changed.
  void NotifyDividerPositionChanged();

  // Notifies observers that the windows in split view is resized.
  void NotifyWindowResized();

  // Notifies observers that the windows are swappped.
  void NotifyWindowSwapped();

  // Updates the black scrim layer's bounds and opacity while dragging the
  // divider. The opacity increases as the split divider gets closer to the edge
  // of the screen.
  void UpdateBlackScrim(const gfx::Point& location_in_screen);

  // Updates the resize mode backdrop. This is drawn behind windows to ensure
  // that the allotted space is always filled, even if the window itself hasn't
  // resized yet.
  void UpdateResizeBackdrop();

  // Updates the bounds for the snapped windows and divider according to the
  // current snap direction.
  void UpdateSnappedWindowsAndDividerBounds();

  // Gets the position where the black scrim should show.
  SnapPosition GetBlackScrimPosition(const gfx::Point& location_in_screen);

  // Updates |divider_position_| according to the current event location during
  // resizing.
  void UpdateDividerPosition(const gfx::Point& location_in_screen);

  // Returns the closest fix location for |divider_position_|.
  int GetClosestFixedDividerPosition();

  // While the divider is animating to somewhere, stop it and shove it there.
  void StopAndShoveAnimatedDivider();

  // Returns true if we should end tablet split view after resizing, i.e. the
  // split view divider is at an edge of the work area.
  bool ShouldEndTabletSplitViewAfterResizing();

  // Ends split view if |ShouldEndTabletSplitViewAfterResizing| returns true.
  // Handles extra details associated with dragging the divider off the screen.
  void EndTabletSplitViewAfterResizingIfAppropriate();

  // After resizing, if we should end split view mode, returns the window that
  // needs to be activated. Returns nullptr if there is no such window.
  aura::Window* GetActiveWindowAfterResizingUponExit();

  // Returns the maximum value of the |divider_position_|. It is the width of
  // the current display's work area bounds in landscape orientation, or height
  // of the current display's work area bounds in portrait orientation.
  int GetDividerEndPosition() const;

  // Called after a to-be-snapped window |window| got snapped. It updates the
  // split view states and notifies observers about the change. It also restore
  // the snapped window's transform if it's not identity and activate it. If
  // `previous_state` is given and it is a floated window, attempt to snap the
  // next MRU window if possible.
  void OnWindowSnapped(
      aura::Window* window,
      absl::optional<chromeos::WindowStateType> previous_state);

  // If there are two snapped windows, closing/minimizing/tab-dragging one of
  // them will open overview window grid on the closed/minimized/tab-dragged
  // window side of the screen. If there is only one snapped windows, closing/
  // minimizing/tab-dragging the snapped window will end split view mode and
  // adjust the overview window grid bounds if the overview mode is active at
  // that moment. |reason| specifies the reason that the snapped window is
  // detached from splitview.
  void OnSnappedWindowDetached(aura::Window* window,
                               WindowDetachedReason reason);

  // Returns the closest position ratio based on |distance| and |length|.
  float FindClosestPositionRatio(float distance, float length);

  // Gets the divider optional position ratios. The divider can always be
  // moved to the positions in `kFixedPositionRatios`. Whether the divider can
  // be moved to `kOneThirdSnapRatio` or `kTwoThirdSnapRatio` depends
  // on the minimum size of current snapped windows.
  void GetDividerOptionalPositionRatios(
      std::vector<float>* out_position_ratios);

  // Gets the expected window component depending on current screen orientation
  // for resizing purpose.
  int GetWindowComponentForResize(aura::Window* window);
  // Gets the expected end drag position for |window| depending on current
  // screen orientation and split divider position.
  gfx::Point GetEndDragLocationInScreen(aura::Window* window,
                                        const gfx::Point& location_in_screen);

  // Restores |window| transform to identity transform if applicable.
  void RestoreTransformIfApplicable(aura::Window* window);

  // Called after |newly_snapped| gets snapped. Updates window stacking.
  void UpdateWindowStackingAfterSnap(aura::Window* newly_snapped);

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

  // Updates the |snapping_window_transformed_bounds_map_| on |window|. It
  // should be called before trying to snap the window.
  void UpdateSnappingWindowTransformedBounds(aura::Window* window);

  // Inserts |window| into overview window grid if overview mode is active. Do
  // nothing if overview mode is inactive at the moment.
  void InsertWindowToOverview(aura::Window* window, bool animate = true);

  // Finalizes and cleans up after stopping dragging the divider bar to resize
  // snapped windows.
  void FinishWindowResizing(aura::Window* window);

  // Finalizes and cleans up divider dragging/animating. Called when the divider
  // snapping animation completes or is interrupted or totally skipped.
  void EndResizeImpl();

  // Called from a timer during resizing. Facilitates switching between fast and
  // normal tablet resizing modes.
  void OnResizeTimer();

  // Figure out which resize mode we should be using. This is based on the speed
  // at which the divider is dragged.
  void UpdateTabletResizeMode(base::TimeTicks event_time_ticks,
                              const gfx::Point& event_location);

  // Called by OnWindowDragEnded to do the actual work of finishing the window
  // dragging. If |is_being_destroyed| equals true, the dragged window is to be
  // destroyed, and SplitViewController should not try to put it in splitview.
  void EndWindowDragImpl(aura::Window* window,
                         bool is_being_destroyed,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen);

  // Do the split divider spawn animation. It will add a finishing touch to the
  // |window| animation that generally accommodates snapping by dragging.
  void DoSplitDividerSpawnAnimation(aura::Window* window);

  // Root window the split view is in.
  aura::Window* root_window_;

  // The current primary/secondary snapped window.
  aura::Window* primary_window_ = nullptr;
  aura::Window* secondary_window_ = nullptr;

  // Observe the windows that are to be snapped in split screen.
  std::unique_ptr<ToBeSnappedWindowsObserver> to_be_snapped_windows_observer_;

  // Split view divider widget. Only exist in tablet splitview mode. It's a
  // black bar stretching from one edge of the screen to the other, containing a
  // small white drag bar in the middle. As the user presses on it and drag it
  // to left or right, the left and right window will be resized accordingly.
  std::unique_ptr<SplitViewDivider> split_view_divider_;

  // A black scrim layer that fades in over a window when its width drops under
  // 1/3 of the width of the screen, increasing in opacity as the divider gets
  // closer to the edge of the screen.
  std::unique_ptr<ui::Layer> black_scrim_layer_;

  // Backdrop layers that may be visible below windows when resizing.
  std::unique_ptr<ui::Layer> left_resize_backdrop_layer_;
  std::unique_ptr<ui::Layer> right_resize_backdrop_layer_;

  // The window observer that obseves the tab-dragged window in tablet mode.
  std::unique_ptr<TabDraggedWindowObserver> dragged_window_observer_;

  // The distance between the origin of the divider and the origin of the
  // current display's work area in screen coordinates.
  //     |<---     divider_position_    --->|
  //     ---------------------------------------------------------------
  //     |                                  | |                        |
  //     |        primary_window_           | |   secondary_window_    |
  //     |                                  | |                        |
  //     ---------------------------------------------------------------
  int divider_position_ = -1;

  // The closest position ratio of divider among kFixedPositionRatios,
  // kOneThirdSnapRatio and kTwoThirdSnapRatio based on current
  // `divider_position_`. Used to update `divider_position_` on work area
  // changes.
  float divider_closest_ratio_ = std::numeric_limits<float>::quiet_NaN();

  // The location of the previous mouse/gesture event in screen coordinates.
  gfx::Point previous_event_location_;

  // The animation that animates the divider to a fixed position after resizing.
  std::unique_ptr<DividerSnapAnimation> divider_snap_animation_;

  // Current snap state.
  State state_ = State::kNoSnap;

  // The default snap position. It's decided by the first snapped window. If the
  // first window was snapped left, then |default_snap_position_| equals LEFT,
  // i.e., all the other windows will open snapped on the right side - and vice
  // versa.
  SnapPosition default_snap_position_ = SnapPosition::kNone;

  // Whether the previous layout is right-side-up (see |IsLayoutPrimary|).
  // Consistent with |IsLayoutPrimary|, |is_previous_layout_right_side_up_|
  // is always true in clamshell mode. It is not really used in clamshell mode,
  // but it is kept up to date in anticipation that future code changes could
  // introduce a bug similar to https://crbug.com/1029181 which could be
  // overlooked for years while occasionally irritating or confusing real users.
  bool is_previous_layout_right_side_up_ = true;

  // True when the divider is being dragged (not during its snap animation).
  bool is_resizing_ = false;

  // Stores the reason which cause splitview to end.
  EndReason end_reason_ = EndReason::kNormal;

  // The split view type. See SplitViewType for the differences between tablet
  // split view and clamshell split view.
  SplitViewType split_view_type_ = SplitViewType::kTabletType;

  // The time when splitview starts. Used for metric collection purpose.
  base::Time splitview_start_time_;

  // The map from a to-be-snapped window to its transformed bounds.
  base::flat_map<aura::Window*, gfx::Rect>
      snapping_window_transformed_bounds_map_;

  base::ObserverList<SplitViewObserver>::Unchecked observers_;

  // Records the presentation time of resize operation in split view mode.
  std::unique_ptr<ui::PresentationTimeRecorder> presentation_time_recorder_;

  // Observes windows and performs auto snapping if needed.
  std::unique_ptr<AutoSnapController> auto_snap_controller_;

  // The metrics controller for the same root window.
  std::unique_ptr<SplitViewMetricsController> split_view_metrics_controller_;

  // Register for DisplayObserver callbacks.
  display::ScopedDisplayObserver display_observer_{this};

  // A pointer to the to-be-snapped window that will be activated after it's
  // snapped in splitview. There can be two cases when this value can be
  // non-nullptr, when SnapWindow() explicitly specifies the window needs to be
  // activated, or when the to-be-snapped is from overview and was the active
  // window before entering overview, so when it's snapped in splitview, it
  // should remain to be the active window.
  aura::Window* to_be_activated_window_ = nullptr;

  // The split view resize mode for tablet mode.
  TabletResizeMode tablet_resize_mode_ = TabletResizeMode::kNormal;

  // True *while* a resize event is being processed.
  bool processing_resize_event_ = false;

  // Accumulated drag distance, during a time interval.
  int accumulated_drag_distance_ = 0;
  base::TimeTicks accumulated_drag_time_ticks_;

  // Used to potentially invoke `Resize()` during resizes. This is so that
  // tablet resize mode can switch to normal mode (letting windows be resized)
  // even if the divider isn't moved.
  base::OneShotTimer resize_timer_;

  // A flag indicates the window bounds is currently changed due to the virtual
  // keyboard.
  bool changing_bounds_by_vk_ = false;
};

}  // namespace ash

#endif  // ASH_WM_SPLITVIEW_SPLIT_VIEW_CONTROLLER_H_
