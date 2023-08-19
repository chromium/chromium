// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/decode_stashing_image_provider.h"

#include <utility>

namespace cc {
DecodeStashingImageProvider::DecodeStashingImageProvider(
    ImageProvider* source_provider)
    : source_provider_(source_provider) {
  DCHECK(source_provider_);
}
DecodeStashingImageProvider::~DecodeStashingImageProvider() = default;

ImageProvider::ScopedResult DecodeStashingImageProvider::GetRasterContent(
    const DrawImage& draw_image) {
  // TODO(xidachen): Ensure this function works with paint worklet generated
  // images.
  auto decode = source_provider_->GetRasterContent(draw_image);
  if (!decode.needs_unlock())
    return decode;

  // No need to add any destruction callback to the returned image. The images
  // decoded here match the lifetime of this provider.
  auto result = ScopedResult(decode.decoded_image());
  decoded_images_.push_back(std::move(decode));
  return result;
}

void DecodeStashingImageProvider::Reset() {
  decoded_images_.clear();
}

}  // namespace cc
