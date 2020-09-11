// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/skia_paint_image_generator.h"

#include <utility>

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

bool SkiaPaintImageGenerator::onQueryYUVAInfo(
    const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
    SkYUVAPixmapInfo* yuva_pixmap_info) const {
  if (!paint_image_generator_->QueryYUVA(supported_data_types,
                                         yuva_pixmap_info)) {
    return false;
  }
  // TODO(skbug.com/10632): Enable other bit depths now that they are supported
  //  by SkImageGenerator. There's no known reason that this should not work.
  return yuva_pixmap_info->dataType() == SkYUVAPixmapInfo::DataType::kUnorm8;
}

bool SkiaPaintImageGenerator::onGetYUVAPlanes(const SkYUVAPixmaps& planes) {
  return paint_image_generator_->GetYUVAPlanes(planes, frame_index_,
                                               uniqueID());
}

}  // namespace cc
