// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/351564777): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "cc/test/fake_paint_image_generator.h"

#include <utility>

#include "base/containers/contains.h"

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
    const SkYUVAPixmapInfo& yuva_pixmap_info,
    std::vector<FrameMetadata> frames,
    bool allocate_discardable_memory,
    std::vector<SkISize> supported_sizes)
    : PaintImageGenerator(info, std::move(frames)),
      image_backing_memory_(allocate_discardable_memory
                                ? yuva_pixmap_info.computeTotalBytes()
                                : 0,
                            0),
      supported_sizes_(std::move(supported_sizes)),
      is_yuv_(true),
      yuva_pixmap_info_(yuva_pixmap_info) {}

FakePaintImageGenerator::~FakePaintImageGenerator() = default;

sk_sp<SkData> FakePaintImageGenerator::GetEncodedData() const {
  return SkData::MakeEmpty();
}

bool FakePaintImageGenerator::GetPixels(SkPixmap dst_pixmap,
                                        size_t frame_index,
                                        PaintImage::GeneratorClientId client_id,
                                        uint32_t lazy_pixel_ref) {
  base::AutoLock lock(lock_);

  CHECK(!is_yuv_ || expect_fallback_to_rgb_);
  const SkImageInfo& dst_info = dst_pixmap.info();
  if (image_backing_memory_.empty())
    return false;
  if (expect_fallback_to_rgb_) {
    image_backing_memory_.resize(dst_info.computeMinByteSize(), 0);
    image_pixmap_ = SkPixmap(dst_info, image_backing_memory_.data(),
                             dst_info.minRowBytes());
  }
  if (!base::Contains(frames_decoded_count_, frame_index)) {
    frames_decoded_count_[frame_index] = 1;
  } else {
    frames_decoded_count_[frame_index]++;
  }
  CHECK(image_pixmap_.scalePixels(
      dst_pixmap, {SkFilterMode::kLinear, SkMipmapMode::kNearest}));
  decode_infos_.push_back(dst_info);
  return true;
}

bool FakePaintImageGenerator::QueryYUVA(
    const SkYUVAPixmapInfo::SupportedDataTypes& supported_data_types,
    SkYUVAPixmapInfo* yuva_pixmap_info) const {
  if (!is_yuv_)
    return false;

  *yuva_pixmap_info = yuva_pixmap_info_;
  return yuva_pixmap_info->isSupported(supported_data_types);
}

bool FakePaintImageGenerator::GetYUVAPlanes(
    const SkYUVAPixmaps& pixmaps,
    size_t frame_index,
    uint32_t lazy_pixel_ref,
    PaintImage::GeneratorClientId client_id) {
  base::AutoLock lock(lock_);

  CHECK(is_yuv_);
  CHECK(!expect_fallback_to_rgb_);
  if (image_backing_memory_.empty())
    return false;
  size_t plane_sizes[SkYUVAInfo::kMaxPlanes];
  yuva_pixmap_info_.computeTotalBytes(plane_sizes);
  uint8_t* src_plane_memory = image_backing_memory_.data();
  int num_planes = pixmaps.numPlanes();
  for (int i = 0; i < num_planes; ++i) {
    memcpy(pixmaps.plane(i).writable_addr(), src_plane_memory, plane_sizes[i]);
    src_plane_memory += plane_sizes[i];
  }
  if (!base::Contains(frames_decoded_count_, frame_index)) {
    frames_decoded_count_[frame_index] = 1;
  } else {
    frames_decoded_count_[frame_index]++;
  }
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
