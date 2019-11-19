// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_
#define CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_

#include "base/containers/stack_container.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/paint_export.h"

namespace cc {
// An ImageProvider that passes decode requests through to the
// |source_provider| but keeps the decode cached throughtout its lifetime,
// instead of passing the ref to the caller.
class CC_PAINT_EXPORT DecodeStashingImageProvider : public ImageProvider {
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
  ImageProvider* source_provider_;
  base::StackVector<ScopedResult, 1> decoded_images_;
};

}  // namespace cc

#endif  // CC_PAINT_DECODE_STASHING_IMAGE_PROVIDER_H_
