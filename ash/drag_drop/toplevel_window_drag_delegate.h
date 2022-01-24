// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
#define ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_

#include "base/bind.h"
#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-forward.h"

namespace aura {
class Window;
}

namespace gfx {
class PointF;
}

namespace ui {
class LocatedEvent;
}

namespace ash {

// Interface that makes it possible to implement toplevel window drag handling
// during Drag & Drop sessions.
class ToplevelWindowDragDelegate {
 public:
  virtual void OnToplevelWindowDragStarted(const gfx::PointF& start_location,
                                           ui::mojom::DragEventSource source,
                                           aura::Window* source_window) = 0;

  virtual ui::mojom::DragOperation OnToplevelWindowDragDropped() = 0;

  virtual void OnToplevelWindowDragCancelled() = 0;

  virtual void OnToplevelWindowDragEvent(ui::LocatedEvent* event) = 0;

  using CancelDragDropCallback = base::RepeatingCallback<void(void)>;

  // Conditionally takes capture of top level touch events, returning whether
  // this was successful.
  virtual bool TakeCapture(aura::Window* root_window,
                           aura::Window* source_window,
                           CancelDragDropCallback callback) = 0;

  // Converts an event target that was dispatched against a capture window to
  // once that can be processed by the drag and drop controller.
  //
  // This should only be called on events if TakeCapture returned true at the
  // start of a drag and drop session.
  virtual aura::Window* GetTarget(const ui::LocatedEvent& event) = 0;

  // Converts an event that was dispatched against a capture window to once
  // that can be processed by the drag and drop controller, using the target
  // returned via GetTarget.
  //
  // This should only be called on events if TakeCapture returned true at the
  // start of a drag and drop session.
  virtual ui::LocatedEvent* ConvertEvent(aura::Window* target,
                                         const ui::LocatedEvent& event) = 0;

  // Return the capture window used if TakeCapture returns true.
  virtual aura::Window* capture_window() = 0;

 protected:
  virtual ~ToplevelWindowDragDelegate() = default;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
