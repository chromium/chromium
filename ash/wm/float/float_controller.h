// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_FLOAT_FLOAT_CONTROLLER_H_
#define ASH_WM_FLOAT_FLOAT_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// This controller allows windows to be on top of all app windows, but below
// pips. When a window is 'floated', it remains always on top for the user so
// that they can complete secondary tasks. Floated window stays in the
// |float_container|.
class ASH_EXPORT FloatController : public aura::WindowObserver {
 public:
  FloatController();
  FloatController(const FloatController&) = delete;
  FloatController& operator=(const FloatController&) = delete;
  ~FloatController() override;

  aura::Window* float_window() { return float_window_; }

  // Return true if `window` is floated, otherwise false.
  bool IsFloated(aura::Window* window) const;

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  friend class WindowState;

  // Floats/Unfloats `window`.
  // Only one floating window is allowed, floating a new window will
  // unfloat the other floated window (if any).
  void Float(aura::Window* window);
  void Unfloat(aura::Window* window);

  // Unfloats floated window.
  void ResetFloatedWindow();

  // Only one floating window is allowed, updated when a new window
  // is floated.
  aura::Window* float_window_ = nullptr;

  // Observes floated window.
  base::ScopedObservation<aura::Window, aura::WindowObserver>
      float_window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_FLOAT_FLOAT_CONTROLLER_H_