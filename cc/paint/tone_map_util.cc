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

namespace {

// Scale the color values in linear space.
sk_sp<SkColorFilter> GetLinearScaleFilter(float s) {
  if (s == 1.f) {
    return nullptr;
  }
  SkColorMatrix scale_matrix;
  scale_matrix.setScale(s, s, s);
  sk_sp<SkColorFilter> filter =
      SkColorFilters::Matrix(scale_matrix, SkColorFilters::Clamp::kNo);
  CHECK(filter);
  return filter->makeWithWorkingColorSpace(SkColorSpace::MakeRGB(
      SkNamedTransferFn::kLinear, SkNamedGamut::kRec2020));
}

// The Reinhard tone mapping function performs the tone mapping described in:
// https://docs.google.com/document/d/17T2ek1i2R7tXdfHCnM-i5n6__RoYe0JyMfKmTEjoGR8/edit?usp=sharing
// And further detailed in:
// https://colab.research.google.com/drive/1hI10nq6L6ru_UFvz7-f7xQaQp0qarz_K
// The working space of this shader is premultiplied, and so the computed value
// of M (maxRGB) must be un-multiplied.
static constexpr char kReinhardToneMapSKSL[] =
    "uniform half a;\n"
    "uniform half b;\n"
    "half4 main(half4 color) {\n"
    "  half M = max(max(color.r, color.g), color.b);\n"
    "  if (M > 0.0) {\n"
    "    if (color.a > 0.0) {\n"
    "      M /= color.a;\n"
    "    }\n"
    "    color.rgb *= (1.0 + a * M) / (1.0 + b * M);\n"
    "  }\n"
    "  return color;\n"
    "}\n";

sk_sp<SkColorFilter> GetReinhardToneMapFilter(
    float content_max_component_value,
    float target_max_component_value) {
  static const SkRuntimeEffect* effect =
      SkRuntimeEffect::MakeForColorFilter(
          SkString(kReinhardToneMapSKSL, sizeof(kReinhardToneMapSKSL) - 1),
          /*options=*/{})
          .effect.release();
  CHECK(effect);

  // Computation of the uniforms a and b is derived in
  // https://colab.research.google.com/drive/1hI10nq6L6ru_UFvz7-f7xQaQp0qarz_K
  float uniforms[2] = {0.f, 0.f};
  if (content_max_component_value > target_max_component_value) {
    uniforms[0] = target_max_component_value /
                  (content_max_component_value * content_max_component_value);
    uniforms[1] = 1.f / target_max_component_value;
  }
  auto uniforms_data = SkData::MakeWithCopy(uniforms, sizeof(uniforms));

  sk_sp<SkColorFilter> filter = effect->makeColorFilter(uniforms_data);
  CHECK(filter);
  return filter->makeWithWorkingColorSpace(SkColorSpace::MakeRGB(
      SkNamedTransferFn::kLinear, SkNamedGamut::kRec2020));
}

}  // namespace

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
  skcms_TransferFunction trfn;
  image->colorSpace()->transferFn(&trfn);

  // Parse AGTM, only if the feature is enabled.
  std::unique_ptr<skhdr::Agtm> agtm_parsed;
  if (gfx::HdrMetadataAgtm::IsEnabled()) {
    agtm_parsed = skhdr::Agtm::Make(metadata.getSerializedAgtm());
  }

  // The remainder of the function will construct `filter` to perform all
  // transformations (scaling, OOTF, and tone mapping).
  sk_sp<SkColorFilter> filter;

  // Several stages will use the reference white luminance. Compute it ahead
  // of time.
  auto compute_reference_white_luminance = [&]() {
    // AGTM metadata gets priority.
    if (agtm_parsed) {
      return agtm_parsed->getHdrReferenceWhite();
    }
    // Then NDWL.
    if (metadata.ndwl.has_value() && metadata.ndwl->nits > 0.f) {
      return metadata.ndwl->nits;
    }
    // Then defer to the source color space.
    if (skcms_TransferFunction_isPQ(&trfn) ||
        skcms_TransferFunction_isHLG(&trfn)) {
      return trfn.a;
    }
    // Then use the default.
    return gfx::ColorSpace::kDefaultSDRWhiteLevel;
  };
  const float reference_white_luminance = compute_reference_white_luminance();

  if (skcms_TransferFunction_isPQ(&trfn) ||
      skcms_TransferFunction_isHLG(&trfn)) {
    // The HLG or PQ SkColorSpace may have a white level baked into it.
    // Re-scale to be relative to the white level from the metadata.
    filter = GetLinearScaleFilter(trfn.a / reference_white_luminance);
  }

  // Apply tone mapping.
  if (agtm_parsed) {
    auto tone_map_filter = agtm_parsed->makeColorFilter(target_hdr_headroom);
    filter = SkColorFilters::Compose(tone_map_filter, std::move(filter));
  } else {
    const float content_max_luminance =
        gfx::HDRMetadata::GetContentMaxLuminance(metadata);
    auto tone_map_filter = GetReinhardToneMapFilter(
        content_max_luminance / reference_white_luminance,
        std::exp2(target_hdr_headroom));
    filter = SkColorFilters::Compose(tone_map_filter, std::move(filter));
  }

  // Perform the original filter after tone mapping.
  paint.setColorFilter(
      SkColorFilters::Compose(paint.refColorFilter(), std::move(filter)));
}

}  // namespace cc
