// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
#define ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_

#include "ash/drag_drop/drag_drop_capture_delegate.h"
#include "base/functional/bind.h"
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
class ToplevelWindowDragDelegate : public DragDropCaptureDelegate {
 public:
  virtual void OnToplevelWindowDragStarted(const gfx::PointF& start_location,
                                           ui::mojom::DragEventSource source,
                                           aura::Window* source_window) = 0;

  virtual ui::mojom::DragOperation OnToplevelWindowDragDropped() = 0;

  virtual void OnToplevelWindowDragCancelled() = 0;

  virtual void OnToplevelWindowDragEvent(ui::LocatedEvent* event) = 0;

 protected:
  ~ToplevelWindowDragDelegate() override = default;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
