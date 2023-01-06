// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_
#define ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_

#include <memory>

#include "ash/ash_export.h"
#include "base/functional/bind.h"
#include "ui/events/event.h"

namespace aura {
class Window;
}

namespace ash {
class DragDropTrackerDelegate;

using CancelDragDropCallback = base::RepeatingCallback<void(void)>;

// Provides functions for handling drag events inside and outside the root
// window where drag is started. This internally sets up a capture window for
// tracking drag events outside the root window where drag is initiated.
// Only X11 environment is supported for now.
class ASH_EXPORT DragDropTracker {
 public:
  DragDropTracker(aura::Window* context_root, CancelDragDropCallback callback);

  DragDropTracker(const DragDropTracker&) = delete;
  DragDropTracker& operator=(const DragDropTracker&) = delete;

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
  std::unique_ptr<ui::LocatedEvent> ConvertEvent(aura::Window* target,
                                                 const ui::LocatedEvent& event);

 private:
  std::unique_ptr<ash::DragDropTrackerDelegate> tracker_window_delegate_;
  // A window for capturing drag events while dragging.
  std::unique_ptr<aura::Window> capture_window_;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_DRAG_DROP_TRACKER_H_
