// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_CONTROLLER_H_
#define ASH_WM_PIP_PIP_CONTROLLER_H_

#include "ash/ash_export.h"
#include "ash/wm/scoped_window_tucker.h"
#include "base/scoped_observation.h"
#include "ui/aura/window.h"

namespace ash {

class WindowDimmer;

// This controller manages the PiP window.
class ASH_EXPORT PipController : public aura::WindowObserver {
 public:
  PipController();
  PipController(const PipController&) = delete;
  PipController& operator=(const PipController&) = delete;
  ~PipController() override;

  // Returns the PiP window that this controller is managing.
  aura::Window* pip_window() { return pip_window_; }

  // Returns whether the PiP window is tucked.
  bool is_tucked() const { return is_tucked_; }

  // Set the target window for this controller.
  void SetPipWindow(aura::Window* window);

  // Remove the target window from this controller.
  void UnsetPipWindow(aura::Window* window);

  // Check if PiP has valid size constraints for resizing.
  bool CanResizePip();

  // Updates the PiP bounds if necessary. This may need to happen when the
  // display work area changes, or if system ui regions like the virtual
  // keyboard position changes.
  void UpdatePipBounds();

  void TuckWindow(bool left);
  void OnUntuckAnimationEnded();
  void UntuckWindow();
  views::Widget* GetTuckHandleWidget();
  void SetDimOpacity(float opacity);

  bool HandleDoubleTap(const ui::Event& event);
  bool HandleKeyboardShortcut();

  // aura::WindowObserver:
  void OnWindowDestroying(aura::Window* window) override;

 private:
  class PipSizeSwitchHandler {
     public:
      // Do not call these Process~() functions directly.
      // These should be called using PipController::Handle~() functions.
      bool ProcessDoubleTapEvent(const ui::Event& event);
      bool ProcessShortcutEvent(aura::Window* pip_window);

     private:
      bool ResizePip(WindowState* window_state);
      gfx::Rect prev_bounds_;
  };

  friend class PipControllerTestAPI;

  // The `pip_window` this controller is managing.
  raw_ptr<aura::Window> pip_window_;

  // True if `pip_window` is tucked. False during construction.
  bool is_tucked_ = false;

  // Scoped object that handles the special tucked window state, which is not
  // a normal window state. Null when `pip_window` is not tucked.
  std::unique_ptr<ScopedWindowTucker> scoped_window_tucker_;

  // Window dimmer that is used to dim the PiP window during the tuck.
  std::unique_ptr<WindowDimmer> dimmer_;

  base::ScopedObservation<aura::Window, aura::WindowObserver>
      pip_window_observation_{this};

  PipSizeSwitchHandler pip_size_switch_handler_;

  base::WeakPtrFactory<PipController> weak_ptr_factory_{this};
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_CONTROLLER_H_
