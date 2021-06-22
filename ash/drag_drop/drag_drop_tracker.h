// Copyright (c) 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_
#define ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/macros.h"
#include "ui/events/event.h"

namespace aura {
class Window;
class WindowDelegate;
}

namespace ash {

// Provides functions for handling drag events inside and outside the root
// window where drag is started. This internally sets up a capture window for
// tracking drag events outside the root window where drag is initiated.
// Only X11 environment is supported for now.
class ASH_EXPORT DragDropTracker {
 public:
  DragDropTracker(aura::Window* context_root, aura::WindowDelegate* delegate);
  ~DragDropTracker();

  aura::Window* capture_window() { return capture_window_.get(); }

  // Tells our |capture_window_| to take capture. This is not done right at
  // creation to give the caller a chance to perform any operations needed
  // before the capture is transferred.
  void TakeCapture();

  // Gets the target located at |event| in the coordinates of the active root
  // window.
  aura::Window* GetTarget(const ui::LocatedEvent& event);

  // Converts the locations of |event| in the coordinates of the active root
  // window to the ones in |target|'s coordinates.
  // Caller takes ownership of the returned object.
  ui::LocatedEvent* ConvertEvent(aura::Window* target,
                                 const ui::LocatedEvent& event);

 private:
  // A window for capturing drag events while dragging.
  std::unique_ptr<aura::Window> capture_window_;

  DISALLOW_COPY_AND_ASSIGN(DragDropTracker);
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_
