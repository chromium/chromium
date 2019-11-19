// Copyright 2017 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/paint/image_provider.h"

#include "cc/paint/paint_record.h"

namespace cc {

ImageProvider::ScopedResult::ScopedResult() = default;

ImageProvider::ScopedResult::ScopedResult(DecodedDrawImage image)
    : image_(std::move(image)) {}

ImageProvider::ScopedResult::ScopedResult(sk_sp<PaintRecord> record)
    : record_(std::move(record)) {}

ImageProvider::ScopedResult::ScopedResult(DecodedDrawImage image,
                                          DestructionCallback callback)
    : image_(std::move(image)), destruction_callback_(std::move(callback)) {}

ImageProvider::ScopedResult::ScopedResult(ScopedResult&& other)
    : image_(std::move(other.image_)),
      record_(std::move(other.record_)),
      destruction_callback_(std::move(other.destruction_callback_)) {}

ImageProvider::ScopedResult& ImageProvider::ScopedResult::operator=(
    ScopedResult&& other) {
  DestroyDecode();

  image_ = std::move(other.image_);
  record_ = std::move(other.record_);
  destruction_callback_ = std::move(other.destruction_callback_);
  return *this;
}

ImageProvider::ScopedResult::~ScopedResult() {
  DestroyDecode();
}

void ImageProvider::ScopedResult::DestroyDecode() {
  if (!destruction_callback_.is_null())
    std::move(destruction_callback_).Run();
}

}  // namespace cc
