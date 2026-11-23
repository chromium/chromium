// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RASTER_INDUCING_VALUES_H_
#define CC_PAINT_RASTER_INDUCING_VALUES_H_

#include "cc/paint/deferred_value_map.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/scroll_offset_map.h"

namespace cc {

// The values that raster applies to a recording. Raster tasks and picture quads
// own a copy and pass a pointer to playback and serialization.
struct CC_PAINT_EXPORT RasterInducingValues {
  // Returns a shared empty instance.
  static const RasterInducingValues& Empty();

  RasterInducingValues();
  RasterInducingValues(ScrollOffsetMap scroll_offsets,
                       DeferredValueMap compositor_animated_values);
  RasterInducingValues(const RasterInducingValues&);
  RasterInducingValues& operator=(const RasterInducingValues&);
  RasterInducingValues(RasterInducingValues&&) noexcept;
  RasterInducingValues& operator=(RasterInducingValues&&) noexcept;
  ~RasterInducingValues();

  bool operator==(const RasterInducingValues&) const = default;

  ScrollOffsetMap scroll_offsets;
  DeferredValueMap compositor_animated_values;
};

}  // namespace cc

#endif  // CC_PAINT_RASTER_INDUCING_VALUES_H_
