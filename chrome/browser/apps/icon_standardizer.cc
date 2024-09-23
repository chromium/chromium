// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/apps/icon_standardizer.h"

#include "base/trace_event/trace_event.h"
#include "third_party/skia/include/core/SkCanvas.h"
#include "third_party/skia/include/core/SkMaskFilter.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

namespace apps {

namespace {

constexpr float kCircleOutlineStrokeWidthRatio = 0.1f;

constexpr int kMinimumVisibleAlpha = 40;

constexpr float kCircleShapePixelDifferenceThreshold = 0.02f;

constexpr float kIconScaleToFit = 0.85f;

constexpr float kBackgroundCircleScale = 176.0f / 192.0f;

constexpr float kMinimumVisibleCircularIconSizeRatio = 0.625f;

constexpr float kMaximumVisibleCircularIconSizeRatio = 1.20f;

float GetDistanceBetweenPoints(gfx::PointF first_point,
                               gfx::PointF second_point) {
  float x_difference = first_point.x() - second_point.x();
  float y_difference = first_point.y() - second_point.y();
  return sqrt(x_difference * x_difference + y_difference * y_difference);
}

// Returns the distance for the farthest visible pixel away from the center of
// the icon.
float GetFarthestVisiblePointFromCenter(const SkBitmap& bitmap) {
  TRACE_EVENT0("ui", "apps::GetFarthestVisiblePointFromCenter");
  int width = bitmap.width();
  int height = bitmap.height();

  const SkPixmap pixmap = bitmap.pixmap();
  bool const nativeColorType = pixmap.colorType() == kN32_SkColorType;

  gfx::PointF center_point((width - 1) / 2.0f, (height - 1) / 2.0f);
  float max_distance = -1.0f;

  // Find the farthest visible pixel from the center by going through one row
  // at a time and for each row find the first and the last non-transparent
  // pixel and calculate its distance from the center.
  for (int y = 0; y < height; y++) {
    const SkColor* nativeRow =
        nativeColorType
            ? reinterpret_cast<const SkColor*>(bitmap.getAddr32(0, y))
            : nullptr;
    bool does_row_have_visible_pixels = false;

    for (int x = 0; x < width; x++) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMinimumVisibleAlpha) {
        gfx::PointF current_point(x, y);
        max_distance =
            std::max(GetDistanceBetweenPoints(current_point, center_point),
                     max_distance);

        does_row_have_visible_pixels = true;
        break;
      }
    }

    // No visible pixels on this row.
    if (!does_row_have_visible_pixels) {
      continue;
    }

    for (int x = width - 1; x > 0; x--) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMinimumVisibleAlpha) {
        gfx::PointF current_point(x, y);
        max_distance =
            std::max(GetDistanceBetweenPoints(current_point, center_point),
                     max_distance);

        break;
      }
    }
  }
  return (max_distance == -1.0f) ? (sqrt(width * width * 2)) : max_distance;
}

