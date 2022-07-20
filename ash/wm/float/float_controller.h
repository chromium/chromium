// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
#define ASH_WM_FLOAT_FLOAT_CONTROLLER_H_

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
// |float_container|.
class ASH_EXPORT FloatController : public aura::WindowObserver,
                                   public TabletModeObserver,
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

  aura::Window* float_window() { return float_window_; }

  MagnetismCorner magnetism_corner() const { return magnetism_corner_; }

  // Gets the ideal float bounds of `window` in tablet mode if it were to be
  // floated.
  gfx::Rect GetPreferredFloatWindowTabletBounds(aura::Window* window);

  // Tucks or untucks `float_window_`. Does nothing if the window is already
  // tucked or untucked.
  void MaybeTuckFloatedWindow();
  void MaybeUntuckFloatedWindow();

  // Called by the resizer when a drag is completed. This assumes the dragged
  // window associated with the resizer is `float_window_`. Updates the bounds
  // and magnetism of the floated window.
  void OnDragCompleted(const gfx::PointF& last_location_in_parent);

  // Called by the resizer when a drag is completed by a fling or swipe gesture
  // event. Updates the magnetism of the window and then tucks the window
  // offscreen. `left` and `up` are used to determine the direction of the fling
  // or swipe gesture.
  void OnFlingOrSwipe(bool left, bool up);

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

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
  friend class DefaultState;
  friend class TabletModeWindowState;
  friend class WindowFloatTest;

  // Floats/Unfloats `window`.
  // Only one floating window is allowed, floating a new window will
  // unfloat the other floated window (if any).
  void Float(aura::Window* window);
  void Unfloat(aura::Window* window);

  // Unfloats floated window.
  void ResetFloatedWindow();

  // Updates `window`'s shadow and bounds depending on whether is in floated and
  // if it is in tablet mode.
  void MaybeUpdateWindowUIAndBoundsForTablet(aura::Window* window);

  // Only one floating window is allowed, updated when a new window
  // is floated.
  aura::Window* float_window_ = nullptr;

  // When a window is floated, the window position should not be auto-managed.
  // Use this value to reset the auto-managed state when unfloat a window.
  bool position_auto_managed_ = false;

  // The corner a floated window should be magnetized to. It persists throughout
  // the session; if you drag a window to the bottom left and float another
  // window, that window will also be magnetized to the bottom left.
  MagnetismCorner magnetism_corner_ = MagnetismCorner::kBottomRight;

  // Scoped class that handles the special tucked window state, which is not a
  // normal window state. Null when there is no current tucked window.
  std::unique_ptr<ScopedWindowTucker> scoped_window_tucker_;

  // Observes floated window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      float_window_observation_{this};

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  absl::optional<display::ScopedOptionalDisplayObserver> display_observer_;
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
