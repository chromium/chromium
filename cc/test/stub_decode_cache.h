// Copyright 2017 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_STUB_DECODE_CACHE_H_
#define CC_TEST_STUB_DECODE_CACHE_H_

#include "cc/tiles/image_decode_cache.h"

namespace cc {

class StubDecodeCache : public ImageDecodeCache {
 public:
  StubDecodeCache() = default;
  ~StubDecodeCache() override = default;

  TaskResult GetTaskForImageAndRef(ClientId client_id,
                                   const DrawImageWithTarget& image,
                                   const TracingInfo& tracing_info) override;
  TaskResult GetOutOfRasterDecodeTaskForImageAndRef(
      ClientId client_id,
      const DrawImageWithTarget& image,
      bool speculative) override;
  void UnrefImage(const DrawImageWithTarget& image) override {}
  DecodedDrawImage GetDecodedImageForDraw(
      const DrawImageWithTarget& image) override;
  void DrawWithImageFinished(const DrawImageWithTarget& image,
                             const DecodedDrawImage& decoded_image) override {}
  void ReduceCacheUsage() override {}
  void SetShouldAggressivelyFreeResources(
      bool aggressively_free_resources) override {}
  void ClearCache() override {}
  size_t GetMaximumMemoryLimitBytes() const override;
  bool UseCacheForDrawImage(const DrawImageWithTarget& image) const override;
  void RecordStats() override {}
};

}  // namespace cc

#endif  // CC_TEST_STUB_DECODE_CACHE_H_
