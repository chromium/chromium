// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/test/pixel_comparator.h"

#include <stdint.h>

#include <algorithm>

#include "base/check_op.h"
#include "base/logging.h"
#include "ui/gfx/geometry/rect.h"

namespace cc {

bool ExactPixelComparator::Compare(const SkBitmap& actual_bmp,
                                   const SkBitmap& expected_bmp) const {
  // Number of pixels with an error
  int error_pixels_count = 0;

  gfx::Rect error_bounding_rect = gfx::Rect();

  // Check that bitmaps have identical dimensions.
  DCHECK_EQ(actual_bmp.width(), expected_bmp.width());
  DCHECK_EQ(actual_bmp.height(), expected_bmp.height());

  for (int x = 0; x < actual_bmp.width(); ++x) {
    for (int y = 0; y < actual_bmp.height(); ++y) {
      SkColor actual_color = actual_bmp.getColor(x, y);
      SkColor expected_color = expected_bmp.getColor(x, y);
      if (discard_alpha_) {
        actual_color = SkColorSetA(actual_color, 0);
        expected_color = SkColorSetA(expected_color, 0);
      }
      if (actual_color != expected_color) {
        ++error_pixels_count;
        error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
      }
    }
  }

  if (error_pixels_count != 0) {
    LOG(ERROR) << "Number of pixel with an error: " << error_pixels_count;
    LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
    return false;
  }

  return true;
}

ManhattanDistancePixelComparator::ManhattanDistancePixelComparator(
    int tolerance)
    : tolerance_(tolerance) {}

bool ManhattanDistancePixelComparator::Compare(
    const SkBitmap& actual_bmp,
    const SkBitmap& expected_bmp) const {
  // Check that bitmaps have identical dimensions.
  DCHECK_EQ(actual_bmp.width(), expected_bmp.width());
  DCHECK_EQ(actual_bmp.height(), expected_bmp.height());

  for (int y = 0; y < actual_bmp.height(); ++y) {
    for (int x = 0; x < actual_bmp.width(); ++x) {
      SkColor actual_color = actual_bmp.getColor(x, y);
      SkColor expected_color = expected_bmp.getColor(x, y);

      int pixel_b = SkColorGetB(actual_color);
      int pixel_g = SkColorGetG(actual_color);
      int pixel_r = SkColorGetR(actual_color);

      int ref_pixel_b = SkColorGetB(expected_color);
      int ref_pixel_g = SkColorGetG(expected_color);
      int ref_pixel_r = SkColorGetR(expected_color);

      int manhattan_distance = std::abs(pixel_b - ref_pixel_b) +
                               std::abs(pixel_g - ref_pixel_g) +
                               std::abs(pixel_r - ref_pixel_r);

      if (manhattan_distance > tolerance_) {
        LOG(ERROR) << "Pixel test failed on (" << x << ", " << y << "). "
                   << "Manhattan distance: " << manhattan_distance << ".";
        return false;
      }
    }
  }

  return true;
}

bool FuzzyPixelComparator::Compare(const SkBitmap& actual_bmp,
                                   const SkBitmap& expected_bmp) const {
  // Check that bitmaps have identical dimensions.
  DCHECK_EQ(actual_bmp.width(), expected_bmp.width());
  DCHECK_EQ(actual_bmp.height(), expected_bmp.height());

  // Check that bitmaps are not empty.
  DCHECK_GT(actual_bmp.width(), 0);
  DCHECK_GT(actual_bmp.height(), 0);

  DCHECK_GE(max_abs_error_limit_, small_abs_error_limit_);

  // Number of pixels with an error.
  int error_pixels_count = 0;
  // Number of pixels with a small error.
  int small_error_pixels_count = 0;
  // Number of pixels with a critical error.
  int critial_error_pixels_count = 0;
  // The per channel sums of absolute errors over all pixels.
  int64_t sum_abs_error_r = 0;
  int64_t sum_abs_error_g = 0;
  int64_t sum_abs_error_b = 0;
  int64_t sum_abs_error_a = 0;
  // The per channel maximum absolute errors over all pixels.
  int max_abs_error_r = 0;
  int max_abs_error_g = 0;
  int max_abs_error_b = 0;
  int max_abs_error_a = 0;

  gfx::Rect error_bounding_rect = gfx::Rect();

  for (int x = 0; x < actual_bmp.width(); ++x) {
    for (int y = 0; y < actual_bmp.height(); ++y) {
      SkColor actual_color = actual_bmp.getColor(x, y);
      SkColor expected_color = expected_bmp.getColor(x, y);
      if (discard_alpha_) {
        actual_color = SkColorSetA(actual_color, 0);
        expected_color = SkColorSetA(expected_color, 0);
      }

      if (actual_color != expected_color) {
        ++error_pixels_count;

        // Compute per channel errors
        uint32_t expected_alpha = SkColorGetA(expected_color);
        int error_r = SkColorGetR(actual_color) - SkColorGetR(expected_color);
        int error_g = SkColorGetG(actual_color) - SkColorGetG(expected_color);
        int error_b = SkColorGetB(actual_color) - SkColorGetB(expected_color);
        int error_a = SkColorGetA(actual_color) - expected_alpha;
        int abs_error_r = std::abs(error_r);
        int abs_error_g = std::abs(error_g);
        int abs_error_b = std::abs(error_b);
        int abs_error_a = std::abs(error_a);

        // Increment small error counter if error is below threshold
        if (abs_error_r <= small_abs_error_limit_ &&
            abs_error_g <= small_abs_error_limit_ &&
            abs_error_b <= small_abs_error_limit_ &&
            abs_error_a <= small_abs_error_limit_) {
          ++small_error_pixels_count;
        }

        if (abs_error_a != 0 &&
            (expected_alpha == 0 || expected_alpha == 0xff)) {
          DCHECK(!discard_alpha_);
          ++critial_error_pixels_count;
        }

        // Update per channel maximum absolute errors
        max_abs_error_r = std::max(max_abs_error_r, abs_error_r);
        max_abs_error_g = std::max(max_abs_error_g, abs_error_g);
        max_abs_error_b = std::max(max_abs_error_b, abs_error_b);
        max_abs_error_a = std::max(max_abs_error_a, abs_error_a);

        // Update per channel absolute error sums
        sum_abs_error_r += abs_error_r;
        sum_abs_error_g += abs_error_g;
        sum_abs_error_b += abs_error_b;
        sum_abs_error_a += abs_error_a;
      }
    }
  }

  // Compute error metrics from collected data
  int pixels_count = actual_bmp.width() * actual_bmp.height();
  DCHECK(pixels_count);

  float large_error_pixels_percentage = 0.0f;
  float small_error_pixels_percentage = 0.0f;
  int large_error_pixels_count = error_pixels_count - small_error_pixels_count;
  large_error_pixels_percentage =
      static_cast<float>(large_error_pixels_count) / pixels_count * 100.0f;
  small_error_pixels_percentage =
      static_cast<float>(small_error_pixels_count) / pixels_count * 100.0f;

  float avg_abs_error_r = 0.0f;
  float avg_abs_error_g = 0.0f;
  float avg_abs_error_b = 0.0f;
  float avg_abs_error_a = 0.0f;
  if (error_pixels_count > 0) {
    avg_abs_error_r = static_cast<float>(sum_abs_error_r) / error_pixels_count;
    avg_abs_error_g = static_cast<float>(sum_abs_error_g) / error_pixels_count;
    avg_abs_error_b = static_cast<float>(sum_abs_error_b) / error_pixels_count;
    avg_abs_error_a = static_cast<float>(sum_abs_error_a) / error_pixels_count;
  }

  if (large_error_pixels_percentage > large_error_pixels_percentage_limit_ ||
      small_error_pixels_percentage > small_error_pixels_percentage_limit_ ||
      avg_abs_error_r > avg_abs_error_limit_ ||
      avg_abs_error_g > avg_abs_error_limit_ ||
      avg_abs_error_b > avg_abs_error_limit_ ||
      avg_abs_error_a > avg_abs_error_limit_ ||
      max_abs_error_r > max_abs_error_limit_ ||
      max_abs_error_g > max_abs_error_limit_ ||
      max_abs_error_b > max_abs_error_limit_ ||
      max_abs_error_a > max_abs_error_limit_ || critial_error_pixels_count) {
    if (small_abs_error_limit_) {
      LOG(ERROR) << "Percentage of pixels with errors >"
                 << small_abs_error_limit_ << ": "
                 << large_error_pixels_percentage;
      LOG(ERROR) << "Percentage of pixels with errors <="
                 << small_abs_error_limit_ << ": "
                 << small_error_pixels_percentage;
    } else {
      LOG(ERROR) << "Percentage of pixels with an error: "
                 << large_error_pixels_percentage;
    }
    LOG(ERROR) << "Average absolute error (excluding identical pixels): "
               << "R=" << avg_abs_error_r << " "
               << "G=" << avg_abs_error_g << " "
               << "B=" << avg_abs_error_b << " "
               << "A=" << avg_abs_error_a;
    LOG(ERROR) << "Largest absolute error: "
               << "R=" << max_abs_error_r << " "
               << "G=" << max_abs_error_g << " "
               << "B=" << max_abs_error_b << " "
               << "A=" << max_abs_error_a;
    LOG(ERROR) << "Critical errors: " << critial_error_pixels_count;

    for (int x = 0; x < actual_bmp.width(); ++x) {
      for (int y = 0; y < actual_bmp.height(); ++y) {
        SkColor actual_color = actual_bmp.getColor(x, y);
        SkColor expected_color = expected_bmp.getColor(x, y);
        if (discard_alpha_) {
          actual_color = SkColorSetA(actual_color, 0);
          expected_color = SkColorSetA(expected_color, 0);
        }
        if (actual_color != expected_color)
          error_bounding_rect.Union(gfx::Rect(x, y, 1, 1));
      }
    }
    LOG(ERROR) << "Error Bounding Box : " << error_bounding_rect.ToString();
    return false;
  } else {
    return true;
  }
}

}  // namespace cc
