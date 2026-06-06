// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <utility>
#include <vector>

#include "cc/paint/paint_image_generator.h"

#include "base/atomic_sequence_num.h"
#include "third_party/skia/include/core/SkImageInfo.h"
#include "third_party/skia/include/core/SkSize.h"

namespace cc {

PaintImageGenerator::PaintImageGenerator(const SkImageInfo& info,
                                         const gfx::HDRMetadata& hdr_metadata,
                                         std::vector<FrameMetadata> frames)
    : info_(info),
      hdr_metadata_(hdr_metadata),
      generator_content_id_(PaintImage::GetNextContentId()),
      frames_(std::move(frames)) {}

PaintImageGenerator::~PaintImageGenerator() = default;

PaintImage::ContentId PaintImageGenerator::GetContentIdForFrame(
    size_t frame_index) const {
  return generator_content_id_;
}

SkISize PaintImageGenerator::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  // The base class just returns the original size as the only supported decode
  // size.
  return info_.dimensions();
}

const ImageHeaderMetadata*
PaintImageGenerator::GetMetadataForDecodeAcceleration() const {
  return nullptr;
}

}  // namespace cc
