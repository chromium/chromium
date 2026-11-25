// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_IMAGE_PROVIDER_H_
#define CC_PAINT_IMAGE_PROVIDER_H_

#include <optional>
#include <utility>

#include "base/functional/callback.h"
#include "cc/paint/decoded_draw_image.h"
#include "cc/paint/draw_image.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_record.h"

namespace cc {
class PaintImage;

// Used to replace lazy generated PaintImages with decoded images for
// rasterization, or PaintWorklet-backed PaintImages with PaintRecords.
class CC_PAINT_EXPORT ImageProvider {
 public:
  class CC_PAINT_EXPORT ScopedResult {
   public:
    using DestructionCallback = base::OnceClosure;

    ScopedResult();
    explicit ScopedResult(DecodedDrawImage image);
    ScopedResult(DecodedDrawImage image, DestructionCallback callback);
    ScopedResult(const ScopedResult&) = delete;
    ScopedResult(ScopedResult&& other);
    ~ScopedResult();

    ScopedResult& operator=(const ScopedResult&) = delete;
    ScopedResult& operator=(ScopedResult&& other);

    explicit operator bool() const { return static_cast<bool>(image_); }
    const DecodedDrawImage& decoded_image() const { return image_; }
    bool needs_unlock() const { return !destruction_callback_.is_null(); }

   private:
    void DestroyDecode();

    DecodedDrawImage image_;
    DestructionCallback destruction_callback_;
  };

  virtual ~ImageProvider() = default;

  // Callers must check `PaintImage::IsDeferredPaintRecord()` (which includes
  // `IsPaintWorklet()`) before choosing an entry point: call `GetPaintRecord()`
  // if true, and `GetRasterContent()` otherwise.

  // Returns the DecodedDrawImage to use for this DrawImage. If no image is
  // provided, the draw for this image will be skipped during raster.
  virtual ScopedResult GetRasterContent(const DrawImage& draw_image) = 0;

  // Returns the PaintRecord produced by a paint worklet JS paint callback for
  // `paint_image`.
  virtual std::optional<PaintRecord> GetPaintRecord(
      const PaintImage& paint_image);
};

}  // namespace cc

#endif  // CC_PAINT_IMAGE_PROVIDER_H_
