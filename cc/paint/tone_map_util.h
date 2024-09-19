// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TONE_MAP_UTIL_H_
#define CC_PAINT_TONE_MAP_UTIL_H_

#include "cc/paint/paint_export.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPaint.h"

namespace cc {

class PaintImage;

// Helper class for applying tone mapping on the fly in DrawImage and
// DrawImageRect.
class CC_PAINT_EXPORT ToneMapUtil {
 public:
  // Return true if the specified PaintImage should be drawn using a gainmap
  // shader.
  static bool UseGainmapShader(const PaintImage& image);

  // Return true if the specified PaintImage should be drawn using a tone
  // mapping shader.
  static bool UseGlobalToneMapFilter(const PaintImage& image);

  // Return true if images that have the specified color space should be drawn
  // using a tone mapping shader.
  static bool UseGlobalToneMapFilter(const SkColorSpace* cs);

  // Add a color filter to `paint` that will perform tone mapping.
  static void AddGlobalToneMapFilterToPaint(
      SkPaint& paint,
      const PaintImage& image,
      sk_sp<SkColorSpace> dst_color_space);
};

}  // namespace cc

#endif  // CC_PAINT_TONE_MAP_UTIL_H_
