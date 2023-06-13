// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RASTER_CAPABILITIES_H_
#define CC_TREES_RASTER_CAPABILITIES_H_

#include "cc/cc_export.h"

namespace cc {

struct CC_EXPORT RasterCapabilities {
  RasterCapabilities() = default;
  RasterCapabilities(const RasterCapabilities& other) = delete;
  RasterCapabilities& operator=(const RasterCapabilities& other) = delete;
  ~RasterCapabilities() = default;

  bool need_update_gpu_rasterization_status = false;
  bool use_gpu_rasterization = false;

  bool can_use_msaa = false;
  bool use_dmsaa_for_tiles = false;

  // The maximum size (either width or height) that any texture can be. Also
  // holds a reasonable value for software compositing bitmaps.
  int max_texture_size = 0;
};
}  // namespace cc

#endif  // CC_TREES_RASTER_CAPABILITIES_H_
