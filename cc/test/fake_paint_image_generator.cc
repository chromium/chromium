// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include <cc/test/fake_paint_image_generator.h>

#include <utility>

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

FakePaintImageGenerator::FakePaintImageGenerator(
    const SkImageInfo& info,
    const SkYUVASizeInfo& yuva_size_info,
    std::vector<FrameMetadata> frames,
    bool allocate_discardable_memory,
    std::vector<SkISize> supported_sizes)
    : PaintImageGenerator(info, std::move(frames)),
      image_backing_memory_(
          allocate_discardable_memory ? yuva_size_info.computeTotalBytes() : 0,
          0),
      supported_sizes_(std::move(supported_sizes)),
      is_yuv_(true),
      yuva_size_info_(yuva_size_info) {}

FakePaintImageGenerator::~FakePaintImageGenerator() = default;

sk_sp<SkData> FakePaintImageGenerator::GetEncodedData() const {
  return SkData::MakeEmpty();
}

bool FakePaintImageGenerator::GetPixels(const SkImageInfo& info,
                                        void* pixels,
                                        size_t row_bytes,
                                        size_t frame_index,
                                        PaintImage::GeneratorClientId client_id,
                                        uint32_t lazy_pixel_ref) {
  CHECK(!is_yuv_ || expect_fallback_to_rgb_);
  if (image_backing_memory_.empty())
    return false;
  if (expect_fallback_to_rgb_) {
    image_backing_memory_.resize(info.computeMinByteSize(), 0);
    image_pixmap_ =
        SkPixmap(info, image_backing_memory_.data(), info.minRowBytes());
  }
  if (frames_decoded_count_.find(frame_index) == frames_decoded_count_.end())
    frames_decoded_count_[frame_index] = 1;
  else
    frames_decoded_count_[frame_index]++;
  SkPixmap dst(info, pixels, row_bytes);
  CHECK(image_pixmap_.scalePixels(dst, kMedium_SkFilterQuality));
  decode_infos_.push_back(info);
  return true;
}

bool FakePaintImageGenerator::QueryYUVA8(SkYUVASizeInfo* yuv_info,
                                         SkYUVAIndex indices[],
                                         SkYUVColorSpace* color_space) const {
  if (!is_yuv_)
    return false;
  *yuv_info = yuva_size_info_;
  return true;
}

bool FakePaintImageGenerator::GetYUVA8Planes(const SkYUVASizeInfo& yuv_info,
                                             const SkYUVAIndex indices[],
                                             void* planes[4],
                                             size_t frame_index,
                                             uint32_t lazy_pixel_ref) {
  CHECK(is_yuv_);
  CHECK(!expect_fallback_to_rgb_);
  if (image_backing_memory_.empty())
    return false;
  int numPlanes = SkYUVASizeInfo::kMaxCount;
  void* src_planes[numPlanes];
  yuv_info.computePlanes(image_backing_memory_.data(), src_planes);
  for (int i = 0; i < numPlanes; ++i) {
    size_t bytes_for_plane_i =
        yuv_info.fWidthBytes[i] *
        base::checked_cast<size_t>(yuv_info.fSizes[i].height());
    memcpy(planes[i], src_planes[i], bytes_for_plane_i);
  }
  if (frames_decoded_count_.find(frame_index) == frames_decoded_count_.end())
    frames_decoded_count_[frame_index] = 1;
  else
    frames_decoded_count_[frame_index]++;
  return true;
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

const ImageHeaderMetadata*
FakePaintImageGenerator::GetMetadataForDecodeAcceleration() const {
  return &image_metadata_;
}

}  // namespace cc
