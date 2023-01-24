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
  // Returns true if the two bitmaps are identical. Otherwise, returns false
  // and report the number of pixels with an error on LOG(ERROR).
  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 protected:
  // Exclude alpha channel from comparison?
  bool discard_alpha_ = false;
};

class AlphaDiscardingExactPixelComparator : public ExactPixelComparator {
 public:
  AlphaDiscardingExactPixelComparator() { discard_alpha_ = true; }
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
// computes average and maximum absolute errors per color channel. It can be
// configured to discard alpha channel. If alpha channel is not discarded
// (by default), alpha changing among fully transparent, translucent and fully
// opaque (e.g. 0 to 1, 254 to 255) are always reported.
class FuzzyPixelComparator : public PixelComparator {
 public:
  FuzzyPixelComparator& DiscardAlpha() {
    discard_alpha_ = true;
    return *this;
  }
  // Sets the limits for percentages of pixels with errors:
  // - large errors: >small_abs_error_limit_,
  // - small errors: >0 and <= small_abs_error_limit_.
  // If small_abs_error_limit_ is zero (by default),
  // `large_error_pixels_percentage_limit` is for all errors.
  FuzzyPixelComparator& SetErrorPixelsPercentageLimit(
      float large_error_pixels_percentage_limit,
      float small_error_pixels_percentage_limit = 0) {
    large_error_pixels_percentage_limit_ = large_error_pixels_percentage_limit;
    small_error_pixels_percentage_limit_ = small_error_pixels_percentage_limit;
    return *this;
  }
  // Sets limit for average absolute error (excluding identical pixels).
  // The default is 255.0, which means it's not checked.
  FuzzyPixelComparator& SetAvgAbsErrorLimit(float avg_abs_error_limit) {
    avg_abs_error_limit_ = avg_abs_error_limit;
    return *this;
  }
  // Sets limits of the largest absolute error and the small absolute error.
  // The default of `max_abs_error_limit` is 255, which means it's not checked
  // by default. The default of `small_abs_eror_limit` is 0, which means all
  // errors are treated as large errors by default.
  FuzzyPixelComparator& SetAbsErrorLimit(int max_abs_error_limit,
                                         int small_abs_error_limit = 0) {
    max_abs_error_limit_ = max_abs_error_limit;
    small_abs_error_limit_ = small_abs_error_limit;
    return *this;
  }

  // Computes error metrics and returns true if the errors don't exceed the
  // specified limits. Otherwise, returns false and reports the error metrics on
  // LOG(ERROR).
  bool Compare(const SkBitmap& actual_bmp,
               const SkBitmap& expected_bmp) const override;

 private:
  // Exclude alpha channel from comparison?
  bool discard_alpha_ = false;
  float large_error_pixels_percentage_limit_ = 0;
  // Limit for percentage of pixels with a small error
  // (>0 and <= small_abs_error_limit_).
  float small_error_pixels_percentage_limit_ = 0;
  float avg_abs_error_limit_ = 255.0;
  int max_abs_error_limit_ = 255;
  int small_abs_error_limit_ = 0;
};

// All pixels can be off by one, but any more than that is an error.
class FuzzyPixelOffByOneComparator : public FuzzyPixelComparator {
 public:
  FuzzyPixelOffByOneComparator() {
    SetErrorPixelsPercentageLimit(100.f);
    SetAbsErrorLimit(1);
  }

 protected:
  using FuzzyPixelComparator::DiscardAlpha;
  using FuzzyPixelComparator::SetAbsErrorLimit;
  using FuzzyPixelComparator::SetAvgAbsErrorLimit;
  using FuzzyPixelComparator::SetErrorPixelsPercentageLimit;
};

class AlphaDiscardingFuzzyPixelOffByOneComparator
    : public FuzzyPixelOffByOneComparator {
 public:
  AlphaDiscardingFuzzyPixelOffByOneComparator() { DiscardAlpha(); }
};
}  // namespace cc

#endif  // CC_TEST_PIXEL_COMPARATOR_H_
