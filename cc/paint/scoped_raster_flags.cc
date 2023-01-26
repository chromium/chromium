// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/scoped_raster_flags.h"

#include <utility>

#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_filter.h"
#include "cc/paint/paint_image_builder.h"

namespace cc {
ScopedRasterFlags::~ScopedRasterFlags() = default;

void ScopedRasterFlags::DecodeImageShader(const SkMatrix& ctm) {
  if (!flags()->HasShader() ||
      flags()->getShader()->shader_type() != PaintShader::Type::kImage)
    return;

  PaintImage image = flags()->getShader()->paint_image();
  if (image.IsPaintWorklet()) {
    ImageProvider::ScopedResult result =
        decode_stashing_image_provider_->GetRasterContent(DrawImage(image));
    if (result && result.has_paint_record()) {
      const PaintShader* shader = flags()->getShader();
      SkMatrix local_matrix = shader->GetLocalMatrix();
      auto decoded_shader = PaintShader::MakePaintRecord(
          result.ReleaseAsRecord(), shader->tile(), shader->tx(), shader->tx(),
          &local_matrix);
      MutableFlags()->setShader(decoded_shader);
    } else {
      decode_failed_ = true;
    }
    return;
  }

  uint32_t transfer_cache_entry_id = kInvalidImageTransferCacheEntryId;
  PaintFlags::FilterQuality raster_quality = flags()->getFilterQuality();
  bool transfer_cache_entry_needs_mips = false;
  gpu::Mailbox mailbox;
  auto decoded_shader = flags()->getShader()->CreateDecodedImage(
      ctm, flags()->getFilterQuality(), &*decode_stashing_image_provider_,
      &transfer_cache_entry_id, &raster_quality,
      &transfer_cache_entry_needs_mips, &mailbox);
  DCHECK_EQ(transfer_cache_entry_id, kInvalidImageTransferCacheEntryId);
  DCHECK_EQ(transfer_cache_entry_needs_mips, false);
  DCHECK(mailbox.IsZero());

  if (!decoded_shader) {
    decode_failed_ = true;
    return;
  }

  MutableFlags()->setFilterQuality(raster_quality);
  MutableFlags()->setShader(decoded_shader);
}

void ScopedRasterFlags::DecodeRecordShader(const SkMatrix& ctm,
                                           int max_texture_size) {
  if (!flags()->HasShader() ||
      flags()->getShader()->shader_type() != PaintShader::Type::kPaintRecord)
    return;

  // Only replace shaders with animated images. Creating transient shaders for
  // replacing decodes during raster results in cache misses in skia's picture
  // shader cache, which results in re-rasterizing the picture for every draw.
  if (flags()->getShader()->image_analysis_state() !=
      ImageAnalysisState::kAnimatedImages) {
    return;
  }

  gfx::SizeF raster_scale(1.f, 1.f);
  auto decoded_shader = flags()->getShader()->CreateScaledPaintRecord(
      ctm, max_texture_size, &raster_scale);
  decoded_shader->ResolveSkObjects(&raster_scale,
                                   &*decode_stashing_image_provider_);
  MutableFlags()->setShader(std::move(decoded_shader));
}

void ScopedRasterFlags::DecodeFilter() {
  if (!flags()->getImageFilter() ||
      !flags()->getImageFilter()->has_discardable_images() ||
      flags()->getImageFilter()->image_analysis_state() !=
          ImageAnalysisState::kAnimatedImages) {
    return;
  }

  MutableFlags()->setImageFilter(flags()->getImageFilter()->SnapshotWithImages(
      &*decode_stashing_image_provider_));
}

void ScopedRasterFlags::AdjustStrokeIfNeeded(const SkMatrix& ctm) {
  // With anti-aliasing turned off, strokes with a device space width in (0, 1)
  // may not raster at all.  To avoid this, we have two options:
  //
  // 1) force a hairline stroke (stroke-width == 0)
  // 2) force anti-aliasing on

  SkSize scale;
  if (flags()->isAntiAlias() ||                          // safe to raster
      flags()->getStyle() == PaintFlags::kFill_Style ||  // not a stroke
      !flags()->getStrokeWidth() ||                      // explicit hairline
      !ctm.decomposeScale(&scale)) {                     // cannot decompose
    return;
  }

  const auto stroke_vec =
      SkVector::Make(flags()->getStrokeWidth() * scale.width(),
                     flags()->getStrokeWidth() * scale.height());
  if (stroke_vec.x() >= 1.f && stroke_vec.y() >= 1.f)
    return;  // safe to raster

  const auto can_substitute_hairline =
      flags()->getStrokeCap() == PaintFlags::kDefault_Cap &&
      flags()->getStrokeJoin() == PaintFlags::kDefault_Join;
  if (can_substitute_hairline && stroke_vec.x() < 1.f && stroke_vec.y() < 1.f) {
    // Use modulated hairline when possible, as it is faster and produces
    // results closer to the original intent.
    MutableFlags()->setStrokeWidth(0);
    MutableFlags()->setAlphaf(flags()->getAlphaf() *
                              std::sqrt(stroke_vec.x() * stroke_vec.y()));
    return;
  }

  // Fall back to anti-aliasing.
  MutableFlags()->setAntiAlias(true);
}

}  // namespace cc
