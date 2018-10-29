// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_PIP_PIP_WINDOW_RESIZER_H_
#define ASH_WM_PIP_PIP_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"
#include "base/macros.h"
#include "ui/display/display.h"
#include "ui/gfx/geometry/rect.h"

namespace ash {

namespace wm {
class WindowState;
}  // namespace wm

// Controls resizing for windows with the PIP window state type. This
// includes things like snapping the PIP window to the edges of the work area
// and handling swipe-to-dismiss.
class ASH_EXPORT PipWindowResizer : public WindowResizer {
 public:
  explicit PipWindowResizer(wm::WindowState* window_state);
  ~PipWindowResizer() override;

  // WindowResizer:
  void Drag(const gfx::Point& location_in_parent, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  wm::WindowState* window_state() { return window_state_; }

  gfx::Point last_location_in_screen_;
  bool moved_or_resized_ = false;

  DISALLOW_COPY_AND_ASSIGN(PipWindowResizer);
};

}  // namespace ash

#endif  // ASH_WM_PIP_PIP_WINDOW_RESIZER_H_
