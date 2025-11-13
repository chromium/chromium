// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifdef UNSAFE_BUFFERS_BUILD
// TODO(crbug.com/40285824): Remove this and convert code to safer constructs.
#pragma allow_unsafe_buffers
#endif

#include "chrome/browser/ash/app_list/md_icon_normalizer.h"

#include <algorithm>
#include <cmath>
#include <numbers>
#include <utility>
#include <vector>

#include "base/trace_event/trace_event.h"
#include "skia/ext/image_operations.h"
#include "third_party/skia/include/core/SkBitmap.h"
#include "ui/gfx/canvas.h"
#include "ui/gfx/image/image_skia.h"
#include "ui/gfx/image/image_skia_operations.h"
#include "ui/gfx/image/image_skia_rep.h"

// The implementation is copied and adapted from the Android Launcher.
// See com.android.launcher3.graphics.IconNormalizer.java in the Android source.

namespace app_list {

namespace {

// No normalization to be attempted for icons smaller than this size.
constexpr int kMinIconSize = 32;

// Ratio of icon visible area to full icon size for a square shaped icon.
constexpr float kMaxSquareAreaFactor = 361.0f / 576;

// Ratio of icon visible area to full icon size for a circular shaped icon.
constexpr float kMaxCircleAreaFactor = 380.0f / 576;

constexpr float kCircleAreaByRect = std::numbers::pi_v<float> / 4;

// Slope used to calculate icon visible area to full icon size for any generic
// shaped icon.
constexpr float kLinearScaleSlope =
    (kMaxCircleAreaFactor - kMaxSquareAreaFactor) / (1 - kCircleAreaByRect);

constexpr int kMaxShadowAlpha = 40;

void ConvertToConvexArray(std::vector<float>* x_coord,
                          int direction,
                          int y_from,
                          int y_to) {
  TRACE_EVENT0("ui", "app_list::ConvertToConvexArray");
  std::vector<float> angles(y_to - y_from);

  int y_last = -1;  // Last valid y coordinate which didn't have a missing value

  float last_angle;

  for (int i = y_from + 1; i <= y_to; i++) {
    if ((*x_coord)[i] <= -1)
      continue;

    int start;

    if (y_last == -1) {
      start = y_from;
    } else {
      float current_angle = ((*x_coord)[i] - (*x_coord)[y_last]) / (i - y_last);
      start = y_last;
      // If this position creates a concave angle, keep moving up until we find
      // a position which creates a convex angle.
      if ((current_angle - last_angle) * direction < 0) {
        while (start > y_from) {
          start--;
          current_angle = ((*x_coord)[i] - (*x_coord)[start]) / (i - start);
          if ((current_angle - angles[start - y_from]) * direction >= 0)
            break;
        }
      }
    }

    // Reset from last check
    last_angle = ((*x_coord)[i] - (*x_coord)[start]) / (i - start);
    // Update all the points from start.
    for (int j = start; j < i; j++) {
      angles[j - y_from] = last_angle;
      (*x_coord)[j] = (*x_coord)[start] + last_angle * (j - start);
    }
    y_last = i;
  }
}

float GetMdIconScale(const SkBitmap& bitmap) {
  TRACE_EVENT0("ui", "app_list::GetMdIconScale");
  const SkPixmap pixmap = bitmap.pixmap();

  // In the absence of alpha information, assume that the icon is a fully opaque
  // square and scale accordingly.
  if (pixmap.alphaType() == kUnknown_SkAlphaType ||
      pixmap.alphaType() == kOpaque_SkAlphaType) {
    return std::sqrt(kMaxSquareAreaFactor);
  }

  bool const nativeColorType = pixmap.colorType() == kN32_SkColorType;

  const int width = pixmap.width();
  const int height = pixmap.height();

  // If the icon is too small, no scaling makes sense.
  if (std::min(width, height) < kMinIconSize)
    return 1;

  std::vector<float> border_left(height, -1);
  std::vector<float> border_right(height, -1);

  // Overall bounds of the visible icon.
  int y_from = -1;
  int y_to = -1;
  int x_left = width;
  int x_right = -1;

  // Create border by going through all pixels one row at a time and for each
  // row find the first and the last non-transparent pixel. Set those values to
  // border_left and border_right and use -1 if there are no visible pixel in
  // the row.

  for (int y = 0; y < height; y++) {
    const SkColor* nativeRow =
        nativeColorType
            ? reinterpret_cast<const SkColor*>(bitmap.getAddr32(0, y))
            : nullptr;

    for (int x = 0; x < width; x++) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMaxShadowAlpha) {
        border_left[y] = x;
        x_left = std::min(x_left, x);
        break;
      }
    }

    // No visible pixels on this row.
    if (border_left[y] == -1)
      continue;

