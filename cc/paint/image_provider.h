// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_PROVIDER_H_
#define CC_PAINT_IMAGE_PROVIDER_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "base/types/optional_util.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_op_buffer.h"

namespace cc {
class PaintImage;

// Used to replace lazy generated PaintImages with decoded images for
// rasterization.
class CC_PAINT_EXPORT ImageProvider {
 public:
  class CC_PAINT_EXPORT ScopedResult {
   public:
    using DestructionCallback = base::OnceClosure;

    ScopedResult();
    explicit ScopedResult(DecodedDrawImage image);
    explicit ScopedResult(std::optional<PaintRecord> record);
    ScopedResult(DecodedDrawImage image, DestructionCallback callback);
    ScopedResult(const ScopedResult&) = delete;
    ScopedResult(ScopedResult&& other);
    ~ScopedResult();

    ScopedResult& operator=(const ScopedResult&) = delete;
    ScopedResult& operator=(ScopedResult&& other);

    explicit operator bool() const { return image_ || record_; }
    const DecodedDrawImage& decoded_image() const { return image_; }
    bool needs_unlock() const { return !destruction_callback_.is_null(); }

    bool has_paint_record() const { return record_.has_value(); }
    PaintRecord ReleaseAsRecord() {
      DCHECK(has_paint_record());
      return std::move(record_.value());
    }

   private:
    void DestroyDecode();

    DecodedDrawImage image_;
    std::optional<PaintRecord> record_;
    DestructionCallback destruction_callback_;
  };

  virtual ~ImageProvider() = default;

  // Returns either:
  // 1. The DecodedDrawImage to use for this PaintImage. If no image is
  // provided, the draw for this image will be skipped during raster. Or,
  // 2. The PaintRecord produced by paint worklet JS paint callback.
  virtual ScopedResult GetRasterContent(const DrawImage& draw_image) = 0;
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_PROVIDER_H_
