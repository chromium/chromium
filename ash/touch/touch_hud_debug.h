// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TOUCH_TOUCH_HUD_DEBUG_H_
#define ASH_TOUCH_TOUCH_HUD_DEBUG_H_

#include <stdint.h>

#include <map>
#include <memory>

#include "ash/ash_export.h"
#include "ash/touch/touch_observer_hud.h"
#include "base/memory/raw_ptr.h"

namespace views {
class Label;
class View;
}  // namespace views

namespace ash {
class TouchHudCanvas;
class TouchLog;

// A heads-up display to show touch traces on the screen and log touch events.
// As a derivative of TouchObserverHud, objects of this class manage their own
// lifetime.
class ASH_EXPORT TouchHudDebug : public TouchObserverHud {
 public:
  enum Mode {
    FULLSCREEN,
    REDUCED_SCALE,
    INVISIBLE,
  };

  explicit TouchHudDebug(aura::Window* initial_root);

  TouchHudDebug(const TouchHudDebug&) = delete;
  TouchHudDebug& operator=(const TouchHudDebug&) = delete;

  // Changes the display mode (e.g. scale, visibility). Calling this repeatedly
  // cycles between a fixed number of display modes.
  void ChangeToNextMode();

  Mode mode() const { return mode_; }

  // TouchObserverHud:
  void Clear() override;

 private:
  ~TouchHudDebug() override;

  void SetMode(Mode mode);

  void UpdateTouchPointLabel(int index);

  // TouchObserverHud:
  void OnTouchEvent(ui::TouchEvent* event) override;
  void OnDisplayMetricsChanged(const display::Display& display,
                               uint32_t metrics) override;
  void SetHudForRootWindowController(RootWindowController* controller) override;
  void UnsetHudForRootWindowController(
      RootWindowController* controller) override;

  static const int kMaxTouchPoints = 32;

  Mode mode_;

  std::unique_ptr<TouchLog> touch_log_;

  raw_ptr<TouchHudCanvas> canvas_;
  raw_ptr<views::View> label_container_;
  views::Label* touch_labels_[kMaxTouchPoints];
};

}  // namespace ash

#endif  // ASH_TOUCH_TOUCH_HUD_DEBUG_H_
