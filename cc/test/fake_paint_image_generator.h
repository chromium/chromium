// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_
#define CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_

#include <vector>

#include "base/containers/flat_map.h"
#include "cc/paint/paint_image_generator.h"

namespace cc {

class FakePaintImageGenerator : public PaintImageGenerator {
 public:
  // RGB decoding mode constructor.
  explicit FakePaintImageGenerator(
      const SkImageInfo& info,
      std::vector<FrameMetadata> frames = {FrameMetadata()},
      bool allocate_discardable_memory = true,
      std::vector<SkISize> supported_sizes = {});
  // YUV decoding mode constructor.
  explicit FakePaintImageGenerator(
      const SkImageInfo& info,
      const SkYUVASizeInfo& yuva_size_info,
      std::vector<FrameMetadata> frames = {FrameMetadata()},
      bool allocate_discardable_memory = true,
      std::vector<SkISize> supported_sizes = {});
  FakePaintImageGenerator(const FakePaintImageGenerator&) = delete;
  ~FakePaintImageGenerator() override;

  FakePaintImageGenerator& operator=(const FakePaintImageGenerator&) = delete;

  // PaintImageGenerator implementation.
  sk_sp<SkData> GetEncodedData() const override;
  bool GetPixels(const SkImageInfo& info,
                 void* pixels,
                 size_t row_bytes,
                 size_t frame_index,
                 PaintImage::GeneratorClientId client_id,
                 uint32_t lazy_pixel_ref) override;
  bool QueryYUVA8(SkYUVASizeInfo* info,
                  SkYUVAIndex indices[],
                  SkYUVColorSpace* color_space) const override;
  bool GetYUVA8Planes(const SkYUVASizeInfo& info,
                      const SkYUVAIndex indices[],
                      void* planes[4],
                      size_t frame_index,
                      uint32_t lazy_pixel_ref) override;
  SkISize GetSupportedDecodeSize(const SkISize& requested_size) const override;
  const ImageHeaderMetadata* GetMetadataForDecodeAcceleration() const override;

  const base::flat_map<size_t, int>& frames_decoded() const {
    return frames_decoded_count_;
  }
  const std::vector<SkImageInfo>& decode_infos() const {
    CHECK(!is_yuv_);
    return decode_infos_;
  }
  void reset_frames_decoded() { frames_decoded_count_.clear(); }
  void SetExpectFallbackToRGB() { expect_fallback_to_rgb_ = true; }
  void SetImageHeaderMetadata(const ImageHeaderMetadata& image_metadata) {
    image_metadata_ = image_metadata;
  }

 private:
  std::vector<uint8_t> image_backing_memory_;
  SkPixmap image_pixmap_;
  base::flat_map<size_t, int> frames_decoded_count_;
  std::vector<SkISize> supported_sizes_;
  std::vector<SkImageInfo> decode_infos_;
  bool is_yuv_ = false;
  SkYUVASizeInfo yuva_size_info_;
  // TODO(skbug.com/8564): After Skia supports rendering from software YUV
  // planes and after Chrome implements it, we should no longer expect RGB
  // fallback.
  bool expect_fallback_to_rgb_ = false;
  ImageHeaderMetadata image_metadata_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_
