// Copyright 2022 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
#define ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_

#include "ui/views/test/view_skia_gold_pixel_diff.h"

namespace ash {

// A helper class that provides utility functions for performing pixel diff
// tests via the Skia Gold.
class AshPixelDiffTestHelper {
 public:
  AshPixelDiffTestHelper();
  AshPixelDiffTestHelper(const AshPixelDiffTestHelper&) = delete;
  AshPixelDiffTestHelper& operator=(const AshPixelDiffTestHelper&) = delete;
  ~AshPixelDiffTestHelper();

  // Takes a screenshot of the primary fullscreen then uploads it to the Skia
  // Gold to perform pixel comparison. Returns the comparison result.
  bool ComparePrimaryFullScreen(const std::string& screenshot_name);

  // Initializes the underlying utility class for Skia Gold pixel tests.
  // NOTE: this function has to be called before any pixel comparison.
  void InitSkiaGoldPixelDiff(const std::string& screenshot_prefix,
                             const std::string& corpus = std::string());

 private:
  // Used to take screenshots and upload images to the Skia Gold server to
  // perform pixel comparison.
  // NOTE: the user of `ViewSkiaGoldPixelDiff` has the duty to initialize
  // `pixel_diff` before performing any pixel comparison.
  views::ViewSkiaGoldPixelDiff pixel_diff_;
};

}  // namespace ash

#endif  // ASH_TEST_ASH_PIXEL_DIFF_TEST_HELPER_H_
