// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_SPLITVIEW_SPLIT_VIEW_CONTROLLER_H_
#define ASH_WM_SPLITVIEW_SPLIT_VIEW_CONTROLLER_H_

#include <limits>
#include <memory>
#include <optional>
#include <vector>

#include "ash/accessibility/accessibility_observer.h"
#include "ash/ash_export.h"
#include "ash/public/cpp/keyboard/keyboard_controller_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/overview/overview_metrics.h"
#include "ash/wm/overview/overview_observer.h"
#include "ash/wm/overview/overview_types.h"
#include "ash/wm/splitview/layout_divider_controller.h"
#include "ash/wm/splitview/split_view_divider.h"
#include "ash/wm/splitview/split_view_types.h"
#include "ash/wm/window_state_observer.h"
#include "ash/wm/wm_event.h"
#include "ash/wm/wm_metrics.h"
#include "base/containers/flat_map.h"
#include "base/memory/raw_ptr.h"
#include "base/observer_list.h"
#include "base/time/time.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display.h"
#include "ui/display/display_observer.h"
#include "ui/wm/public/activation_change_observer.h"

namespace display {
enum class TabletState;
}  // namespace display

namespace gfx {
class Point;
}  // namespace gfx

namespace ui {
class Layer;
class PresentationTimeRecorder;
}  // namespace ui

