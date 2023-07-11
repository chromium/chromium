// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_DOUBLE_TAP_HANDLER_H_
#define ASH_WM_PIP_PIP_DOUBLE_TAP_HANDLER_H_

#include "ui/gfx/geometry/rect.h"

namespace ui {
class Event;
}  // namespace ui

namespace ash {

class WindowState;

// Handles double tap events for PiP windows.
// I.E. When double tapped: expands, shrinks.
class PipDoubleTapHandler {
 public:
  PipDoubleTapHandler();
  PipDoubleTapHandler(const PipDoubleTapHandler&) = delete;
  PipDoubleTapHandler& operator=(const PipDoubleTapHandler&) = delete;
  virtual ~PipDoubleTapHandler();

  // Called when a `MouseEvent` or `GestureEvent` is called.
  bool ProcessDoubleTapEvent(const ui::Event& event);

 private:
  // Called by P`rocessDoubleTapEvent`.
  // Expands/shrinks the PiP window when users double tap on a PiP window.
  bool ProcessDoubleTapEventImpl(const ui::Event& event,
                                 WindowState* window_state);

  gfx::Rect prev_bounds_;
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_DOUBLE_TAP_HANDLER_H_
