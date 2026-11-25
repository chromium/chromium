// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SCOPED_RASTER_FLAGS_H_
#define CC_PAINT_SCOPED_RASTER_FLAGS_H_

#include <optional>

#include "base/compiler_specific.h"
#include "base/memory/stack_allocated.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {

struct RasterInducingValues;

// A helper class to modify the flags for raster. This includes resolving
// deferred colors, alpha folding from SaveLayers and decoding images.
class CC_PAINT_EXPORT ScopedRasterFlags {
  STACK_ALLOCATED();

 public:
  // |flags| and |image_provider| must outlive this class.
  template <class F>
    requires(std::is_same_v<F, float>)
  ScopedRasterFlags(const PaintFlags* flags LIFETIME_BOUND,
                    ImageProvider* image_provider LIFETIME_BOUND,
                    const SkMatrix& ctm,
                    int max_texture_size,
                    F alpha,
                    const RasterInducingValues* raster_inducing_values)
      : original_flags_(flags) {
    if (image_provider) {
      decode_stashing_image_provider_.emplace(image_provider);

      // We skip the op if any images fail to decode.
      DecodeImageShader(ctm);
      if (decode_failed_)
        return;
      DecodeRecordShader(ctm, max_texture_size);
      if (decode_failed_)
        return;
      DecodeFilter();
      if (decode_failed_)
        return;
    }

    if (alpha != 1.0f) {
      DCHECK(flags->SupportsFoldingAlpha());
      MutableFlags()->setAlphaf(flags->getAlphaf() * alpha);
    }

    AdjustStrokeIfNeeded(ctm);
    ResolveDeferredColor(raster_inducing_values);
  }
  ScopedRasterFlags(const ScopedRasterFlags&) = delete;
  ~ScopedRasterFlags();

  ScopedRasterFlags& operator=(const ScopedRasterFlags&) = delete;

  // The returned PaintFlags* must not outlive this object. Copies of *flags()
  // or Skia objects derived from it (e.g. an SkPaint passed to
  // SkCanvas::saveLayer()) may outlive this object only if the caller retains
  // any stashed image decodes via ReleaseDecodedImages() until their usage
  // completes.
  const PaintFlags* flags() const LIFETIME_BOUND {
    if (decode_failed_)
      return nullptr;

    return modified_flags_ ? &*modified_flags_ : original_flags_;
  }

  // Returns true if any decoded images are currently stashed by this object.
  bool HasDecodedImages() const {
    return decode_stashing_image_provider_ &&
           decode_stashing_image_provider_->HasDecodedImages();
  }

  // Moves out any stashed image decodes so the caller can keep them alive
  // beyond the lifetime of this ScopedRasterFlags instance.
  absl::InlinedVector<ImageProvider::ScopedResult, 1> ReleaseDecodedImages();

 private:
  void DecodeImageShader(const SkMatrix& ctm);
  void DecodeRecordShader(const SkMatrix& ctm, int max_texture_size);
  void DecodeFilter();

  void AdjustStrokeIfNeeded(const SkMatrix& ctm);
  void ResolveDeferredColor(const RasterInducingValues* raster_inducing_values);

  PaintFlags* MutableFlags() LIFETIME_BOUND {
    if (!modified_flags_)
      modified_flags_.emplace(*original_flags_);
    return &*modified_flags_;
  }

  const PaintFlags* original_flags_ = nullptr;
  std::optional<PaintFlags> modified_flags_;
  std::optional<DecodeStashingImageProvider> decode_stashing_image_provider_;
  bool decode_failed_ = false;
};

}  // namespace cc

#endif  // CC_PAINT_SCOPED_RASTER_FLAGS_H_
