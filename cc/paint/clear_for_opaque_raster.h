// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_CLEAR_FOR_OPAQUE_RASTER_H_
#define CC_PAINT_CLEAR_FOR_OPAQUE_RASTER_H_

#include "cc/paint/paint_export.h"

namespace gfx {
class Rect;
class Size;
class Vector2dF;
}  // namespace gfx

namespace cc {

// Called when we are drawing opaque content with |translation| and |scale|.
// Calculates |outer_rect| and |inner_rect| between which the drawn content
// would not be opaque due to |translation| and/or |scale| and should be cleared
// with an opaque color before drawing the original contents, to ensure all
// texels are fully opaque. The output rects are in the device space.
// Returns false if no clearing for opaque is needed.
bool CC_PAINT_EXPORT
CalculateClearForOpaqueRasterRects(const gfx::Vector2dF& translation,
                                   const gfx::Vector2dF& scale,
                                   const gfx::Size& content_size,
                                   const gfx::Rect& canvas_bitmap_rect,
                                   const gfx::Rect& canvas_playback_rect,
                                   gfx::Rect& outer_rect,
                                   gfx::Rect& inner_rect);

}  // namespace cc

#endif  // CC_PAINT_CLEAR_FOR_OPAQUE_RASTER_H_