bool IsIconRepCircleShaped(const gfx::ImageSkiaRep& rep) {
  TRACE_EVENT0("ui", "apps::IsIconRepCircleShaped");
  SkBitmap bitmap(rep.GetBitmap());
  int width = bitmap.width();
  int height = bitmap.height();

  SkBitmap preview;
  preview.allocN32Pixels(width, height);
  preview.eraseColor(SK_ColorTRANSPARENT);

  // |preview| will be the original icon with all visible pixels colored red.
  for (int y = 0; y < height; y++) {
    const SkColor* src_color =
        reinterpret_cast<SkColor*>(bitmap.getAddr32(0, y));
    SkColor* preview_color =
        reinterpret_cast<SkColor*>(preview.getAddr32(0, y));

    for (int x = 0; x < width; x++) {
      SkColor target_color;

      if (SkColorGetA(src_color[x]) < 1) {
        target_color = SK_ColorTRANSPARENT;
      } else {
        target_color = SK_ColorRED;
      }

      preview_color[x] = target_color;
    }
  }

  const float circle_radius = GetFarthestVisiblePointFromCenter(preview);
  const float visible_icon_size_ratio =
      (circle_radius * 2) / static_cast<float>(width);

  // If the visible icon is too small or too large then it should not be
  // considered circular. This rules out small icons and large square shaped
  // icons.
  if (visible_icon_size_ratio < kMinimumVisibleCircularIconSizeRatio ||
      visible_icon_size_ratio > kMaximumVisibleCircularIconSizeRatio) {
    return false;
  }

  SkPoint circle_center = SkPoint::Make(width / 2, height / 2);

  // Use a canvas to perform XOR and DST_OUT operations, which should
  // generate a transparent bitmap for |preview| if the original icon is
  // shaped like a circle.
  SkCanvas canvas(preview);
  SkPaint paint_circle_mask;
  paint_circle_mask.setColor(SK_ColorBLUE);
  paint_circle_mask.setStyle(SkPaint::kFill_Style);
  paint_circle_mask.setAntiAlias(true);

  // XOR operation to remove a circle.
  paint_circle_mask.setBlendMode(SkBlendMode::kXor);
  canvas.drawCircle(circle_center, circle_radius, paint_circle_mask);

  SkPaint paint_outline;
  paint_outline.setColor(SK_ColorGREEN);
  paint_outline.setStyle(SkPaint::kStroke_Style);

  const float outline_stroke_width =
      circle_radius * 2 * kCircleOutlineStrokeWidthRatio;
  const float radius_offset = outline_stroke_width / 4.0f;

  paint_outline.setStrokeWidth(outline_stroke_width);
  paint_outline.setAntiAlias(true);

  // DST_OUT operation to remove an extra circle outline.
  paint_outline.setBlendMode(SkBlendMode::kDstOut);
  canvas.drawCircle(circle_center, circle_radius - radius_offset,
                    paint_outline);

  // Compute the total pixel difference between the circle mask and the
  // original icon.
  int total_pixel_difference = 0;
  for (int y = 0; y < preview.height(); ++y) {
    SkColor* src_color = reinterpret_cast<SkColor*>(preview.getAddr32(0, y));
    for (int x = 0; x < preview.width(); ++x) {
      if (SkColorGetA(src_color[x]) >= kMinimumVisibleAlpha) {
        total_pixel_difference++;
      }
    }
  }

  float percentage_diff_pixels =
      static_cast<float>(total_pixel_difference) / (width * height);

  // If the pixel difference between a circle and the original icon is small
  // enough, then the icon can be considered circle shaped.
  return (percentage_diff_pixels < kCircleShapePixelDifferenceThreshold);
}

std::optional<gfx::ImageSkiaRep> StandardizeSizeOfImageRep(
    const gfx::ImageSkiaRep& rep,
    float scale) {
  TRACE_EVENT0("ui", "apps::StandardizeSizeOfImageRep");
  SkBitmap unscaled_bitmap(rep.GetBitmap());
  int width = unscaled_bitmap.width();
  int height = unscaled_bitmap.height();

  if (width == height) {
    return std::nullopt;
  }

  int longest_side = std::max(width, height);

  SkBitmap final_bitmap;
  final_bitmap.allocN32Pixels(longest_side, longest_side);
  final_bitmap.eraseColor(SK_ColorTRANSPARENT);

  SkCanvas canvas(final_bitmap);
  canvas.drawImage(unscaled_bitmap.asImage(), (longest_side - width) / 2,
                   (longest_side - height) / 2);

  return gfx::ImageSkiaRep(final_bitmap, scale);
}

// Returns an image with equal width and height. If necessary, padding is
// added to ensure the width and height are equal.
gfx::ImageSkia StandardizeSize(const gfx::ImageSkia& image) {
  TRACE_EVENT0("ui", "apps::StandardizeSize");
  gfx::ImageSkia final_image;

  for (gfx::ImageSkiaRep rep : image.image_reps()) {
    std::optional<gfx::ImageSkiaRep> new_rep =
        StandardizeSizeOfImageRep(rep, rep.scale());
    if (!new_rep) {
      return image;
    }

    final_image.AddRepresentation(new_rep.value());
  }

  return final_image;
}

}  // namespace

