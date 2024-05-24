// Copyright 2024 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/record_replay.h"
#include "base/record_replay_paint_surface.h"

// We use this file to allow depending on record_replay_render from blink.
// We cannot access the render code directly since, during V8 snapshotting,
// blink is linked, but the display service is not.

namespace recordreplay {

// Callback to reset the paint surface.
static ResetPaintSurfaceCallback gResetPaintSurfaceCallback = nullptr;
void SetResetPaintSurfaceCallback(ResetPaintSurfaceCallback reset_paint_surface) {
  gResetPaintSurfaceCallback = reset_paint_surface;
}
void DoResetPaintSurface() {
  if (gResetPaintSurfaceCallback) {
    gResetPaintSurfaceCallback();
  }
}

}  // namespace recordreplay
