// Copyright 2025 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "base/containers/auto_spanification_helper.h"

#include <array>
#include <cstdint>

#include "base/containers/span.h"
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

}  // namespace

}  // namespace base::internal::spanification
