// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/tone_map_util.h"

#include <cmath>
#include <memory>
#include <string_view>
#include <utility>

#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"
#include "third_party/skia/include/private/SkHdrMetadata.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {

bool ToneMapUtil::UseGlobalToneMapFilter(const SkImage* image,
                                         const SkColorSpace* dst_color_space) {
  if (!image) {
    return false;
  }
  // Workaround for crbug.com/337538021: Disable tone mapping when the source
  // and destination spaces are the same, to avoid applying tone mapping when
  // uploading HLG or PQ frames to textures.
  if (SkColorSpace::Equals(image->colorSpace(), dst_color_space)) {
    return false;
  }
  return UseGlobalToneMapFilter(image->colorSpace());
}

bool ToneMapUtil::UseGlobalToneMapFilter(const SkColorSpace* cs) {
  if (!cs) {
    return false;
  }
  skcms_TransferFunction fn;
  cs->transferFn(&fn);
  return skcms_TransferFunction_isHLGish(&fn) ||
         skcms_TransferFunction_isPQish(&fn) ||
         skcms_TransferFunction_isHLG(&fn) || skcms_TransferFunction_isPQ(&fn);
}

void ToneMapUtil::AddGlobalToneMapFilterToPaint(
    SkPaint& paint,
    const SkImage* image,
    const gfx::HDRMetadata& metadata,
    float target_hdr_headroom) {
  if (!image || !image->colorSpace()) {
    return;
  }

  skhdr::Metadata skia_metadata;

  // Parse AGTM only if the feature is enabled.
  skhdr::AdaptiveGlobalToneMap agtm;
  bool agtm_valid = false;
  if (gfx::HdrMetadataAgtm::IsEnabled()) {
    agtm_valid = agtm.parse(metadata.getSerializedAgtm());
  }

  // Use NDWL to specify HDR reference white only if AGTM was not present.
  if (!agtm_valid && metadata.ndwl.has_value() && metadata.ndwl->nits > 0.f) {
    agtm.fHdrReferenceWhite = metadata.ndwl->nits;
    agtm_valid = true;
  }

  // Set the MDCV, CLLI, and AGTM values on `skia_metadata`.
  if (metadata.smpte_st_2086.has_value()) {
    skia_metadata.setMasteringDisplayColorVolume({
        .fDisplayPrimaries = metadata.smpte_st_2086->primaries,
        .fMaximumDisplayMasteringLuminance =
            metadata.smpte_st_2086->luminance_max,
        .fMinimumDisplayMasteringLuminance =
            metadata.smpte_st_2086->luminance_min,
    });
  }
  if (metadata.cta_861_3.has_value()) {
    skia_metadata.setContentLightLevelInformation({
        .fMaxCLL =
            static_cast<float>(metadata.cta_861_3->max_content_light_level),
        .fMaxFALL = static_cast<float>(
            metadata.cta_861_3->max_frame_average_light_level),
    });
  }
  if (agtm_valid) {
    skia_metadata.setAdaptiveGlobalToneMap(agtm);
  }

  // Use skhdr::Metadata to compute the filter.
  auto tone_map_filter = skia_metadata.makeToneMapColorFilter(
      target_hdr_headroom, image->colorSpace());

  // Perform the original filter after tone mapping.
  if (tone_map_filter) {
    paint.setColorFilter(SkColorFilters::Compose(paint.refColorFilter(),
                                                 std::move(tone_map_filter)));
  }
}

}  // namespace cc
