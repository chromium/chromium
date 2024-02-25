// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DIRECTLY_COMPOSITED_IMAGE_INFO_H_
#define CC_PAINT_DIRECTLY_COMPOSITED_IMAGE_INFO_H_

#include "ui/gfx/geometry/vector2d.h"

namespace cc {

// An image is directly composited if it's the only content of a layer.
// It'll be rasterized at the intrinsic size.
struct DirectlyCompositedImageInfo {
  // See PictureLayerImpl::direct_composited_image_default_raster_scale_.
  gfx::Vector2dF default_raster_scale;
  // Whether to use nearest neighbor filtering when scaling the layer.
  bool nearest_neighbor;
};

}  // namespace cc

#endif  // CC_PAINT_DIRECTLY_COMPOSITED_IMAGE_INFO_H_
