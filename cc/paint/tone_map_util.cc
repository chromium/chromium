// Copyright 2024 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/tone_map_util.h"

#include <string_view>
#include <utility>

#include "base/json/json_reader.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkColorFilter.h"
#include "third_party/skia/include/core/SkColorSpace.h"
#include "third_party/skia/include/core/SkPaint.h"
#include "third_party/skia/include/effects/SkColorMatrix.h"
#include "third_party/skia/include/effects/SkRuntimeEffect.h"

namespace cc {

namespace {

// The HLG OOTF filter applies reference HLG opto-optical transfer function as
// described in Table 5 of ITU-R BT.2100-3, using the value of gamma=1.2.
// The working space of this shader has premultiplied alpha, and so the
// computed value of Y must be un-multiplied.
static constexpr char kHlgOotfSKSL[] =
    "half4 main(half4 color) {\n"
    "  half3 L = half3(0.2627, 0.6780, 0.0593);\n"
    "  half Y = dot(L, color.rgb);\n"
    "  if (Y > 0.0) {\n"
    "    if (color.a > 0.0) {\n"
    "      Y /= color.a;\n"
    "    }\n"
    "    color.rgb *= pow(Y, 0.2);\n"
    "  }\n"
    "  return color;\n"
    "}\n";

sk_sp<SkColorFilter> GetHlgOotfFilter() {
  static const SkRuntimeEffect* effect =
      SkRuntimeEffect::MakeForColorFilter(
          SkString(kHlgOotfSKSL, sizeof(kHlgOotfSKSL) - 1),
          /*options=*/{})
          .effect.release();
  CHECK(effect);

  sk_sp<SkColorFilter> filter = effect->makeColorFilter(nullptr);
  CHECK(filter);
  return filter->makeWithWorkingColorSpace(SkColorSpace::MakeRGB(
      SkNamedTransferFn::kLinear, SkNamedGamut::kRec2020));
}

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

bool ToneMapUtil::UseGainmapShader(const PaintImage& image) {
  if (image.gainmap_sk_image_) {
    DCHECK(image.cached_sk_image_);
    DCHECK(image.gainmap_info_.has_value());
    return true;
  }
  return false;
}

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
         skcms_TransferFunction_isPQish(&fn);
}

void ToneMapUtil::AddGlobalToneMapFilterToPaint(
    SkPaint& paint,
    const SkImage* image,
    const std::optional<gfx::HDRMetadata>& metadata,
    float target_linear_hdr_headroom) {
  if (!image || !image->colorSpace()) {
    return;
  }
  skcms_TransferFunction trfn;
  image->colorSpace()->transferFn(&trfn);

  // The remaineder of the function will construct `filter` to perform all
  // transformations (scaling, OOTF, and tone mapping).
  sk_sp<SkColorFilter> filter;

  // Several stages will use the reference white luminance, so extract it
  // early.
  const float reference_white_luminance =
      gfx::HDRMetadata::GetReferenceWhiteLuminance(metadata);

  // The HLG or PQ SkColorSpace may have a white level baked into it. Re-scale
  // to be relative to the white level from the metadata, and apply the
  // reference HLG OOTF (if needed).
  if (skcms_TransferFunction_isPQish(&trfn)) {
    // Scale so that we are using the reference inverse OETF, which maps [0,1]
    // to [0,1]. Then scale by the reference display luminance (10,000 nits),
    // and divide by white luminance.
    const float trfn_max = skcms_TransferFunction_eval(&trfn, 1.f);
    filter =
        GetLinearScaleFilter(10000.f / reference_white_luminance / trfn_max);
  } else if (skcms_TransferFunction_isHLGish(&trfn)) {
    // Scale so that we are using the reference inverse OETF, which maps [0,1]
    // to [0,1] (instead of [0,1] to [0,12] or something else).
    const float trfn_max = skcms_TransferFunction_eval(&trfn, 1.f);
    auto pre_ootf_scale = GetLinearScaleFilter(1.f / trfn_max);
    // Apply the reference OOTF on this.
    auto ootf = GetHlgOotfFilter();
    // Scale by the reference display luminance (1000 nits), and divide by the
    // white luminance.
    auto post_ootf_scale =
        GetLinearScaleFilter(1000.f / reference_white_luminance);

    // Set `filter` to the three operations in sequence.
    filter = SkColorFilters::Compose(
        post_ootf_scale, SkColorFilters::Compose(ootf, pre_ootf_scale));
  }

  // Apply tone mapping.
  {
    const float content_max_luminance =
        gfx::HDRMetadata::GetContentMaxLuminance(metadata);
    auto tone_map_filter = GetReinhardToneMapFilter(
        content_max_luminance / reference_white_luminance,
        target_linear_hdr_headroom);
    filter = SkColorFilters::Compose(tone_map_filter, std::move(filter));
  }

  // Perform the original filter after tone mapping.
  paint.setColorFilter(
      SkColorFilters::Compose(paint.refColorFilter(), std::move(filter)));
}

}  // namespace cc
