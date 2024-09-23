// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_CAPTURE_DELEGATE_H_
#define ASH_DRAG_DROP_DRAG_DROP_CAPTURE_DELEGATE_H_

#include "ash/ash_export.h"
#include "base/functional/bind.h"
#include "ui/events/gestures/gesture_types.h"

namespace aura {
class Window;
}

namespace ui {
class LocatedEvent;
}

namespace ash {
class DragDropTracker;

class ASH_EXPORT DragDropCaptureDelegate {
 public:
  using CancelDragDropCallback = base::RepeatingCallback<void(void)>;

  DragDropCaptureDelegate();

  DragDropCaptureDelegate(const DragDropCaptureDelegate&) = delete;
  DragDropCaptureDelegate& operator=(const DragDropCaptureDelegate&) = delete;

  virtual ~DragDropCaptureDelegate();

  // Conditionally takes capture of top level touch events, returning whether
  // this was successful.
  bool TakeCapture(aura::Window* root_window,
                   aura::Window* source_window,
                   CancelDragDropCallback callback,
                   ui::TransferTouchesBehavior behavior);

  // Converts an event target that was dispatched against a capture window to
  // once that can be processed by the drag and drop controller.
  //
  // This should only be called on events if TakeCapture returned true at the
  // start of a drag and drop session. Returns nullptr after `ReleaseCapture`.
  aura::Window* GetTarget(const ui::LocatedEvent& event);

  // Converts an event that was dispatched against a capture window to once
  // that can be processed by the drag and drop controller, using the target
  // returned via GetTarget.
  //
  // This should only be called on events if TakeCapture returned true at the
  // start of a drag and drop session. Returns nullptr after `ReleaseCapture`.
  std::unique_ptr<ui::LocatedEvent> ConvertEvent(aura::Window* target,
                                                 const ui::LocatedEvent& event);

  // Return the capture window used if TakeCapture returns true. Returns nullptr
  // after `ReleaseCapture`.
  aura::Window* capture_window();

  // Stop capturing input events.
  void ReleaseCapture();

 private:
  std::unique_ptr<DragDropTracker> drag_drop_tracker_;
};

void DispatchGestureEndToWindow(aura::Window* window);

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_CAPTURE_DELEGATE_H_
