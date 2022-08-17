// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
#define ASH_WM_FLOAT_FLOAT_CONTROLLER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observation.h"
#include "chromeos/ui/frame/multitask_menu/float_controller_base.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"
#include "ui/display/display_observer.h"

namespace ash {

// This controller allows windows to be on top of all app windows, but below
// pips. When a window is 'floated', it remains always on top for the user so
// that they can complete secondary tasks. Floated window stays in the
// `kShellWindowId_FloatContainer`.
class ASH_EXPORT FloatController : public TabletModeObserver,
                                   public display::DisplayObserver,
                                   public chromeos::FloatControllerBase {
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

  // The distance from the edge of the floated window to the edge of the work
  // area when it is floated.
  static constexpr int kFloatWindowPaddingDp = 8;

  // Returns float window bounds in clamshell mode.
  static gfx::Rect GetPreferredFloatWindowClamshellBounds(aura::Window* window);

  // Determines if a window can be floated in clamshell mode.
  static bool CanFloatWindowInClamshell(aura::Window* window);

  // Determines if a window can be floated in tablet mode.
  static bool CanFloatWindowInTablet(aura::Window* window);

  // Gets the ideal float bounds of `floated_window` in tablet mode if it were
  // to be floated.
  gfx::Rect GetPreferredFloatWindowTabletBounds(
      aura::Window* floated_window) const;

  // Untucks `floated_window`. Does nothing if the window is already untucked.
  void MaybeUntuckFloatedWindowForTablet(aura::Window* floated_window);

  // Checks if `floated_window` is tucked.
  bool IsFloatedWindowTuckedForTablet(const aura::Window* floated_window) const;

  // Called by the resizer when a drag is completed. Updates the bounds
  // and magnetism of the `floated_window`.
  void OnDragCompletedForTablet(aura::Window* floated_window,
                                const gfx::PointF& last_location_in_parent);

  // TODO(shidi): Temporary passing `floated_window` here, will follow-up in
  // desk logic to use only `active_floated_window_`.
  // Called by the resizer when a drag is completed by a fling or swipe gesture
  // event. Updates the magnetism of the window and then tucks the window
  // offscreen. `left` and `up` are used to determine the direction of the fling
  // or swipe gesture.
  void OnFlingOrSwipeForTablet(aura::Window* floated_window,
                               bool left,
                               bool up);

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeEnding() override;
  void OnTabletControllerDestroyed() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

  // chromeos::FloatControllerBase:
  void ToggleFloat(aura::Window* window) override;

 private:
  class ScopedWindowTucker;
  class FloatedWindowInfo;
  friend class DefaultState;
  friend class TabletModeWindowState;
  friend class WindowFloatTest;

  // Floats/Unfloats `window`.
  // Only one floating window is allowed per desk, floating a new window on the
  // same desk or moving a floated window to that desk will unfloat the other
  // floated window (if any).
  void Float(aura::Window* window);
  void Unfloat(aura::Window* window);

  // Unfloats `floated_window` from the desk it belongs to.
  void ResetFloatedWindow(aura::Window* floated_window);

  // Returns the `FloatedWindowInfo` for the given window if it's floated, or
  // nullptr otherwise.
  FloatedWindowInfo* MaybeGetFloatedWindowInfo(
      const aura::Window* window) const;

  // This is called by `FloatedWindowInfo::OnWindowDestroying` to remove
  // `floated_window` from `floated_window_info_map_`.
  void OnFloatedWindowDestroying(aura::Window* floated_window);

  // Used to map floated window to to its FloatedWindowInfo.
  // Contains extra info for a floated window such as its pre-float auto managed
  // state and tablet mode magnetism.
  base::flat_map<aura::Window*, std::unique_ptr<FloatedWindowInfo>>
      floated_window_info_map_;

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  absl::optional<display::ScopedOptionalDisplayObserver> display_observer_;
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
