// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RENDER_SURFACE_FILTERS_H_
#define CC_PAINT_RENDER_SURFACE_FILTERS_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/vector2d_f.h"

namespace gfx {
class SizeF;
}

namespace cc {
class PaintFilter;
class FilterOperations;

class CC_PAINT_EXPORT RenderSurfaceFilters {
 public:
  RenderSurfaceFilters() = delete;

  static sk_sp<PaintFilter> BuildImageFilter(
      const FilterOperations& filters,
      const gfx::SizeF& size,
      const gfx::Vector2dF& offset = gfx::Vector2dF(0, 0));
};

}  // namespace cc

#endif  // CC_PAINT_RENDER_SURFACE_FILTERS_H_
