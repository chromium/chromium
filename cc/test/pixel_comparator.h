// Copyright 2013 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_TEST_PIXEL_COMPARATOR_H_
#define CC_TEST_PIXEL_COMPARATOR_H_

#include "base/compiler_specific.h"
#include "third_party/skia/include/core/SkBitmap.h"

namespace cc {

// Interface for pixel comparators.
class PixelComparator {
 public:
  virtual ~PixelComparator() = default;

  virtual bool Compare(const SkBitmap& actual_bmp,
                       const SkBitmap& expected_bmp) const = 0;
};

// Exact pixel comparator. Counts the number of pixel with an error.
class ExactPixelComparator : public PixelComparator {
 public:
  explicit ExactPixelComparator(const bool discard_alpha);
  ~ExactPixelComparator() override = default;

  // Returns true if the two bitmaps are identical. Otherwise, returns false
  // and report the number of pixels with an error on LOG(ERROR). Differences
  // in the alpha channel are ignored.
  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 private:
  // Exclude alpha channel from comparison?
  bool discard_alpha_;
};

// Different platforms have slightly different pixel output, due to different
// graphics implementations. Slightly different pixels (in BGR space) are still
// counted as a matching pixel by this simple manhattan distance threshold.
// If, at any pixel, the sum of the absolute differences in each color component
// (excluding alpha) exceeds the threshold the test is failed.
class ManhattanDistancePixelComparator : public PixelComparator {
 public:
  explicit ManhattanDistancePixelComparator(int tolerance = 25);
  ~ManhattanDistancePixelComparator() override = default;

  // Returns true if the two bitmaps are identical within the specified
  // manhattan distance. Otherwise, returns false and report the first pixel
  // that differed by more than the tolerance distance using a LOG(ERROR).
  // Differences in the alpha channel are ignored.
  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 private:
  const int tolerance_;
};

// Fuzzy pixel comparator. Counts small and arbitrary errors separately and
// computes average and maximum absolute errors per color channel.
class FuzzyPixelComparator : public PixelComparator {
 public:
  FuzzyPixelComparator(bool discard_alpha,
                       float error_pixels_percentage_limit,
                       float small_error_pixels_percentage_limit,
                       float avg_abs_error_limit,
                       int max_abs_error_limit,
                       int small_error_threshold,
                       bool check_critical_error = true);
  ~FuzzyPixelComparator() override = default;

  // Computes error metrics and returns true if the errors don't exceed the
  // specified limits. Otherwise, returns false and reports the error metrics on
  // LOG(ERROR). Differences in the alpha channel are ignored.
  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 private:
  // Exclude alpha channel from comparison?
  bool discard_alpha_;
  // Limit for percentage of pixels with an error.
  float error_pixels_percentage_limit_;
  // Limit for percentage of pixels with a small error.
  float small_error_pixels_percentage_limit_;
  // Limit for average absolute error (excluding identical pixels).
  float avg_abs_error_limit_;
  // Limit for largest absolute error.
  int max_abs_error_limit_;
  // Threshold for small errors.
  int small_error_threshold_;
  // If true, comparator will report critical errors. For example:
  // alpha value goes from 0 to 1 or 256 to 255.
  bool check_critical_error_;
};

// All pixels can be off by one, but any more than that is an error.
class FuzzyPixelOffByOneComparator : public FuzzyPixelComparator {
 public:
  explicit FuzzyPixelOffByOneComparator(bool discard_alpha)
      : FuzzyPixelComparator(discard_alpha, 100.f, 0.f, 1.f, 1, 0) {}
};

}  // namespace cc

#endif  // CC_TEST_PIXEL_COMPARATOR_H_
