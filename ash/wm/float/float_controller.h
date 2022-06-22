// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
#define ASH_WM_FLOAT_FLOAT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/tablet_mode_observer.h"
#include "ash/wm/tablet_mode/tablet_mode_controller.h"
#include "base/scoped_observation.h"
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
                                   public display::DisplayObserver {
 public:
  FloatController();
  FloatController(const FloatController&) = delete;
  FloatController& operator=(const FloatController&) = delete;
  ~FloatController() override;

  // Gets the ideal float bounds of `window` in tablet mode if it were to be
  // floated.
  static gfx::Rect GetPreferredFloatWindowTabletBounds(aura::Window* window);

  // Determines if a window can be floated in tablet mode.
  static bool CanFloatWindowInTablet(aura::Window* window);

  aura::Window* float_window() { return float_window_; }

  // Return true if `window` is floated, otherwise false.
  bool IsFloated(const aura::Window* window) const;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

  // TabletModeObserver:
  void OnTabletModeStarting() override;
  void OnTabletModeEnded() override;
  void OnTabletControllerDestroyed() override;

  // display::DisplayObserver:
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;

 private:
  friend class DefaultState;
  friend class TabletModeWindowState;

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

  // Observes floated window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      float_window_observation_{this};

  base::ScopedObservation<TabletModeController, TabletModeObserver>
      tablet_mode_observation_{this};
  absl::optional<display::ScopedOptionalDisplayObserver> display_observer_;
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
