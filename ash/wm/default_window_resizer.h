// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_WM_DEFAULT_WINDOW_RESIZER_H_
#define ASH_WM_DEFAULT_WINDOW_RESIZER_H_

#include "ash/ash_export.h"
#include "ash/wm/window_resizer.h"
#include "base/macros.h"

namespace ash {

// WindowResizer is used by ToplevelWindowEventFilter to handle dragging, moving
// or resizing a window. All coordinates passed to this are in the parent
// windows coordiantes.
class ASH_EXPORT DefaultWindowResizer : public WindowResizer {
 public:
  ~DefaultWindowResizer() override;

  // Creates a new DefaultWindowResizer. The caller takes ownership of the
  // returned object.
  static DefaultWindowResizer* Create(WindowState* window_state);

  // Returns true if the drag will result in changing the window in anyway.
  bool is_resizable() const { return details().is_resizable; }

  bool changed_size() const {
    return !(details().bounds_change & kBoundsChange_Repositions);
  }

  // WindowResizer:
  void Drag(const gfx::Point& location, int event_flags) override;
  void CompleteDrag() override;
  void RevertDrag() override;
  void FlingOrSwipe(ui::GestureEvent* event) override;

 private:
  explicit DefaultWindowResizer(WindowState* window_state);

  // Set to true once Drag() is invoked and the bounds of the window change.
  bool did_move_or_resize_;

  DISALLOW_COPY_AND_ASSIGN(DefaultWindowResizer);
};

}  // namespace aura

#endif  // ASH_WM_DEFAULT_WINDOW_RESIZER_H_
