// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SKIA_PAINT_IMAGE_GENERATOR_H_
#define CC_PAINT_SKIA_PAINT_IMAGE_GENERATOR_H_

#include "cc/paint/paint_export.h"
#include "cc/paint/paint_image.h"
#include "third_party/skia/include/core/SkImageGenerator.h"

namespace cc {
class PaintImageGenerator;

class CC_PAINT_EXPORT SkiaPaintImageGenerator final : public SkImageGenerator {
 public:
  SkiaPaintImageGenerator(sk_sp<PaintImageGenerator> paint_image_generator,
                          size_t frame_index,
                          PaintImage::GeneratorClientId client_id);
  SkiaPaintImageGenerator(const SkiaPaintImageGenerator&) = delete;
  ~SkiaPaintImageGenerator() override;

  SkiaPaintImageGenerator& operator=(const SkiaPaintImageGenerator&) = delete;

  sk_sp<SkData> onRefEncodedData() override;
  bool onGetPixels(const SkImageInfo&,
                   void* pixels,
                   size_t row_bytes,
                   const Options& options) override;
  bool onQueryYUVA8(SkYUVASizeInfo* size_info,
                    SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                    SkYUVColorSpace* color_space) const override;
  bool onGetYUVA8Planes(const SkYUVASizeInfo& size_info,
                        const SkYUVAIndex indices[SkYUVAIndex::kIndexCount],
                        void* planes[3]) override;

 private:
  sk_sp<PaintImageGenerator> paint_image_generator_;
  const size_t frame_index_;
  const PaintImage::GeneratorClientId client_id_;
};

}  // namespace cc

#endif  // CC_PAINT_SKIA_PAINT_IMAGE_GENERATOR_H_
