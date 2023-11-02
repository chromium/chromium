// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_HUD_PROJECTION_H_
#define ASH_TOUCH_TOUCH_HUD_PROJECTION_H_

#include "ash/touch/touch_observer_hud.h"

namespace ash {
class TouchHudRenderer;

// A heads-up display to show active touch points on the screen. As a derivative
// of TouchObserverHud, objects of this class manage their own lifetime. Used
// for the --show-taps flag.
class TouchHudProjection : public TouchObserverHud {
 public:
  explicit TouchHudProjection(aura::Window* initial_root);

  TouchHudProjection(const TouchHudProjection&) = delete;
  TouchHudProjection& operator=(const TouchHudProjection&) = delete;

  // TouchObserverHud:
  void Clear() override;

 private:
  friend class TouchHudProjectionTest;

  ~TouchHudProjection() override;

  // TouchObserverHud:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void SetHudForRootWindowController(RootWindowController* controller) override;
  void UnsetHudForRootWindowController(
      RootWindowController* controller) override;

  // TouchHudRenderer draws out the touch points.
  std::unique_ptr<TouchHudRenderer> touch_hud_renderer_;
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_HUD_PROJECTION_H_
