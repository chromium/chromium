// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cc/test/fake_paint_image_generator.h>

namespace cc {

FakePaintImageGenerator::FakePaintImageGenerator(
    const SkImageInfo& info,
    std::vector<FrameMetadata> frames,
    bool allocate_discardable_memory,
    std::vector<SkISize> supported_sizes)
    : PaintImageGenerator(info, std::move(frames)),
      image_backing_memory_(
          allocate_discardable_memory ? info.computeMinByteSize() : 0,
          0),
      image_pixmap_(info, image_backing_memory_.data(), info.minRowBytes()),
      supported_sizes_(std::move(supported_sizes)) {}

FakePaintImageGenerator::~FakePaintImageGenerator() = default;

sk_sp<SkData> FakePaintImageGenerator::GetEncodedData() const {
  return nullptr;
}

bool FakePaintImageGenerator::GetPixels(const SkImageInfo& info,
                                        void* pixels,
                                        size_t row_bytes,
                                        size_t frame_index,
                                        PaintImage::GeneratorClientId client_id,
                                        uint32_t lazy_pixel_ref) {
  if (image_backing_memory_.empty())
    return false;
  if (frames_decoded_count_.find(frame_index) == frames_decoded_count_.end())
    frames_decoded_count_[frame_index] = 1;
  else
    frames_decoded_count_[frame_index]++;
  SkPixmap dst(info, pixels, row_bytes);
  CHECK(image_pixmap_.scalePixels(dst, kMedium_SkFilterQuality));
  decode_infos_.push_back(info);
  return true;
}

bool FakePaintImageGenerator::QueryYUVA8(SkYUVASizeInfo* info,
                                         SkYUVAIndex indices[],
                                         SkYUVColorSpace* color_space) const {
  return false;
}

bool FakePaintImageGenerator::GetYUVA8Planes(const SkYUVASizeInfo& info,
                                             const SkYUVAIndex indices[],
                                             void* planes[4],
                                             size_t frame_index,
                                             uint32_t lazy_pixel_ref) {
  NOTREACHED();
  return false;
}

SkISize FakePaintImageGenerator::GetSupportedDecodeSize(
    const SkISize& requested_size) const {
  for (auto& size : supported_sizes_) {
    if (size.width() >= requested_size.width() &&
        size.height() >= requested_size.height()) {
      return size;
    }
  }
  return PaintImageGenerator::GetSupportedDecodeSize(requested_size);
}

}  // namespace cc
