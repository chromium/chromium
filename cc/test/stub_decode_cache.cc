// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/stub_decode_cache.h"

namespace cc {

ImageDecodeCache::TaskResult StubDecodeCache::GetTaskForImageAndRef(
    ClientId client_id,
    const DrawImage& image,
    const TracingInfo& tracing_info) {
  return TaskResult(/*need_unref=*/true, /*is_at_raster_decode=*/false);
}

ImageDecodeCache::TaskResult
StubDecodeCache::GetOutOfRasterDecodeTaskForImageAndRef(ClientId client_id,
                                                        const DrawImage& image,
                                                        bool speculative) {
  return TaskResult(/*need_unref=*/true, /*is_at_raster_decode=*/false);
}

DecodedDrawImage StubDecodeCache::GetDecodedImageForDraw(
    const DrawImage& image) {
  return DecodedDrawImage();
}

size_t StubDecodeCache::GetMaximumMemoryLimitBytes() const {
  return 0u;
}

bool StubDecodeCache::UseCacheForDrawImage(const DrawImage& image) const {
  return true;
}

}  // namespace cc