namespace ash {
class AutoSnapController;
class OverviewSession;
class SplitViewOverviewSession;
class SplitViewMetricsController;
class SplitViewObserver;
class SplitViewOverviewSessionTest;

// `SplitViewController` controls what the window snapping behaviors should be
// in different UI modes (clamshell UI mode and tablet UI mode), and how the
// window snapping interacts with the overview mode. There is an instance for
// each display.
// The window snapping behaviors in clamshell mode:
// 1. If the feature flag `kSnapGroup` is enabled, once a window is snapped to
// one side of the screen, Overview will open automatically on the other side of
// the screen for the user to decide a 2nd window to snap. On window selected,
// the two windows will then be in one snap group. `SplitViewController` will
// observe the two snapped windows and control their behaviors until the two
// windows are no longer in a snap group. The two snapped windows and the split
// view divider are placed side-by-side with no overlap in the split screen (see
// `SnapGroup` for more details). User is able to resize the two windows with
// the `split_view_divider_`.  When the user explicitly ends split view mode,
// two windows will be restored to their previous bounds and the
// `split_view_divider_` will be reset. `SplitViewController` will no longer
// observe and manage the two windows.
// 2. If in overview mode and on one window snapped, the overview grid will show
// up on the other half of the screen for user to choose the other to-be-snapped
// window. The overview session won't show on the other half of the screen
// if there is no window can be shown in overview.
// 3. For other cases in clamshell mode, the snapping behaviors are not managed
// by `SplitViewController`.
//
// The window snapping behaviors in tablet mode:
// The window snapping behaviors in tablet mode will be managed by
// `SplitViewController`. On one window snapped in the tablet mode, the overview
// session will show up on the other half of the screen for user to choose the
// to-be-snapped window. And the user has to explicitly end the split view mode.
// TODO(xdai): Make it work for multi-display non mirror environment.
class ASH_EXPORT SplitViewController : public aura::WindowObserver,
                                       public WindowStateObserver,
                                       public ShellObserver,
                                       public OverviewObserver,
                                       public display::DisplayObserver,
                                       public AccessibilityObserver,
                                       public ash::KeyboardControllerObserver,
                                       public wm::ActivationChangeObserver,
                                       public LayoutDividerController {
 public:
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
    // Splitview is being ended due to a Snap Group being added.
    kSnapGroups,
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

  explicit SplitViewController(aura::Window* root_window);

  SplitViewController(const SplitViewController&) = delete;
  SplitViewController& operator=(const SplitViewController&) = delete;

  ~SplitViewController() override;

  aura::Window* root_window() { return root_window_; }
  aura::Window* primary_window() { return primary_window_; }
  aura::Window* secondary_window() { return secondary_window_; }

  State state() const { return state_; }
  SnapPosition default_snap_position() const { return default_snap_position_; }
  SplitViewDivider* split_view_divider() { return &split_view_divider_; }
  EndReason end_reason() const { return end_reason_; }
  SplitViewMetricsController* split_view_metrics_controller() {
    return split_view_metrics_controller_.get();
  }
  aura::Window* to_be_activated_window() { return to_be_activated_window_; }

  // Returns the divider position of the split view divider.
  int GetDividerPosition() const;

  // Returns true if the divider is resizing (not animating) in tablet mode
  // split view, or between two windows in Snap Groups.
  bool IsResizingWithDivider() const;

  // Returns true if split view mode is active. Please see SplitViewType above
  // to see the difference between tablet mode and clamshell mode splitview
  // mode.
  bool InSplitViewMode() const;
  bool InClamshellSplitViewMode() const;
  bool InTabletSplitViewMode() const;

  // Checks the following criteria:
  // 1. Split view mode is supported (see |ShouldAllowSplitView|).
  // 2. |window| can be activated (see |wm::CanActivateWindow|).
  // 3. The |WindowState| of |window| can snap (see |WindowState::CanSnap|).
  // 4. |window|'s minimum size, if any, fits into the left or top with the
  //    default divider position. (If the work area length is odd, then the
  //    right or bottom will be one pixel larger.)
  // See also the `DCHECK`s in `SnapWindow()`.
  bool CanSnapWindow(aura::Window* window, float snap_ratio) const;

  // Returns true if `window` can keep snapped with the current snap ratio.
  bool CanKeepCurrentSnapRatio(aura::Window* window) const;

  // Returns true if partial overview should start on the opposite side of the
  // screen on the given `window` snapped.
  bool WillStartPartialOverview(aura::Window* window) const;

  // Returns the snap ratio (if valid) for `window` depending on the default
  // window. Returns null if `window` cannot get snapped. If there is no default
  // window, it will check if `window` can be half snapped. Otherwise, it checks
  // if `window` can be snapped opposite of the default window. If default
  // window is 2/3 and `window` cannot be snapped 1/3 but can be snapped 1/2, it
  // will be snapped 1/2 unless default window cannot be snapped 1/2.
  std::optional<float> ComputeAutoSnapRatio(aura::Window* window);

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
  // transition. `snap_action_source` specifies the source for this snap event.
  void SnapWindow(aura::Window* window,
                  SnapPosition snap_position,
                  WindowSnapActionSource snap_action_source =
                      WindowSnapActionSource::kNotSpecified,
                  bool activate_window = false,
                  float snap_ratio = chromeos::kDefaultSnapRatio);

  // This is called by `BaseState` or `TabletModeWindowState` when receiving a
  // snap WMEvent i.e. WM_EVENT_SNAP_PRIMARY or WM_EVENT_SNAP_SECONDARY. `this`
  // will decide if this window needs to be snapped in split view.
  // `snap_action_source` specifies the source for this snap event.
  void OnSnapEvent(aura::Window* window,
                   WMEventType event_type,
                   WindowSnapActionSource snap_action_source);

  // Attaches the to-be-snapped `window` to split view at `snap_position`. It
  // will try to remove the `window` from the overview grid first if `window`
  // is contained in the overview grid. We'll add a finishing touch to the snap
  // animation of `window` if split view mode is not already active, and if
  // `window` is not minimized and has a non-identity transform.
  // `snap_action_source` specifies the source for this snap event.
  void AttachToBeSnappedWindow(aura::Window* window,
                               SnapPosition snap_position,
                               WindowSnapActionSource snap_action_source);

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

  // Returns true if we should consider the width of the split view divider.
  bool ShouldConsiderDivider() const;

  // Returns true during the divider snap animation.
  bool IsDividerAnimating() const;

  // Ends the split view mode, from which point `SplitViewController` no longer
  // manages the window(s).
  void EndSplitView(EndReason end_reason = EndReason::kNormal);

  // Returns true if `window` is a snapped window in splitview.
  bool IsWindowInSplitView(const aura::Window* window) const;

  // Returns true if `window` is in a transitinal state which means that
  // `SplitViewController` has already changed its internal snapped state for
  // `window` but the snapped state has not been applied to `window`'s window
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
                         const gfx::Point& last_location_in_screen,
                         WindowSnapActionSource snap_action_source);
  void OnWindowDragCanceled();

  // Computes the snap position for a dragged window, based on the last
  // mouse/gesture event location. Called by |EndWindowDragImpl| when
  // desired_snap_position is |NONE| but because split view is already active,
  // the dragged window needs to be snapped anyway.
  SnapPosition ComputeSnapPosition(const gfx::Point& last_location_in_screen);

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
  void OnWindowRemovingFromRootWindow(aura::Window* window,
                                      aura::Window* new_root) override;

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
  void OnDisplaysRemoved(const display::Displays& removed_displays) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // AccessibilityObserver:
  void OnAccessibilityStatusChanged() override;
  void OnAccessibilityControllerShutdown() override;

  // KeyboardControllerObserver:
  void OnKeyboardOccludedBoundsChanged(const gfx::Rect& screen_bounds) override;

  // wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // LayoutDividerController:
  aura::Window* GetRootWindow() const override;
  void StartResizeWithDivider(const gfx::Point& location_in_screen) override;
  void UpdateResizeWithDivider(const gfx::Point& location_in_screen) override;
  bool EndResizeWithDivider(const gfx::Point& location_in_screen) override;
  void OnResizeEnding() override;
  void OnResizeEnded() override;
  void SwapWindows() override;
  gfx::Rect GetSnappedWindowBoundsInScreen(
      SnapPosition snap_position,
      aura::Window* window_for_minimum_size,
      float snap_ratio,
      bool account_for_divider_width) const override;
  SnapPosition GetPositionOfSnappedWindow(
      const aura::Window* window) const override;

