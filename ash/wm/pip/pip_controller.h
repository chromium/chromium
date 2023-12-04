// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_CONTROLLER_H_
#define ASH_WM_PIP_PIP_CONTROLLER_H_

#include "ash/ash_export.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"
#include "ui/aura/window_observer.h"

namespace ash {

// This controller manages the PiP window.
class ASH_EXPORT PipController : public aura::WindowObserver {
 public:
  PipController();
  PipController(const PipController&) = delete;
  PipController& operator=(const PipController&) = delete;
  ~PipController() override;

  // Returns the PiP window that this controller is managing.
  aura::Window* pip_window() { return pip_window_; }

  // Set the target window for this controller.
  void SetPipWindow(aura::Window* window);

  // Remove the target window from this controller.
  void UnsetPipWindow(aura::Window* window);

  // Updates the PiP bounds if necessary. This may need to happen when the
  // display work area changes, or if system ui regions like the virtual
  // keyboard position changes.
  void UpdatePipBounds();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  // The `pip_window` this controller is managing.
  raw_ptr<aura::Window, ExperimentalAsh> pip_window_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      pip_window_observation_{this};
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_CONTROLLER_H_
