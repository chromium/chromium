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

// Minimized mock of CRYPTO_BUFFER_data and CRYPTO_BUFFER_len defined in
// //third_party/boringssl/src/include/openssl/pool.h
struct CRYPTO_BUFFER {
  base::raw_ptr<uint8_t> data = nullptr;
  size_t len = 0;
};
const uint8_t* CRYPTO_BUFFER_data(const CRYPTO_BUFFER* buf) {
  return buf->data.get();
}
size_t CRYPTO_BUFFER_len(const CRYPTO_BUFFER* buf) {
  return buf->len;
}

TEST(AutoSpanificationHelperTest, CryptoBufferData) {
  std::array<uint8_t, 128> array;
  CRYPTO_BUFFER buffer = {array.data(), array.size()};

  base::span<const uint8_t> span = UNSAFE_CRYPTO_BUFFER_DATA(&buffer);
  EXPECT_EQ(span.data(), array.data());
  EXPECT_EQ(span.size(), array.size());
}

// Minimized mock of hb_buffer_get_glyph_infos and
// hb_buffer_get_glyph_positions defined in
// //third_party/harfbuzz-ng/src/src/hb-buffer.h
struct hb_glyph_info_t {};
struct hb_glyph_position_t {};
struct hb_buffer_t {
  base::raw_ptr<hb_glyph_info_t> info = nullptr;
  base::raw_ptr<hb_glyph_position_t> pos = nullptr;
  unsigned int len = 0;
};
hb_glyph_info_t* hb_buffer_get_glyph_infos(hb_buffer_t* buffer,
                                           unsigned int* length) {
  if (length) {
    *length = buffer->len;
  }
  return buffer->info.get();
}
hb_glyph_position_t* hb_buffer_get_glyph_positions(hb_buffer_t* buffer,
                                                   unsigned int* length) {
  if (length) {
    *length = buffer->len;
  }
  return buffer->pos.get();
}

TEST(AutoSpanificationHelperTest, HbBufferGetGlyphInfos) {
  std::array<hb_glyph_info_t, 128> info_array;
  hb_buffer_t buffer;
  unsigned int length = 0;
  base::span<hb_glyph_info_t> infos;

  buffer = {.info = info_array.data(), .len = info_array.size()};
  infos = UNSAFE_HB_BUFFER_GET_GLYPH_INFOS(&buffer, &length);
  EXPECT_EQ(infos.data(), info_array.data());
  EXPECT_EQ(infos.size(), info_array.size());
  EXPECT_EQ(length, info_array.size());

  infos = UNSAFE_HB_BUFFER_GET_GLYPH_INFOS(&buffer, nullptr);
  EXPECT_EQ(infos.data(), info_array.data());
  EXPECT_EQ(infos.size(), info_array.size());
}

TEST(AutoSpanificationHelperTest, HbBufferGetGlyphPositions) {
  std::array<hb_glyph_position_t, 128> pos_array;
  hb_buffer_t buffer;
  unsigned int length = 0;
  base::span<hb_glyph_position_t> positions;

  buffer = {.pos = pos_array.data(), .len = pos_array.size()};
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
  EXPECT_EQ(positions.data(), pos_array.data());
  EXPECT_EQ(positions.size(), pos_array.size());
  EXPECT_EQ(length, pos_array.size());

  buffer = {.pos = pos_array.data(), .len = pos_array.size()};
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, /*length=*/nullptr);
  EXPECT_EQ(positions.data(), pos_array.data());
  EXPECT_EQ(positions.size(), pos_array.size());

  buffer = {.pos = nullptr,
            .len = pos_array.size()};  // pos == nullptr, len != 0
  positions = UNSAFE_HB_BUFFER_GET_GLYPH_POSITIONS(&buffer, &length);
  EXPECT_EQ(positions.data(), nullptr);
  EXPECT_EQ(positions.size(), 0);  // The span's size is 0
  EXPECT_NE(length, 0);            // even when `length` is non-zero.
}

// Minimized mock of g_get_system_data_dirs
// https://web.mit.edu/barnowl/share/gtk-doc/html/glib/glib-Miscellaneous-Utility-Functions.html#g-get-system-data-dirs
using gchar = char;
constexpr auto kGlibSystemDataDirs =
    std::to_array<const gchar* const>({"foo", "bar", "baz", nullptr});
const gchar* const* g_get_system_data_dirs() {
  return kGlibSystemDataDirs.data();
}

TEST(AutoSpanificationHelperTest, GGetSystemDataDirs) {
  base::span<const gchar* const> dirs = UNSAFE_G_GET_SYSTEM_DATA_DIRS();
  EXPECT_EQ(dirs.data(), kGlibSystemDataDirs.data());
  EXPECT_EQ(dirs.size(), kGlibSystemDataDirs.size());
}

}  // namespace

}  // namespace base::internal::spanification
