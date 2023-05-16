// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_
#define ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_

#include <set>

#include "ash/ash_export.h"
#include "ash/capture_mode/capture_mode_types.h"
#include "base/memory/raw_ptr.h"
#include "ui/aura/window_observer.h"
#include "ui/base/cursor/cursor.h"
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
                                         public ::wm::ActivationChangeObserver {
 public:
  explicit CaptureWindowObserver(CaptureModeSession* capture_mode_session);
  CaptureWindowObserver(const CaptureWindowObserver&) = delete;
  CaptureWindowObserver& operator=(const CaptureWindowObserver&) = delete;

  ~CaptureWindowObserver() override;

  // Updates selected window depending on the mouse/touch event location. If
  // there is an eligible window under the current mouse/touch event location,
  // its bounds will be highlighted.
  void UpdateSelectedWindowAtPosition(const gfx::Point& location_in_screen);

  // Sets the given `window` as the current observed `window_`. `window` will be
  // ignored if it's a child of the wallpaper container or it's the home
  // launcher window. If `allow_window_change` is false, `window_` will not be
  // allowed to be altered throughout the entire capture mode session.
  void SetSelectedWindow(aura::Window* window, bool allow_window_change = true);

  // aura::WindowObserver:
  void OnWindowBoundsChanged(aura::Window* window,
                             const gfx::Rect& old_bounds,
                             const gfx::Rect& new_bounds,
                             ui::PropertyChangeReason reason) override;
  void OnWindowVisibilityChanging(aura::Window* window, bool visible) override;
  void OnWindowDestroying(aura::Window* window) override;

  // ::wm::ActivationChangeObserver:
  void OnWindowActivated(ActivationReason reason,
                         aura::Window* gained_active,
                         aura::Window* lost_active) override;

  aura::Window* window() { return window_; }

 private:
  void StartObserving(aura::Window* window);
  void StopObserving();

  // Repaints the window capture region.
  void RepaintCaptureRegion();

  // Current observed window.
  raw_ptr<aura::Window, ExperimentalAsh> window_ = nullptr;

  // If false, `window_` is not allowed to be changed throughout the capture
  // mode session once set.
  bool allow_window_change_ = true;

  // Stores current mouse or touch location in screen coordinate.
  gfx::Point location_in_screen_;

  // Pointer to current capture session. Not nullptr during this lifecycle.
  const raw_ptr<CaptureModeSession, ExperimentalAsh> capture_mode_session_;
};

}  // namespace ash

#endif  // ASH_CAPTURE_MODE_CAPTURE_WINDOW_OBSERVER_H_
