// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/apps/icon_standardizer.h"

#include "testing/gtest/include/gtest/gtest.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkImage.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"

namespace {

constexpr int kIconSize = 64;

constexpr float kMaxCircleIconSize = 176.0f / 192.0f;
constexpr float kStandardCircleRadius = kIconSize * kMaxCircleIconSize / 2.0f;

// Returns whether the bitmaps are equal.
bool AreBitmapsEqual(const SkBitmap& first_bitmap,
                     const SkBitmap& second_bitmap) {
  const size_t size = first_bitmap.computeByteSize();
  bool bitmaps_equal = true;

  uint8_t* first_data = reinterpret_cast<uint8_t*>(first_bitmap.getPixels());
  uint8_t* second_data = reinterpret_cast<uint8_t*>(second_bitmap.getPixels());
  for (size_t i = 0; i < size; ++i) {
    if (first_data[i] != second_data[i]) {
      bitmaps_equal = false;
      break;
    }
  }

  return bitmaps_equal;
}

bool DoesIconHaveWhiteBackgroundCircle(const SkBitmap& bitmap) {
  const int y = kIconSize / 2;
  SkColor* src_color = reinterpret_cast<SkColor*>(bitmap.getAddr32(0, y));
  for (int x = 0; x < bitmap.width(); ++x) {
    if (src_color[x] == SK_ColorWHITE) {
      return true;
    }
  }
  return false;
}

}  // namespace

using CreateStandardIconTest = testing::Test;

// Test that a square icon gets scaled down and drawn on top of a circular
// background when converted to a standard icon.
TEST_F(CreateStandardIconTest, SquareIconToStandardIcon) {
  SkBitmap square_icon_bitmap;
  square_icon_bitmap.allocN32Pixels(kIconSize, kIconSize);
  square_icon_bitmap.eraseColor(SK_ColorRED);

  gfx::ImageSkia standard_icon = apps::CreateStandardIconImage(
      gfx::ImageSkia::CreateFrom1xBitmap(square_icon_bitmap));

  // Create |test_standard_bitmap| which will be a manually created standard
  // icon, with background circle and a scaled down square icon inside.
  SkBitmap test_standard_bitmap;
  test_standard_bitmap.allocN32Pixels(kIconSize, kIconSize);
  test_standard_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(test_standard_bitmap);

  SkPaint paint_background_circle;
  paint_background_circle.setAntiAlias(true);
  paint_background_circle.setColor(SK_ColorWHITE);
  paint_background_circle.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(
      SkPoint::Make((kIconSize - 1) / 2.0f, (kIconSize - 1) / 2.0f),
      kStandardCircleRadius, paint_background_circle);

  const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
      square_icon_bitmap, skia::ImageOperations::RESIZE_BEST, 36, 36);
  canvas.drawImage(scaled_bitmap.asImage(), 14, 14);

  EXPECT_TRUE(DoesIconHaveWhiteBackgroundCircle(*standard_icon.bitmap()));
  EXPECT_TRUE(AreBitmapsEqual(*standard_icon.bitmap(), test_standard_bitmap));
}

// Test that a large circular icon gets scaled down when converted to a standard
// icon.
TEST_F(CreateStandardIconTest, CircularIconToStandardIcon) {
  // Create a bitmap for drawing a red circle as a placeholder circular icon.
  SkBitmap circle_icon_bitmap;
  circle_icon_bitmap.allocN32Pixels(kIconSize, kIconSize);
  circle_icon_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(circle_icon_bitmap);
  SkPaint paint_circle_icon;
  paint_circle_icon.setAntiAlias(true);
  paint_circle_icon.setColor(SK_ColorRED);
  paint_circle_icon.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(SkPoint::Make(kIconSize / 2, kIconSize / 2), kIconSize / 2,
                    paint_circle_icon);

  // Get the standard icon version of the red circle icon.
  gfx::ImageSkia generated_standard_icon = apps::CreateStandardIconImage(
      gfx::ImageSkia::CreateFromBitmap(circle_icon_bitmap, 2.0f));

  // Scale the bitmap to fit the size of a standardized circle icon.
  SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
      circle_icon_bitmap, skia::ImageOperations::RESIZE_BEST, 58, 58);

  // Draw the |scaled_bitmap| to |manually_scaled_bitmap| to ensure the size
  // of the bitmap is the same as the |generated_standard_icon| for comparison.
  SkBitmap manually_scaled_bitmap;
  manually_scaled_bitmap.allocN32Pixels(kIconSize, kIconSize);
  manually_scaled_bitmap.eraseColor(SK_ColorTRANSPARENT);
  SkCanvas canvas2(manually_scaled_bitmap);
  canvas2.drawImage(scaled_bitmap.asImage(), 3, 3);

  EXPECT_FALSE(
      DoesIconHaveWhiteBackgroundCircle(*generated_standard_icon.bitmap()));
  EXPECT_TRUE(AreBitmapsEqual(*generated_standard_icon.bitmap(),
                              manually_scaled_bitmap));
}

// Test that a circle icon that is already standard size, keeps the same size
// when standardized.
TEST_F(CreateStandardIconTest, StandardCircularIconToStandardIcon) {
  // Create a bitmap with a red circle as a placeholder circular icon.
  SkBitmap circle_icon_bitmap;
  circle_icon_bitmap.allocN32Pixels(kIconSize, kIconSize);
  circle_icon_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(circle_icon_bitmap);
  SkPaint paint_circle_icon;
  paint_circle_icon.setAntiAlias(true);
  paint_circle_icon.setColor(SK_ColorRED);
  paint_circle_icon.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(SkPoint::Make(kIconSize / 2.0f, kIconSize / 2.0f),
                    kStandardCircleRadius, paint_circle_icon);

  // Get the standard icon version of the red circle icon.
  gfx::ImageSkia standard_icon = apps::CreateStandardIconImage(
      gfx::ImageSkia::CreateFromBitmap(circle_icon_bitmap, 2.0f));

  EXPECT_FALSE(DoesIconHaveWhiteBackgroundCircle(*standard_icon.bitmap()));
  EXPECT_TRUE(AreBitmapsEqual(*standard_icon.bitmap(), circle_icon_bitmap));
}

// Test that a circle icon that has an extra opaque area near the outside of the
// circle will have a background circle added when standardized.
TEST_F(CreateStandardIconTest, AlmostCircularIconToStandardIcon) {
  // Create a bitmap with a red circle as a placeholder circular icon.
  SkBitmap almost_circle_icon_bitmap;
  almost_circle_icon_bitmap.allocN32Pixels(kIconSize, kIconSize);
  almost_circle_icon_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(almost_circle_icon_bitmap);
  SkPaint paint_flags;
  paint_flags.setAntiAlias(true);
  paint_flags.setColor(SK_ColorRED);
  paint_flags.setStyle(SkPaint::kFill_Style);
  canvas.drawCircle(SkPoint::Make(kIconSize / 2.0f, kIconSize / 2.0f),
                    kStandardCircleRadius, paint_flags);

  // Draw a small square partially outside of the main red circle.
  canvas.drawRect(SkRect::MakeXYWH(6, 6, 15, 15), paint_flags);

  // Get the standard icon version of the almost red circle icon.
  gfx::ImageSkia standard_icon = apps::CreateStandardIconImage(
      gfx::ImageSkia::CreateFromBitmap(almost_circle_icon_bitmap, 2.0f));

  EXPECT_TRUE(DoesIconHaveWhiteBackgroundCircle(*standard_icon.bitmap()));
  EXPECT_FALSE(
      AreBitmapsEqual(*standard_icon.bitmap(), almost_circle_icon_bitmap));
}
