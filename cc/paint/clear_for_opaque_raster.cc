// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/clear_for_opaque_raster.h"

#include <cmath>

#include "base/check_op.h"
#include "ui/gfx/geometry/rect.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace cc {

bool CalculateClearForOpaqueRasterRects(const gfx::Vector2dF& translation,
                                        const gfx::Vector2dF& scale,
                                        const gfx::Size& content_size,
                                        const gfx::Rect& canvas_bitmap_rect,
                                        const gfx::Rect& canvas_playback_rect,
                                        gfx::Rect& outer_rect,
                                        gfx::Rect& inner_rect) {
  // If there is translation, the top and/or left texels are not guaranteed to
  // be fully opaque.
  DCHECK_GE(translation.x(), 0.0f);
  DCHECK_GE(translation.y(), 0.0f);
  DCHECK_LT(translation.x(), 1.0f);
  DCHECK_LT(translation.y(), 1.0f);
  bool left_opaque = translation.x() == 0.0f;
  bool top_opaque = translation.y() == 0.0f;
  // If there is scale, the right and/or bottom texels are not guaranteed to be
  // fully opaque.
  bool right_opaque = scale.x() == 1.0f;
  bool bottom_opaque = scale.y() == 1.0f;
  if (left_opaque && top_opaque && right_opaque && bottom_opaque)
    return false;

  // |outer_rect| is the bounds of all texels affected by content.
  outer_rect = gfx::Rect(content_size);
  // |inner_rect| is the opaque coverage of the content.
  inner_rect = outer_rect;
  // If not fully covered, one texel inside the content rect may not be opaque
  // (because of blending during raster) and, for scale, one texel outside
  // (because of bilinear filtering during draw) may not be opaque.
  outer_rect.Inset(
      gfx::Insets::TLBR(0, 0, bottom_opaque ? 0 : -1, right_opaque ? 0 : -1));
  inner_rect.Inset(gfx::Insets::TLBR(top_opaque ? 0 : 1, left_opaque ? 0 : 1,
                                     bottom_opaque ? 0 : 1,
                                     right_opaque ? 0 : 1));

  // If the playback rect is touching either edge of the content rect, extend it
  // by one to include the extra texel outside that was added to outer_rect
  // above.
  bool touches_left_edge = !left_opaque && !canvas_playback_rect.x();
  bool touches_top_edge = !top_opaque && !canvas_playback_rect.y();
  bool touches_right_edge =
      !right_opaque && content_size.width() == canvas_playback_rect.right();
  bool touches_bottom_edge =
      !bottom_opaque && content_size.height() == canvas_playback_rect.bottom();
  gfx::Rect adjusted_playback_rect = canvas_playback_rect;
  adjusted_playback_rect.Inset(gfx::Insets::TLBR(
      touches_top_edge ? -1 : 0, touches_left_edge ? -1 : 0,
      touches_bottom_edge ? -1 : 0, touches_right_edge ? -1 : 0));

  // No need to clear if the playback area is fully covered by the opaque
  // content.
  if (inner_rect.Contains(adjusted_playback_rect))
    return false;

  if (!outer_rect.Intersects(adjusted_playback_rect))
    return false;

  outer_rect.Intersect(adjusted_playback_rect);
  inner_rect.Intersect(adjusted_playback_rect);
  // inner_rect can be empty if the content is very small.

  // Move the rects into the device space.
  outer_rect.Offset(-canvas_bitmap_rect.OffsetFromOrigin());
  inner_rect.Offset(-canvas_bitmap_rect.OffsetFromOrigin());
  return inner_rect != outer_rect;
}

}  // namespace cc
