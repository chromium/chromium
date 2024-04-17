// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_WINDOW_RESIZER_H_
#define ASH_WM_DEFAULT_WINDOW_RESIZER_H_

#include <memory>

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"

namespace ash {

// WindowResizer is used by ToplevelWindowEventHandler to handle dragging,
// moving or resizing a window. All coordinates passed to this are in the parent
// windows coordinates.
class ASH_EXPORT DefaultWindowResizer : public WindowResizer {
 public:
  ~DefaultWindowResizer() override;

  // Creates a new DefaultWindowResizer.
  static std::unique_ptr<DefaultWindowResizer> Create(
      WindowState* window_state);

  // WindowResizer:
  void Drag(const gfx::PointF& location, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  explicit DefaultWindowResizer(WindowState* window_state);
  DefaultWindowResizer(const DefaultWindowResizer&) = delete;
  DefaultWindowResizer& operator=(const DefaultWindowResizer&) = delete;

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_ = false;
};

}  // namespace ash

#endif  // ASH_WM_DEFAULT_WINDOW_RESIZER_H_