std::optional<gfx::ImageSkiaRep> CreateStandardIconImageRep(
    const gfx::ImageSkiaRep& base_rep,
    float scale) {
  TRACE_EVENT0("ui", "apps::CreateStandardIconImageRep");
  std::optional<gfx::ImageSkiaRep> resized_image_skia_rep =
      StandardizeSizeOfImageRep(base_rep, scale);
  const gfx::ImageSkiaRep& standard_size_rep =
      resized_image_skia_rep.value_or(base_rep);

  SkBitmap unscaled_bitmap(standard_size_rep.GetBitmap());
  int width = unscaled_bitmap.width();
  int height = unscaled_bitmap.height();

  // If icon is already circle shaped, then return the original image and make
  // sure the image is scaled down if its icon size takes up too much space
  // within the bitmap.
  if (IsIconRepCircleShaped(standard_size_rep)) {
    float dis_to_center = GetFarthestVisiblePointFromCenter(unscaled_bitmap);
    float icon_to_bitmap_size_ratio = dis_to_center * 2.0f / width;

    if (icon_to_bitmap_size_ratio <= kBackgroundCircleScale) {
      // No need to scale down the icon, so just use the |unscaled_bitmap|.
      return std::nullopt;
    }
    SkBitmap final_bitmap;
    final_bitmap.allocN32Pixels(width, height);
    final_bitmap.eraseColor(SK_ColorTRANSPARENT);
    SkCanvas canvas(final_bitmap);
    SkPaint paint_icon;
    paint_icon.setMaskFilter(nullptr);
    paint_icon.setBlendMode(SkBlendMode::kSrcOver);

    float icon_scale = kBackgroundCircleScale / icon_to_bitmap_size_ratio;

    gfx::Size scaled_icon_size =
        gfx::ScaleToRoundedSize(standard_size_rep.pixel_size(), icon_scale);
    const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
        unscaled_bitmap, skia::ImageOperations::RESIZE_BEST,
        scaled_icon_size.width(), scaled_icon_size.height());

    int target_left = (width - scaled_icon_size.width()) / 2;
    int target_top = (height - scaled_icon_size.height()) / 2;

    // Draw the scaled down bitmap and add that to the final image.
    canvas.drawImage(scaled_bitmap.asImage(), target_left, target_top,
                     SkSamplingOptions(), &paint_icon);
    return gfx::ImageSkiaRep(final_bitmap, scale);
  }

  SkBitmap final_bitmap;
  final_bitmap.allocN32Pixels(width, height);
  final_bitmap.eraseColor(SK_ColorTRANSPARENT);

  // To draw to |final_bitmap|, create a canvas and draw a circle background
  // with an app icon on top;
  SkCanvas canvas(final_bitmap);
  SkPaint paint_background_circle;
  paint_background_circle.setAntiAlias(true);
  paint_background_circle.setColor(SK_ColorWHITE);
  paint_background_circle.setStyle(SkPaint::kFill_Style);

  float circle_diameter = width * kBackgroundCircleScale;

  // Draw the background circle.
  canvas.drawCircle(SkPoint::Make((width - 1) / 2.0f, (height - 1) / 2.0f),
                    circle_diameter / 2.0f, paint_background_circle);

  float dis_to_center = GetFarthestVisiblePointFromCenter(unscaled_bitmap);
  float icon_diameter = dis_to_center * 2.0f;
  float target_diameter = circle_diameter * kIconScaleToFit;

  // If the icon is too big to fit correctly within the background circle,
  // then set |icon_scale| to fit.
  float icon_scale = (icon_diameter > target_diameter)
                         ? target_diameter / icon_diameter
                         : 1.0f;

  SkPaint paint_icon;
  paint_icon.setMaskFilter(nullptr);
  paint_icon.setBlendMode(SkBlendMode::kSrcOver);

  if (icon_scale == 1.0f) {
    // Draw the unscaled icon on top of the background.
    canvas.drawImage(unscaled_bitmap.asImage(), 0, 0, SkSamplingOptions(),
                     &paint_icon);
  } else {
    gfx::Size scaled_icon_size =
        gfx::ScaleToRoundedSize(standard_size_rep.pixel_size(), icon_scale);
    const SkBitmap scaled_bitmap = skia::ImageOperations::Resize(
        unscaled_bitmap, skia::ImageOperations::RESIZE_BEST,
        scaled_icon_size.width(), scaled_icon_size.height());

    int target_left = (width - scaled_icon_size.width()) / 2;
    int target_top = (height - scaled_icon_size.height()) / 2;

    // Draw the scaled icon on top of the background.
    canvas.drawImage(scaled_bitmap.asImage(), target_left, target_top,
                     SkSamplingOptions(), &paint_icon);
  }

  return gfx::ImageSkiaRep(final_bitmap, scale);
}

gfx::ImageSkia CreateStandardIconImage(const gfx::ImageSkia& image) {
  TRACE_EVENT0("ui", "apps::CreateStandardIconImage");
  gfx::ImageSkia final_image;
  gfx::ImageSkia standard_size_image = StandardizeSize(image);

  for (gfx::ImageSkiaRep rep : standard_size_image.image_reps()) {
    std::optional<gfx::ImageSkiaRep> standard_rep =
        CreateStandardIconImageRep(rep, rep.scale());
    final_image.AddRepresentation(standard_rep.value_or(rep));
  }
  return final_image;
}

}  // namespace apps
