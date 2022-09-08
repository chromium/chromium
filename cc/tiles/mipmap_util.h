// Copyright 2016 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TILES_MIPMAP_UTIL_H_
#define CC_TILES_MIPMAP_UTIL_H_

#include "cc/cc_export.h"
#include "third_party/skia/include/core/SkSize.h"
#include "ui/gfx/geometry/skia_conversions.h"

namespace cc {

class CC_EXPORT MipMapUtil {
 public:
  // Determine the smallest mip level that is larger than |target_size|. Each
  // mip level corresponds to a power of two scale of the image - for instance,
  // level 0 is original size, level 1 is 2x smaller, level 2 is 4x smaller,
  // etc... This function does not do error checking and must be called with a
  // valid src_size (width/height > 0) and mip_level (>= 0).
  static int GetLevelForSize(const gfx::Size& src_size,
                             const gfx::Size& target_size);

  // Determines the scale factor for the given |mip_level|. This function does
  // not do error checking and must be called with a valid src_size
  // (width/height > 0) and mip_level (>= 0).
  static SkSize GetScaleAdjustmentForLevel(const gfx::Size& src_size,
                                           int mip_level);

  // Determine the size of the given |mip_level|. This function does not do
  // error checking and must be called with a valid src_size (width/height > 0)
  // and mip_level (>= 0).
  static gfx::Size GetSizeForLevel(const gfx::Size& src_size, int mip_level);

  // Determines the scale factor for the smallest mip level that is larger than
  // |target_size|. This function does not do error checking and must be called
  // with a valid src_size (width/height > 0) and mip_level (>= 0).
  static SkSize GetScaleAdjustmentForSize(const gfx::Size& src_size,
                                          const gfx::Size& target_size);
};

}  // namespace cc

#endif  // CC_TILES_MIPMAP_UTIL_H_
