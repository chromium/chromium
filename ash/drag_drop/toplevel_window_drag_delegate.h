// Copyright 2020 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
#define ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_

#include "ui/base/dragdrop/mojom/drag_drop_types.mojom-shared.h"

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
  virtual void OnToplevelWindowDragStarted(
      const gfx::PointF& start_location,
      ui::mojom::DragEventSource source) = 0;

  virtual int OnToplevelWindowDragDropped() = 0;

  virtual void OnToplevelWindowDragCancelled() = 0;

  virtual void OnToplevelWindowDragEvent(ui::LocatedEvent* event) = 0;

 protected:
  virtual ~ToplevelWindowDragDelegate() = default;
};

}  // namespace ash

#endif  // ASH_DRAG_DROP_TOPLEVEL_WINDOW_DRAG_DELEGATE_H_
