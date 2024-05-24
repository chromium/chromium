// Copyright 2024 Record Replay Inc. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.


#ifndef BASE_RECORD_REPLAY_PAINT_SURFACE_H_
#define BASE_RECORD_REPLAY_PAINT_SURFACE_H_


namespace recordreplay {

typedef void (*ResetPaintSurfaceCallback)();
void SetResetPaintSurfaceCallback(ResetPaintSurfaceCallback reset_paint_surface);
void DoResetPaintSurface();

}  // namespace recordreplay

#endif // BASE_RECORD_REPLAY_PAINT_SURFACE_H_
