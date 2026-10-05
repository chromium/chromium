// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/tone_map_util.h"

#include <algorithm>
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
#include "ui/gfx/color_space.h"
#include "ui/gfx/hdr_metadata.h"

namespace cc {

namespace {

struct ToneMapInfo {
  // If the metadata and the skcms_TransferFunction have different white points,
  // then multiplying by `white_scale_factor` transforms the
  // skcms_TransferFunction white to the metadata's white.
  float white_scale_factor = 1.f;

  // The baseline image's HDR headroom.
  float baseline_headroom = 0.f;

  // The max and min headroom amongst the headrooms of the baseline image and
  // all alternate images.
  float max_headroom = 0.f;
  float min_headroom = 0.f;

  bool UseGlobalToneMappingFilter() const {
    // If there are two different image representations that use two different
    // headrooms, then the tone mapping filter should be used to map between
    // them.
    if (min_headroom != max_headroom) {
      return true;
    }
    // If the white point must be scaled, then the tone mapping shader is
    // employed to do this.
    if (white_scale_factor != 1.f) {
      return true;
    }
    return false;
  }
};

ToneMapInfo ComputeToneMapInfo(const skcms_TransferFunction& fn,
                               const gfx::HDRMetadata& metadata) {
  const auto fn_type = skcms_TransferFunction_getType(&fn);
  ToneMapInfo info;

  // The most common path is SDR content with no AGTM or extended range
  // metadata, so handle that first.
  if (fn_type != skcms_TFType_PQ && fn_type != skcms_TFType_HLG &&
      !metadata.HasAgtm() && !metadata.extended_range) {
    return info;
  }

  // For HLG and PQ, compute the HDR reference white and white scaling factor.
  float hdr_reference_white =
      skhdr::AdaptiveGlobalToneMap::kDefaultHdrReferenceWhite;
  if (fn_type == skcms_TFType_PQ || fn_type == skcms_TFType_HLG) {
    if (metadata.HasAgtm()) {
      hdr_reference_white = metadata.GetAgtm().fHdrReferenceWhite;
      info.white_scale_factor = hdr_reference_white / fn.a;
    } else {
      hdr_reference_white = fn.a;
    }
  }

  // If there is a headroom-adaptive gain curve, use that to compute the
  // HDR headrooms.
  const skhdr::AdaptiveGlobalToneMap::HeadroomAdaptiveToneMap* hatm = nullptr;
  if (metadata.HasAgtm() && metadata.GetAgtm().fHeadroomAdaptiveToneMap) {
    hatm = &metadata.GetAgtm().fHeadroomAdaptiveToneMap.value();
  }
  if (hatm) {
    info.baseline_headroom = hatm->fBaselineHdrHeadroom;
    if (!hatm->fAlternateImages.empty()) {
      info.min_headroom = std::min(info.baseline_headroom,
                                   hatm->fAlternateImages.front().fHdrHeadroom);
      info.max_headroom = std::max(info.baseline_headroom,
                                   hatm->fAlternateImages.back().fHdrHeadroom);
    } else {
      info.min_headroom = info.baseline_headroom;
      info.max_headroom = info.baseline_headroom;
    }
    return info;
  }

  // Extended range is next-highest priority. Note that `current_headroom` is
  // linear, while ToneMapInfo headrooms are in log2 stops.
  if (metadata.extended_range) {
    info.baseline_headroom = info.min_headroom = info.max_headroom =
        std::log2(std::max(metadata.extended_range->current_headroom, 1.f));
    return info;
  }

  // For HLG and PQ (which are the only ways we can reach here), compute the
  // baseline headroom using the CLLI and MDCV metadata.
  DCHECK(fn_type == skcms_TFType_PQ || fn_type == skcms_TFType_HLG);
  float peak_luminance = gfx::HDRMetadata::GetContentMaxLuminance(metadata);
  if (peak_luminance > hdr_reference_white) {
    info.baseline_headroom = std::log2(peak_luminance / hdr_reference_white);
    info.max_headroom = info.baseline_headroom;
  }
  return info;
}

}  // namespace

bool ToneMapUtil::UseGlobalToneMapFilter(const SkImage* image,
                                         const gfx::HDRMetadata& metadata) {
  if (!image) {
    return false;
  }
  return UseGlobalToneMapFilter(image->colorSpace(), metadata);
}

bool ToneMapUtil::UseGlobalToneMapFilter(const SkColorSpace* cs,
                                         const gfx::HDRMetadata& metadata) {
  if (!cs) {
    return false;
  }
  skcms_TransferFunction fn;
  cs->transferFn(&fn);
  return ComputeToneMapInfo(fn, metadata).UseGlobalToneMappingFilter();
}

float ToneMapUtil::GetMaxHdrHeadroom(const SkColorSpace* cs,
                                     const gfx::HDRMetadata& metadata) {
  if (!cs) {
    return 0.f;
  }
  skcms_TransferFunction fn;
  cs->transferFn(&fn);
  return ComputeToneMapInfo(fn, metadata).max_headroom;
}

float ToneMapUtil::GetMaxHdrHeadroom(const gfx::ColorSpace& cs,
                                     const gfx::HDRMetadata& metadata) {
  return GetMaxHdrHeadroom(cs.ToSkColorSpace().get(), metadata);
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

  // Set the MDCV, CLLI, and AGTM values on `skia_metadata`.
  if (metadata.HasMDCV()) {
    skia_metadata.setMasteringDisplayColorVolume(metadata.GetMDCV());
  }
  if (metadata.HasCLLI()) {
    skia_metadata.setContentLightLevelInformation(metadata.GetCLLI());
  }
  if (metadata.HasAgtm()) {
    skia_metadata.setAdaptiveGlobalToneMap(metadata.GetAgtm());
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
