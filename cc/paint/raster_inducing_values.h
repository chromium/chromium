// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_RASTER_INDUCING_VALUES_H_
#define CC_PAINT_RASTER_INDUCING_VALUES_H_

#include "cc/paint/scroll_offset_map.h"

namespace cc {

// The values that raster applies to a recording. Raster tasks and picture quads
// own a copy and pass a pointer to playback and serialization.
struct RasterInducingValues {
  bool operator==(const RasterInducingValues&) const = default;

  ScrollOffsetMap scroll_offsets;
};

}  // namespace cc

#endif  // CC_PAINT_RASTER_INDUCING_VALUES_H_
