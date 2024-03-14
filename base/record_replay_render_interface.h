// Copyright 2024 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_RECORD_REPLAY_RENDER_INTERFACE_H_
#define BASE_RECORD_REPLAY_RENDER_INTERFACE_H_


#include "ui/gfx/geometry/size.h"

using gfx::Size;

namespace recordreplay {

typedef void (*ResetPaintSurfaceCallback)();
void SetResetPaintSurfaceCallback(ResetPaintSurfaceCallback reset_paint_surface);
void DoResetPaintSurface();

typedef gfx::Size (*GetCurrentViewportPixelSizeCallback)();
// This callback is set when the first render surface is created.
void SetGetCurrentViewportPixelSizeCallback(GetCurrentViewportPixelSizeCallback cb);
// Get the current surface size in pixels.
gfx::Size GetCurrentViewportPixelSize();


}  // namespace recordreplay

#endif // BASE_RECORD_REPLAY_RENDER_INTERFACE_H_