  static void SetUseFastResizeForTesting(bool val);

 private:
  friend class SplitViewControllerTest;
  friend class SplitViewTestApi;
  friend class SplitViewDivider;
  friend class SplitViewOverviewSessionTest;
  friend class SplitViewOverviewSession;
  class DividerSnapAnimation;
  class ToBeSnappedWindowsObserver;

  // Reason that a snapped window is detached from the splitview.
  enum class WindowDetachedReason {
    kWindowMinimized,
    kWindowDestroyed,
    kWindowDragged,
    kWindowFloated,
    kWindowMovedToAnotherDisplay,
    kAddedToSnapGroup,
  };

  // These functions return the snapped window in the specified snap position
  // (left/top or right/bottom) based on the display's orientation.
  //
  // In primary screen orientation:
  //  - `GetPhysicallyLeftOrTopWindow()` returns the `primary_window_`;
  //  - `GetPhysicallyRightOrBottomWindow()` returns the `secondary_window_`.
  //
  // In non-primary screen orientation:
  //  - `GetPhysicallyLeftOrTopWindow()` returns the `secondary_window_`;
  //  - `GetPhysicallyRightOrBottomWindow()` returns the `primary_window_`.
  aura::Window* GetPhysicallyLeftOrTopWindow();
  aura::Window* GetPhysicallyRightOrBottomWindow();

  // Starts observing |window|.
  void StartObserving(aura::Window* window);
  // Stop observing the window at associated with |snap_position|. Also updates
  // shadows and sets |primary_window_| or |secondary_window_| to nullptr.
  void StopObserving(SnapPosition snap_position);

  // Updates split view state and notify its observer about the change.
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

  // Updates the bounds of the given snapped `window` in splitview.
  void UpdateSnappedWindowBounds(aura::Window* window);

  // Updates the bounds for the two snapped windows
  void UpdateSnappedWindowsBounds();

  // Updates the bounds for the snapped windows and divider.
  // TODO(http://b/330567348): Consolidate these three functions and make sure
  // they work properly behind the scenes.
  void UpdateSnappedWindowsAndDividerBounds();

  // Gets the position where the black scrim should show.
  SnapPosition GetBlackScrimPosition(const gfx::Point& location_in_screen);

  // Returns the closest fixed location to `divider_position`.
  int GetClosestFixedDividerPosition(int divider_position);

  // `StopSnapAnimation()` and notifies the `observers_` about the divider
  // position change.
  void StopAndShoveAnimatedDivider();

  // Stops the divider animation and `SetDividerPosition()`.
  void StopSnapAnimation();

  // Returns true if we should end split view after resizing, i.e. the
  // split view divider is at an edge of the work area.
  bool ShouldEndSplitViewAfterResizingAtEdge();

  // Ends split view if `ShouldEndSplitViewAfterResizingAtEdge()` returns true.
  // Handles extra details associated with dragging the divider off the screen.
  void EndSplitViewAfterResizingAtEdgeIfAppropriate();

  // After resizing, if we should end split view mode, returns the window that
  // needs to be activated. Returns nullptr if there is no such window.
  aura::Window* GetActiveWindowAfterResizingUponExit();

  // Called after a to-be-snapped window `window` got snapped. It updates the
  // split view states and notifies observers about the change. It also restore
  // the snapped window's transform if it's not identity and activate it. If
  // `previous_state` is given and it is a floated window, attempt to snap the
  // next MRU window if possible. `snap_action_source` specifies the source for
  // this snap event.
  void OnWindowSnapped(aura::Window* window,
                       std::optional<chromeos::WindowStateType> previous_state,
                       WindowSnapActionSource snap_action_source);

  // If there are two snapped windows, closing/minimizing/tab-dragging one of
  // them will open overview window grid on the closed/minimized/tab-dragged
  // window side of the screen. If there is only one snapped windows, closing/
  // minimizing/tab-dragging the snapped window will end split view mode and
  // adjust the overview window grid bounds if the overview mode is active at
  // that moment. |reason| specifies the reason that the snapped window is
  // detached from splitview.
  void OnSnappedWindowDetached(aura::Window* window,
                               WindowDetachedReason reason);

  // Returns the closest ratio to the `current_ratio`. `current_ratio` is the
  // the ratio between current divider position and the farthest position
  // divider is allowed to end at.
  float FindClosestPositionRatio(float current_ratio);

