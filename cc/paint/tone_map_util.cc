// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/tone_map_util.h"

#include <utility>

#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "ui/gfx/color_conversion_sk_filter_cache.h"

namespace cc {

bool ToneMapUtil::UseGainmapShader(const PaintImage& image) {
  if (image.gainmap_sk_image_) {
    DCHECK(image.cached_sk_image_);
    DCHECK(image.gainmap_info_.has_value());
    return true;
  }
  return false;
}

bool ToneMapUtil::UseGlobalToneMapFilter(const PaintImage& image) {
  if (!image.cached_sk_image_) {
    return false;
  }
  return UseGlobalToneMapFilter(image.cached_sk_image_->colorSpace());
}

bool ToneMapUtil::UseGlobalToneMapFilter(const SkColorSpace* cs) {
  if (!cs) {
    return false;
  }
  skcms_TransferFunction fn;
  cs->transferFn(&fn);
  return skcms_TransferFunction_isHLGish(&fn) ||
         skcms_TransferFunction_isPQish(&fn);
}

void ToneMapUtil::AddGlobalToneMapFilterToPaint(
    SkPaint& paint,
    const PaintImage& image,
    sk_sp<SkColorSpace> dst_color_space) {
  if (!dst_color_space) {
    dst_color_space = SkColorSpace::MakeSRGB();
  }
  // Workaround for b/337538021: Disable tone mapping when the source and
  // destination spaces are the same, to avoid applying tone mapping when
  // uploading HLG or PQ frames to textures.
  if (SkColorSpace::Equals(image.cached_sk_image_->colorSpace(),
                           dst_color_space.get())) {
    return;
  }
  gfx::ColorConversionSkFilterCache cache;
  sk_sp<SkColorFilter> filter = cache.Get(
      gfx::ColorSpace(*image.cached_sk_image_->colorSpace()),
      gfx::ColorSpace(*dst_color_space.get()),
      /*src_bit_depth=*/std::nullopt, image.hdr_metadata_,
      gfx::ColorSpace::kDefaultSDRWhiteLevel, image.target_hdr_headroom_);
  if (paint.getColorFilter()) {
    // Perform tone mapping before the existing color filter.
    paint.setColorFilter(
        paint.getColorFilter()->makeComposed(std::move(filter)));
  } else {
    paint.setColorFilter(std::move(filter));
  }
}

}  // namespace cc
