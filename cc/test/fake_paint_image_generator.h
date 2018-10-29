// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_
#define CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_

#include "base/containers/flat_map.h"
#include "cc/paint/paint_image_generator.h"

namespace cc {

class FakePaintImageGenerator : public PaintImageGenerator {
 public:
  explicit FakePaintImageGenerator(
      const SkImageInfo& info,
      std::vector<FrameMetadata> frames = {FrameMetadata()},
      bool allocate_discardable_memory = true,
      std::vector<SkISize> supported_sizes = {});
  ~FakePaintImageGenerator() override;

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

  const base::flat_map<size_t, int>& frames_decoded() const {
    return frames_decoded_count_;
  }
  const std::vector<SkImageInfo>& decode_infos() const { return decode_infos_; }
  void reset_frames_decoded() { frames_decoded_count_.clear(); }

 private:
  std::vector<uint8_t> image_backing_memory_;
  SkPixmap image_pixmap_;
  base::flat_map<size_t, int> frames_decoded_count_;
  std::vector<SkISize> supported_sizes_;
  std::vector<SkImageInfo> decode_infos_;
};

}  // namespace cc

#endif  // CC_TEST_FAKE_PAINT_IMAGE_GENERATOR_H_
