// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_SCOPED_RASTER_FLAGS_H_
#define CC_PAINT_SCOPED_RASTER_FLAGS_H_

#include <optional>

#include "base/memory/stack_allocated.h"
#include "cc/paint/decode_stashing_image_provider.h"
#include "cc/paint/paint_export.h"
#include "cc/paint/paint_flags.h"

namespace cc {

// A helper class to modify the flags for raster. This includes alpha folding
// from SaveLayers and decoding images.
class CC_PAINT_EXPORT ScopedRasterFlags {
  STACK_ALLOCATED();

 public:
  // |flags| and |image_provider| must outlive this class.
  template <class F, class = std::enable_if_t<std::is_same_v<F, float>>>
  ScopedRasterFlags(const PaintFlags* flags,
                    ImageProvider* image_provider,
                    const SkMatrix& ctm,
                    int max_texture_size,
                    F alpha)
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
  }
  ScopedRasterFlags(const ScopedRasterFlags&) = delete;
  ~ScopedRasterFlags();

  ScopedRasterFlags& operator=(const ScopedRasterFlags&) = delete;

  // The usage of these flags should not extend beyond the lifetime of this
  // object.
  const PaintFlags* flags() const {
    if (decode_failed_)
      return nullptr;

    return modified_flags_ ? &*modified_flags_ : original_flags_;
  }

 private:
  void DecodeImageShader(const SkMatrix& ctm);
  void DecodeRecordShader(const SkMatrix& ctm, int max_texture_size);
  void DecodeFilter();

  void AdjustStrokeIfNeeded(const SkMatrix& ctm);

  PaintFlags* MutableFlags() {
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
