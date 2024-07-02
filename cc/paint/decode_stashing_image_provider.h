// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_
#define CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_

#include "base/memory/stack_allocated.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_export.h"
#include "third_party/abseil-cpp/absl/container/inlined_vector.h"

namespace cc {
// An ImageProvider that passes decode requests through to the
// |source_provider| but keeps the decode cached throughtout its lifetime,
// instead of passing the ref to the caller.
class CC_PAINT_EXPORT DecodeStashingImageProvider : public ImageProvider {
  STACK_ALLOCATED();

 public:
  // |source_provider| must outlive this class.
  explicit DecodeStashingImageProvider(ImageProvider* source_provider);
  DecodeStashingImageProvider(const DecodeStashingImageProvider&) = delete;
  ~DecodeStashingImageProvider() override;

  DecodeStashingImageProvider& operator=(const DecodeStashingImageProvider&) =
      delete;

  // ImageProvider implementation.
  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override;

  // Releases all stashed images. The caller must ensure that it is safe to
  // unlock any images acquired before this.
  void Reset();

 private:
  ImageProvider* source_provider_ = nullptr;
  absl::InlinedVector<ScopedResult, 1> decoded_images_;
};

}  // namespace cc

#endif  // CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_
