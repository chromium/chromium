// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_TARGET_COLOR_PARAMS_H_
#define CC_PAINT_TARGET_COLOR_PARAMS_H_

#include <optional>
#include <string>

#include "cc/paint/paint_export.h"
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {

// Color parameters for a target for rasterization.
struct CC_PAINT_EXPORT TargetColorParams {
  TargetColorParams() = default;
  TargetColorParams(const TargetColorParams&) = default;
  TargetColorParams& operator=(const TargetColorParams&) = default;
  ~TargetColorParams() = default;

  // Constructor to use in tests to specify just a color space.
  explicit TargetColorParams(const gfx::ColorSpace& color_space)
      : color_space(color_space) {}

  // The target buffer's color space.
  gfx::ColorSpace color_space = gfx::ColorSpace::CreateSRGB();

  float GetHdrHeadroom() const { return hdr_headroom.value_or(0); }
  std::optional<float> hdr_headroom;

  // If false, then an ImageDecodeCache will store the images that it decodes
  // for this target using the SkColorType that the cache was constructed with,
  // even if that reduces the precision of the image. If true, then it will not
  // store an image at a lower precision than the SkColorType that most
  // naturally represents it (e.g, high bit depth images will be stored as
  // kRGBA_F16_SkColorType).
  bool preserve_image_precision = false;

  bool operator==(const TargetColorParams& other) const {
    return color_space == other.color_space &&
           hdr_headroom == other.hdr_headroom &&
           preserve_image_precision == other.preserve_image_precision;
  }
  bool operator!=(const TargetColorParams& other) const {
    return !(*this == other);
  }
  size_t GetHash() const;
  std::string ToString() const;
};

}  // namespace cc

#endif  // CC_PAINT_TARGET_COLOR_PARAMS_H_
