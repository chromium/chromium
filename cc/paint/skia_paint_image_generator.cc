// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skia_paint_image_generator.h"

#include "cc/paint/paint_image_generator.h"

namespace cc {

SkiaPaintImageGenerator::SkiaPaintImageGenerator(
    sk_sp<PaintImageGenerator> paint_image_generator,
    size_t frame_index,
    PaintImage::GeneratorClientId client_id)
    : SkImageGenerator(paint_image_generator->GetSkImageInfo()),
      paint_image_generator_(std::move(paint_image_generator)),
      frame_index_(frame_index),
      client_id_(client_id) {}

SkiaPaintImageGenerator::~SkiaPaintImageGenerator() = default;

sk_sp<SkData> SkiaPaintImageGenerator::onRefEncodedData() {
  return paint_image_generator_->GetEncodedData();
}

bool SkiaPaintImageGenerator::onGetPixels(const SkImageInfo& info,
                                          void* pixels,
                                          size_t row_bytes,
                                          const Options& options) {
  return paint_image_generator_->GetPixels(
      info, pixels, row_bytes, frame_index_, client_id_, uniqueID());
}

bool SkiaPaintImageGenerator::onQueryYUVA8(
    SkYUVASizeInfo* size_info,
    SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
    SkYUVColorSpace* color_space) const {
  return paint_image_generator_->QueryYUVA8(size_info, indices, color_space);
}

bool SkiaPaintImageGenerator::onGetYUVA8Planes(
    const SkYUVASizeInfo& size_info,
    const SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
    void* planes[4]) {
  return paint_image_generator_->GetYUVA8Planes(size_info, indices, planes,
                                                frame_index_, uniqueID());
}

}  // namespace cc