    for (int x = width - 1; x > 0; x--) {
      if (SkColorGetA(nativeRow ? nativeRow[x] : pixmap.getColor(x, y)) >
          kMaxShadowAlpha) {
        border_right[y] = x;
        x_right = std::max(x_right, x);
        break;
      }
    }

    y_to = y;
    if (y_from == -1)
      y_from = y;
  }

  if (y_from == -1) {
    // No valid pixels found. Do not scale.
    return 1;
  }

  ConvertToConvexArray(&border_left, 1, y_from, y_to);
  ConvertToConvexArray(&border_right, -1, y_from, y_to);

  // Area of the convex hull
  float area = 0;
  for (int y = 0; y < height; y++) {
    if (border_left[y] <= -1)
      continue;
    area += border_right[y] - border_left[y] + 1;
  }

  // Area of the rectangle required to fit the convex hull
  float rect_area = (y_to + 1 - y_from) * (x_right + 1 - x_left);
  float hull_by_rect = area / rect_area;

  float scale_required;
  if (hull_by_rect < kCircleAreaByRect) {
    scale_required = kMaxCircleAreaFactor;
  } else {
    scale_required =
        kMaxSquareAreaFactor + kLinearScaleSlope * (1 - hull_by_rect);
  }

  float area_scale = area / (width * height);
  // Use sqrt of the final ratio as the image is scaled across both width and
  // height.
  return area_scale > scale_required ? std::sqrt(scale_required / area_scale)
                                     : 1.0f;
}

}  // namespace

gfx::Size GetMdIconPadding(const SkBitmap& bitmap,
                           const gfx::Size& required_size) {
  const float scale = GetMdIconScale(bitmap);
  const float padding_factor = (1 - scale) / 2;
  return gfx::Size(
      static_cast<int>(required_size.width() * padding_factor + 0.5),
      static_cast<int>(required_size.height() * padding_factor + 0.5));
}

void MaybeResizeAndPad(const gfx::Size& required_size,
                       const gfx::Size& padding,
                       SkBitmap* bitmap_out) {
  TRACE_EVENT0("ui", "app_list::MaybeResizeAndPad");
  if (!padding.width() && !padding.height() &&
      required_size.width() == bitmap_out->width() &&
      required_size.height() == bitmap_out->height()) {
    // Neither padding no resizing required, do nothing.
    return;
  }

  const int resized_width = required_size.width() - 2 * padding.width();
  const int resized_height = required_size.height() - 2 * padding.height();
  const SkBitmap resized = skia::ImageOperations::Resize(
      *bitmap_out, skia::ImageOperations::RESIZE_LANCZOS3, resized_width,
      resized_height);
  if (!padding.width() && !padding.height()) {
    // No padding required, return the resized bitmap.
    *bitmap_out = resized;
    return;
  }

  // Add padding.
  gfx::Canvas canvas(required_size, 1, /*transparent=*/false);
  canvas.DrawImageInt(gfx::ImageSkia::CreateFromBitmap(resized, 1),
                      padding.width(), padding.height());
  *bitmap_out = canvas.GetBitmap();
  return;
}

void MaybeResizeAndPadIconForMd(const gfx::Size& required_size_dip,
                                gfx::ImageSkia* icon_out) {
  TRACE_EVENT0("ui", "app_list::MaybeResizeAndPadIconForMd");
  bool transformation_required = false;

  // First pass over representations, collect transformation parameters.
  std::vector<std::pair<gfx::Size, gfx::Size>> params;
  for (gfx::ImageSkiaRep rep : icon_out->image_reps()) {
    const SkBitmap& bitmap = rep.GetBitmap();

    gfx::Size required_size_px(
        static_cast<int>(required_size_dip.width() * rep.scale() + 0.5),
        static_cast<int>(required_size_dip.height() * rep.scale() + 0.5));

    const gfx::Size padding_px(GetMdIconPadding(bitmap, required_size_px));

    params.push_back(std::make_pair(required_size_px, padding_px));

    if (required_size_px.width() != bitmap.width() ||
        required_size_px.height() != bitmap.height() ||
        padding_px.width() != 0 || padding_px.height() != 0) {
      transformation_required = true;
    }
  }

  if (!transformation_required)
    return;

  // Second pass over representations, apply transformations.
  gfx::ImageSkia transformed;
  int i = 0;
  for (gfx::ImageSkiaRep rep : icon_out->image_reps()) {
    SkBitmap bitmap = rep.GetBitmap();
    auto param = params[i++];
    MaybeResizeAndPad(param.first, param.second, &bitmap);
    transformed.AddRepresentation(gfx::ImageSkiaRep(bitmap, rep.scale()));
  }
  *icon_out = transformed;
}

float GetMdIconScaleForTest(const SkBitmap& icon) {
  return GetMdIconScale(icon);
}

}  // namespace app_list
