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
class ColorSpace;
struct HDRMetadata;
}  // namespace gfx

namespace cc {

// Helper class for applying tone mapping on the fly in DrawImage and
// DrawImageRect.
class CC_PAINT_EXPORT ToneMapUtil {
 public:
  // Return true if images that have the specified color space should be drawn
  // using a tone mapping shader. If `targeted_hdr_headroom` is specified, then
  // determine if the filter is required when drawing at the specified HDR
  // headroom.
  static bool UseGlobalToneMapFilter(
      const SkColorSpace* cs,
      const gfx::HDRMetadata& metadata,
      std::optional<float> targeted_hdr_headroom = std::nullopt);
  static bool UseGlobalToneMapFilter(
      const gfx::ColorSpace& cs,
      const gfx::HDRMetadata& metadata,
      std::optional<float> targeted_hdr_headroom = std::nullopt);

  // Return the maximum HDR headroom that this content will render to.
  static float GetMaxHdrHeadroom(const SkColorSpace* cs,
                                 const gfx::HDRMetadata& metadata);
  static float GetMaxHdrHeadroom(const gfx::ColorSpace& cs,
                                 const gfx::HDRMetadata& metadata);

  // Return true if this content will render with HDR headroom greater than 0.
  static bool IsHDR(const SkColorSpace* cs, const gfx::HDRMetadata& metadata);
  static bool IsHDR(const gfx::ColorSpace& cs,
                    const gfx::HDRMetadata& metadata);

  // Add a color filter to `paint` that will perform tone mapping.
  static void AddGlobalToneMapFilterToPaint(SkPaint& paint,
                                            const SkImage* image,
                                            const gfx::HDRMetadata& metadata,
                                            float target_hdr_headroom);
};

}  // namespace cc

#endif  // CC_PAINT_TONE_MAP_UTIL_H_
