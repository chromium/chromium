// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_
#define ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_

#include "ash/ash_export.h"
#include "ash/wm/desks/desks_controller.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/gfx/geometry/point.h"
#include "ui/gfx/native_widget_types.h"
#include "ui/wm/public/activation_change_observer.h"

namespace aura {
class Window;
}  // namespace aura

namespace ash {

class CaptureModeSession;

// Class to observe the current selected to-be-captured window.
class ASH_EXPORT CaptureWindowObserver : public aura::WindowObserver,
                                         public ::wm::ActivationChangeObserver,
                                         public DesksController::Observer {
 public:
  explicit CaptureWindowObserver(CaptureModeSession* capture_mode_session);
  CaptureWindowObserver(const CaptureWindowObserver&) = delete;
  CaptureWindowObserver& operator=(const CaptureWindowObserver&) = delete;

  ~CaptureWindowObserver() override;

  aura::Window* window() { return window_; }
  bool bar_anchored_to_window() const { return bar_anchored_to_window_; }

  // Updates selected window depending on the mouse/touch event location. If
  // there is an eligible window under the current mouse/touch event location,
  // its bounds will be highlighted.
  void UpdateSelectedWindowAtPosition(const gfx::Point& location_in_screen);

  // Sets the given `window` as the current observed `window_`. `window` will be
  // ignored if it's a child of the wallpaper container or it's the home
  // launcher window.
  // If `a11y_alert_again` is true, the a11y alert that announces the selected
  // window will be triggered again even if `window` is the same as the
  // currently selected window.
  // If `bar_anchored_to_window` is true, the capture bar will
  // be anchored to `window_` and it will not be allowed to be altered through
  // the entire capture mode session.
  void SetSelectedWindow(aura::Window* window,
                         bool a11y_alert_again,
                         bool bar_anchored_to_window);

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowParentChanged(aura::Window* window,
                             aura::Window* parent) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  // DesksController::Observer:
  void OnDeskActivationChanged(const Desk* activated,
                               const Desk* deactivated) override;

 private:
  void StartObserving(aura::Window* window);
  void StopObserving();

  // Repaints the window capture region.
  void RepaintCaptureRegion();

  // Current observed window.
  raw_ptr<aura::Window> window_ = nullptr;

  // If true, the capture bar will be anchored to the selected window instead of
  // the bottom of the display by default. And the selected window is not
  // allowed to be changed throughout the session. This is set to true for the
  // game capture session for now.
  bool bar_anchored_to_window_ = false;

  // Stores current mouse or touch location in screen coordinate.
  gfx::Point location_in_screen_;

  // Pointer to current capture session. Not nullptr during this lifecycle.
  const raw_ptr<CaptureModeSession> capture_mode_session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_
