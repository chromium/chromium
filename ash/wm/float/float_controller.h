// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
#define ASH_WM_FLOAT_FLOAT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/rotator/screen_rotation_animator_observer.h"
#include "ash/shell_observer.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/gtest_prod_util.h"
#include "base/scoped_multi_source_observation.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/base/window_state_type.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "ui/display/display_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace display {
enum class TabletState;
}  // namespace display

namespace views {
class Widget;
}  // namespace views

namespace ash {

class Shell;
class WorkspaceEventHandler;

// This controller allows windows to be on top of all app windows, but below
// pips. When a window is 'floated', it remains always on top for the user so
// that they can complete secondary tasks. Floated window stays in the
// `kShellWindowId_FloatContainer`.
class ASH_EXPORT FloatController : public display::DisplayObserver,
                                   public ShellObserver,
                                   public DesksController::Observer,
                                   public chromeos::FloatControllerBase,
                                   public ScreenRotationAnimatorObserver {
 public:
  // The possible corners that a floated window can be placed in tablet mode.
  // The default is `kBottomRight` and this is changed by dragging the window.
  enum class MagnetismCorner {
    kTopLeft = 0,
    kTopRight,
    kBottomLeft,
    kBottomRight,
  };

  FloatController();
  FloatController(const FloatController&) = delete;
  FloatController& operator=(const FloatController&) = delete;
  ~FloatController() override;

  // Returns float window bounds in clamshell mode in root window coordinates.
  static gfx::Rect GetFloatWindowClamshellBounds(
      aura::Window* window,
      chromeos::FloatStartLocation location);

  // Gets the ideal float bounds of `window` in tablet mode if it were to be
  // floated, in root window coordinates.
  static gfx::Rect GetFloatWindowTabletBounds(aura::Window* window);

  // Float the `window` if it's not floated, otherwise unfloat it. If called in
  // clamshell mode, the default location is the bottom right.
  void ToggleFloat(aura::Window* window);

  // Untucks `floated_window`. Does nothing if the window is already untucked.
  void MaybeUntuckFloatedWindowForTablet(aura::Window* floated_window);

  // Checks if `floated_window` is tucked.
  bool IsFloatedWindowTuckedForTablet(const aura::Window* floated_window) const;

  // Returns true if `floated_window` is not tucked and magnetized to the
  // bottom. Used by the shelf layout manager to determine what window to use
  // for the drag window from shelf feature.
  bool IsFloatedWindowAlignedWithShelf(aura::Window* floated_window) const;

  // Gets the tuck handle for a floated and tucked window.
  views::Widget* GetTuckHandleWidget(const aura::Window* floated_window) const;

  // Called by the resizer when a drag is completed. Updates the bounds and
  // magnetism of the `floated_window`.
  void OnDragCompletedForTablet(aura::Window* floated_window);

  // TODO(shidi): Temporary passing `floated_window` here, will follow-up in
  // desk logic to use only `active_floated_window_`.
  // Called by the resizer when a drag is completed by a fling or swipe gesture
  // event. Updates the magnetism of the window and then tucks the window
  // offscreen based on `velocity_x` and `velocity_y`.
  void OnFlingOrSwipeForTablet(aura::Window* floated_window,
                               float velocity_x,
                               float velocity_y);

  // Returns the desk where floated window belongs to if window is floated and
  // registered under `floated_window_info_map_`, otherwise returns nullptr.
  const Desk* FindDeskOfFloatedWindow(const aura::Window* window) const;
  // Returns the floated window that belongs to `desk`. If `desk` doesn't have a
  // floated window, returns nullptr.
  aura::Window* FindFloatedWindowOfDesk(const Desk* desk) const;

  // Called when moving all `original_desk`'s windows out to `target_desk` due
  // to the removal of `original_desk`. This function takes care of floated
  // window (if any) since it doesn't belong to the desk container. Note: during
  // desk removal/combination, `floated_window` will be unfloated if
  // `target_desk` has a floated window.
  void OnMovingAllWindowsOutToDesk(Desk* original_desk, Desk* target_desk);

  // Called when moving the `floated_window` from `active_desk` to
  // `target_desk`. This function takes care of floated window since it doesn't
  // belong to the desk container. Note: Unlike `OnMovingAllWindowsOutToDesk`
  // above, if `target_desk` has a floated window, it will be unfloated, while
  // `floated_window` remains floated. Note: When dragging `floated_window` to a
  // different display, we need to map `floated_window` to the desk container
  // with same ID on target display's root.
  void OnMovingFloatedWindowToDesk(aura::Window* floated_window,
                                   Desk* active_desk,
                                   Desk* target_desk,
                                   aura::Window* target_root);

  void ClearWorkspaceEventHandler(aura::Window* root);

  // DesksController::Observer:
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;
  void OnDisplayTabletStateChanged(display::TabletState state) override;

  // ShellObserver:
  void OnRootWindowAdded(aura::Window* root_window) override;
  void OnRootWindowWillShutdown(aura::Window* root_window) override;
  void OnPinnedStateChanged(aura::Window* pinned_window) override;

  // chromeos::FloatControllerBase:
  void SetFloat(aura::Window* window,
                chromeos::FloatStartLocation float_start_location) override;
  void UnsetFloat(aura::Window* window) override;

  // ScreenRotationAnimatorObserver:
  void OnScreenCopiedBeforeRotation() override;
  void OnScreenRotationAnimationFinished(ScreenRotationAnimator* animator,
                                         bool canceled) override;

 private:
  class FloatedWindowInfo;
  friend class ClientControlledState;
  friend class DefaultState;
  friend class FloatTestApi;
  friend class TabletModeWindowState;

  static MagnetismCorner GetMagnetismCornerForBounds(
      const gfx::Rect& bounds_in_screen);

  // Calls `FloatImpl()` and additionally updates the magnetism if needed.
  void FloatForTablet(aura::Window* window,
                      chromeos::WindowStateType old_state_type);

  // Floats/Unfloats `window`. Only one floating window is allowed per desk,
  // floating a new window on the same desk or moving a floated window to that
  // desk will unfloat the other floated window (if any).
  // Note: currently window can only be floated from an active desk.
  void FloatImpl(aura::Window* window);
  void UnfloatImpl(aura::Window* window);

  // Unfloats `floated_window` from the desk it belongs to.
  void ResetFloatedWindow(aura::Window* floated_window);

  // Returns the `FloatedWindowInfo` for the given window if it's floated, or
  // nullptr otherwise.
  FloatedWindowInfo* MaybeGetFloatedWindowInfo(
      const aura::Window* window) const;

  // This is called by `FloatedWindowInfo::OnWindowDestroying` to remove
  // `floated_window` from `floated_window_info_map_`.
  void OnFloatedWindowDestroying(aura::Window* floated_window);

  // Called by `OnDisplayTabletStateChanged.
  void OnTabletModeStarted();
  void OnTabletModeEnding();

  // Used to map floated window to to its FloatedWindowInfo.
  // Contains extra info for a floated window such as its pre-float auto managed
  // state and tablet mode magnetism.
  base::flat_map<aura::Window*, std::unique_ptr<FloatedWindowInfo>>
      floated_window_info_map_;

  // Workspace event handler which handles double click events to change to
  // maximized state as well as horizontally and vertically maximize. We create
  // one per root window.
  base::flat_map<aura::Window*, std::unique_ptr<WorkspaceEventHandler>>
      workspace_event_handlers_;

  // Float window counter within a session, used for
  // `kFloatWindowCountsPerSessionHistogramName`.
  int floated_window_counter_ = 0;
  // Counts of how many floated window are moved to another desk within a
  // session. `kFloatWindowMoveToAnotherDeskCountsHistogramName`
  int floated_window_move_to_another_desk_counter_ = 0;

  bool disable_tuck_education_for_testing_{false};

  base::ScopedMultiSourceObservation<ScreenRotationAnimator,
                                     ScreenRotationAnimatorObserver>
      screen_rotation_observations_{this};

  base::ScopedObservation<DesksController, DesksController::Observer>
      desks_controller_observation_{this};

  std::optional<display::ScopedOptionalDisplayObserver> display_observer_;
  base::ScopedObservation<Shell, ShellObserver> shell_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
