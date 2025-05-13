// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/auto_spanification_helper.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
#include "base/memory/raw_ptr.h"
#include "testing/gtest/include/gtest/gtest.h"

namespace base::internal::spanification {

namespace {

// Minimized mock of SkBitmap class defined in
// //third_party/skia/include/core/SkBitmap.h
class SkBitmap {
 public:
  uint32_t* getAddr32(int x, int y) const { return &row_[x]; }
  int width() const { return static_cast<int>(row_.size()); }

  mutable std::array<uint32_t, 128> row_{};
};

// The main purpose of the following test cases is to exercise C++ compilation
// on the macro usage rather than testing their behaviors.

TEST(AutoSpanificationHelperTest, SkBitmapGetAddr32Pointer) {
  SkBitmap sk_bitmap;
  const int x = 123;
  base::span<uint32_t> span = UNSAFE_SKBITMAP_GETADDR32(&sk_bitmap, x, 0);
  EXPECT_EQ(span.data(), &sk_bitmap.row_[x]);
  EXPECT_EQ(span.size(), sk_bitmap.row_.size() - x);
}

TEST(AutoSpanificationHelperTest, SkBitmapGetAddr32Reference) {
  SkBitmap sk_bitmap;
  const int x = 123;
  base::span<uint32_t> span = UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, x, 0);
  EXPECT_EQ(span.data(), &sk_bitmap.row_[x]);
  EXPECT_EQ(span.size(), sk_bitmap.row_.size() - x);
}

TEST(AutoSpanificationHelperTest, SkBitmapGetAddr32SmartPtr) {
  std::unique_ptr<SkBitmap> sk_bitmap = std::make_unique<SkBitmap>();
  const int x = 123;
  base::span<uint32_t> span = UNSAFE_SKBITMAP_GETADDR32(sk_bitmap, x, 0);
  EXPECT_EQ(span.data(), &sk_bitmap->row_[x]);
  EXPECT_EQ(span.size(), sk_bitmap->row_.size() - x);
}

// Minimized mock of hb_buffer_get_glyph_positions defined in
// //third_party/perl/c/include/harfbuzz/hb-buffer.h
struct hb_glyph_position_t {};
struct hb_buffer_t {
  base::raw_ptr<hb_glyph_position_t> pos = nullptr;
  unsigned int len = 0;
};
hb_glyph_position_t* hb_buffer_get_glyph_positions(hb_buffer_t* buffer,
                                                   unsigned int* length) {
  if (length) {
    *length = buffer->len;
  }
  return buffer->pos.get();
}

TEST(AutoSpanificationHelperTest, HbBufferGetGlyphPositions) {
  std::array<hb_glyph_position_t, 128> pos_array;
  hb_buffer_t buffer;
  unsigned int length = 0;
  base::span<hb_glyph_position_t> positions;

  buffer = {pos_array.data(), pos_array.size()};
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
  EXPECT_EQ(positions.data(), pos_array.data());
  EXPECT_EQ(positions.size(), pos_array.size());
  EXPECT_EQ(length, pos_array.size());

  buffer = {pos_array.data(), pos_array.size()};
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, /*length=*/nullptr);
  EXPECT_EQ(positions.data(), pos_array.data());
  EXPECT_EQ(positions.size(), pos_array.size());

  buffer = {nullptr, pos_array.size()};  // pos == nullptr, len != 0
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
  EXPECT_EQ(positions.data(), nullptr);
  EXPECT_EQ(positions.size(), 0);  // The span's size is 0
  EXPECT_NE(length, 0);            // even when `length` is non-zero.
}

}  // namespace

}  // namespace base::internal::spanification
