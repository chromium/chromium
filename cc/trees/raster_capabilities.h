// Copyright 2023 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TREES_RASTER_CAPABILITIES_H_
#define CC_TREES_RASTER_CAPABILITIES_H_

#include "cc/cc_export.h"
#include "components/viz/common/resources/shared_image_format.h"

namespace cc {

struct CC_EXPORT RasterCapabilities {
  RasterCapabilities();
  RasterCapabilities(const RasterCapabilities& other);
  RasterCapabilities& operator=(const RasterCapabilities& other);
  ~RasterCapabilities();

  bool use_gpu_rasterization = false;

  bool can_use_msaa = false;

  // The maximum size (either width or height) that any texture can be. Also
  // holds a reasonable value for software compositing bitmaps.
  int max_texture_size = 0;

  // Format used to allocate tiles.
  viz::SharedImageFormat tile_format = viz::SinglePlaneFormat::kRGBA_8888;

  // If tile textures are overlay candidates.
  bool tile_overlay_candidate = false;

  // Format used to allocate RGBA8 UI resources.
  viz::SharedImageFormat ui_rgba_format = viz::SinglePlaneFormat::kRGBA_8888;
};

}  // namespace cc

#endif  // CC_TREES_RASTER_CAPABILITIES_H_
