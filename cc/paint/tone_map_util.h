// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TONE_MAP_UTIL_H_
#define CC_PAINT_TONE_MAP_UTIL_H_

#include <optional>

#include "cc/paint/paint_export.h"

class SkColorSpace;
class SkImage;
class SkPaint;

namespace gfx {
struct HDRMetadata;
}  // namespace gfx

namespace cc {

class PaintImage;

// Helper class for applying tone mapping on the fly in DrawImage and
// DrawImageRect.
class CC_PAINT_EXPORT ToneMapUtil {
 public:
  // Return true if the specified PaintImage should be drawn using a gainmap
  // shader.
  static bool UseGainmapShader(const PaintImage& image);

  // Return true if the specified SkImage should be drawn using a tone mapping
  // shader. The `dst_color_space` parameter is used only as a workaround to
  // disable tone mapping (see comments in the source).
  static bool UseGlobalToneMapFilter(
      const SkImage* image,
      const SkColorSpace* dst_color_space = nullptr);

  // Return true if images that have the specified color space should be drawn
  // using a tone mapping shader.
  static bool UseGlobalToneMapFilter(const SkColorSpace* cs);

  // Add a color filter to `paint` that will perform tone mapping.
  static void AddGlobalToneMapFilterToPaint(
      SkPaint& paint,
      const SkImage* image,
      const std::optional<gfx::HDRMetadata>& metadata,
      float target_linear_hdr_headroom);
};

}  // namespace cc

#endif  // CC_PAINT_TONE_MAP_UTIL_H_