  // Gets the divider optional position ratios. The divider can always be
  // moved to the positions in `kFixedPositionRatios`. Whether the divider can
  // be moved to `chromeos::kOneThirdSnapRatio` or
  // `chromeos::kTwoThirdSnapRatio` depends on the minimum size of current
  // snapped windows.
  void ModifyPositionRatios(std::vector<float>& out_position_ratios);

  // Restores |window| transform to identity transform if applicable.
  void RestoreTransformIfApplicable(aura::Window* window);

  // During resizing, it's possible that the resizing bounds of the snapped
  // window is smaller than its minimum bounds, in this case we apply a
  // translation to the snapped window to make it visually be placed outside of
  // the workspace area.
  void SetWindowsTransformDuringResizing();

  // Restores the snapped windows transform to identity transform after
  // resizing.
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

  // Finalizes and cleans up divider dragging/animating. Called when the divider
  // snapping animation completes or is interrupted or totally skipped.
  void EndResizeWithDividerImpl();

  // Called from a timer during resizing. Facilitates switching between fast and
  // normal tablet resizing modes.
  void OnResizeTimer();

  // Figures out which resize mode we should be using. This is based on the
  // speed at which the divider is dragged.
  void UpdateTabletResizeMode(base::TimeTicks event_time_ticks,
                              const gfx::Point& event_location);

  // Called when the display tablet state is changed.
  void OnTabletModeStarted();
  void OnTabletModeEnding();
  void OnTabletModeEnded();

  // Called by `OnWindowDragEnded()` to do the actual work of finishing the
  // window dragging. If `is_being_destroyed` equals true, the dragged window is
  // to be destroyed, and SplitViewController should not try to put it in
  // splitview. `snap_action_source` specifies the source for this snap event.
  void EndWindowDragImpl(aura::Window* window,
                         bool is_being_destroyed,
                         SnapPosition desired_snap_position,
                         const gfx::Point& last_location_in_screen,
                         WindowSnapActionSource snap_action_source);

  // Called by `SwapWindows()` to swap the window(s) if exist that occupy the
  // `SnapPosition::kPrimary` and `SnapPosition::kSecondary`. The bounds of the
  // window(s) will also be updated.
  void SwapWindowsAndUpdateBounds();

  // Root window the split view is in.
  raw_ptr<aura::Window, DanglingUntriaged> root_window_;

  // The current primary/secondary snapped window.
  raw_ptr<aura::Window> primary_window_ = nullptr;
  raw_ptr<aura::Window> secondary_window_ = nullptr;

  // Observes the windows that are to be snapped in split screen.
  std::unique_ptr<ToBeSnappedWindowsObserver> to_be_snapped_windows_observer_;

  // Split view divider which is a black bar stretching from one edge of the
  // screen to the other, containing a small white drag bar in the middle. As
  // the user presses on it and drag it to horizontally or vertically, the
  // windows will be resized either horizontally or vertically accordingly. It
  // will be used in these two cases:
  // 1. Tablet splitview mode;
  // 2. Clamshell splitview mode when `kSnapGroup` is enabled.
  SplitViewDivider split_view_divider_;

  // A black scrim layer that fades in over a window when its width drops under
  // 1/3 of the width of the screen, increasing in opacity as the divider gets
  // closer to the edge of the screen.
  std::unique_ptr<ui::Layer> black_scrim_layer_;

  // Backdrop layers that may be visible below windows when resizing.
  std::unique_ptr<ui::Layer> left_resize_backdrop_layer_;
  std::unique_ptr<ui::Layer> right_resize_backdrop_layer_;

  // The closest position ratio of divider among kFixedPositionRatios,
  // kOneThirdSnapRatio and kTwoThirdSnapRatio based on current
  // `SplitViewDivider::divider_position_`. Used to update
  // `SplitViewDivider::divider_position_` on work area changes.
  // TODO(sophiewen | michelefan): Move this variable to `SplitViewDivider`.
  float divider_closest_ratio_ = std::numeric_limits<float>::quiet_NaN();

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

  // Stores the reason which cause splitview to end.
  EndReason end_reason_ = EndReason::kNormal;

  // Stores the overview start and enter/exit type.
  std::optional<OverviewStartAction> overview_start_action_;
  std::optional<OverviewEnterExitType> enter_exit_overview_type_;

  // The time when splitview starts. Used for metric collection purpose.
  base::Time splitview_start_time_;

  // The map from a to-be-snapped window to its transformed bounds.
  base::flat_map<aura::Window*, gfx::Rect>
      snapping_window_transformed_bounds_map_;

  base::ObserverList<SplitViewObserver>::Unchecked observers_;

  // Records the presentation time of resize operation in tablet split view
  // mode.
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
  raw_ptr<aura::Window, DanglingUntriaged> to_be_activated_window_ = nullptr;

  // The split view resize mode for tablet mode.
  TabletResizeMode tablet_resize_mode_ = TabletResizeMode::kNormal;

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
