// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_TEST_OPTIONS_PROVIDER_H_
#define CC_TEST_TEST_OPTIONS_PROVIDER_H_

#include "base/containers/flat_map.h"
#include "cc/paint/image_provider.h"
#include "cc/paint/image_transfer_cache_entry.h"
#include "cc/paint/paint_cache.h"
#include "cc/paint/paint_op_buffer.h"
#include "cc/test/test_skcanvas.h"
#include "cc/test/transfer_cache_test_helper.h"
#include "third_party/skia/src/core/SkRemoteGlyphCache.h"

namespace cc {

class TestOptionsProvider : public ImageProvider,
                            public TransferCacheTestHelper {
 public:
  TestOptionsProvider();
  ~TestOptionsProvider() override;

  const PaintOp::SerializeOptions& serialize_options() const {
    return serialize_options_;
  }
  PaintOp::SerializeOptions& mutable_serialize_options() {
    return serialize_options_;
  }
  const PaintOp::DeserializeOptions& deserialize_options() const {
    return deserialize_options_;
  }
  PaintOp::DeserializeOptions& mutable_deserialize_options() {
    return deserialize_options_;
  }
  ImageProvider* image_provider() { return this; }
  TransferCacheTestHelper* transfer_cache_helper() { return this; }

  ClientPaintCache* client_paint_cache() { return &client_paint_cache_; }
  ServicePaintCache* service_paint_cache() { return &service_paint_cache_; }

  SkStrikeServer* strike_server() { return &strike_server_; }
  SkStrikeClient* strike_client() { return &strike_client_; }
  sk_sp<SkColorSpace> color_space() { return color_space_; }
  bool can_use_lcd_text() const { return can_use_lcd_text_; }
  bool context_supports_distance_field_text() const {
    return context_supports_distance_field_text_;
  }
  int max_texture_size() const { return max_texture_size_; }
  size_t max_texture_bytes() const { return max_texture_bytes_; }

  const std::vector<DrawImage>& decoded_images() const {
    return decoded_images_;
  }

  void PushFonts();
  void ClearPaintCache();

 private:
  class DiscardableManager;

  // ImageProvider implementation.
  ImageProvider::ScopedResult GetRasterContent(
      const DrawImage& draw_image) override;

  testing::StrictMock<MockCanvas> canvas_;
  std::vector<DrawImage> decoded_images_;

  sk_sp<DiscardableManager> discardable_manager_;
  SkStrikeServer strike_server_;
  SkStrikeClient strike_client_;
  sk_sp<SkColorSpace> color_space_;
  bool can_use_lcd_text_ = true;
  bool context_supports_distance_field_text_ = true;
  int max_texture_size_ = 1024;
  size_t max_texture_bytes_ = 4 * 1024 * 1024;

  ServicePaintCache service_paint_cache_;
  ClientPaintCache client_paint_cache_;
  std::vector<uint8_t> scratch_buffer_;

  PaintOp::SerializeOptions serialize_options_;
  PaintOp::DeserializeOptions deserialize_options_;
};

}  // namespace cc

#endif  // CC_TEST_TEST_OPTIONS_PROVIDER_H_
