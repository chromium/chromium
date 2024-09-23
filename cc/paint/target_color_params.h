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

  // The maximum SDR luminance of the target, in nits.
  float sdr_max_luminance_nits = gfx::ColorSpace::kDefaultSDRWhiteLevel;

  // The maximum HDR luminance of the target, in multiples of the SDR maximum
  // luminance (a non-HDR-capable display will have a value of 1).
  float hdr_max_luminance_relative = 1.f;

  bool operator==(const TargetColorParams& other) const {
    return color_space == other.color_space &&
           sdr_max_luminance_nits == other.sdr_max_luminance_nits &&
           hdr_max_luminance_relative == other.hdr_max_luminance_relative;
  }
  bool operator!=(const TargetColorParams& other) const {
    return !(*this == other);
  }
  size_t GetHash() const;
  std::string ToString() const;
};

}  // namespace cc

#endif  // CC_PAINT_TARGET_COLOR_PARAMS_H_
