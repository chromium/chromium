// Copyright 2012 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RENDER_SURFACE_FILTERS_H_
#define CC_PAINT_RENDER_SURFACE_FILTERS_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkRefCnt.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {
class PaintFilter;
class FilterOperations;

class CC_PAINT_EXPORT RenderSurfaceFilters {
 public:
  RenderSurfaceFilters() = delete;

  // `layer_bounds` is only used for backdrop filters that reference ZOOM
  static sk_sp<PaintFilter> BuildImageFilter(
      const FilterOperations& filters,
      const gfx::Rect& layer_bounds = {});
};

}  // namespace cc

#endif  // CC_PAINT_RENDER_SURFACE_FILTERS_H_
