// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_UTILS_H_
#define CC_INPUT_SCROLL_UTILS_H_

#include <limits>

#include "build/build_config.h"
#include "cc/cc_export.h"

namespace gfx {
class Vector2dF;
class SizeF;
}  // namespace gfx

namespace cc {

static constexpr int kPixelsPerLineStep = 40;
static constexpr float kMinFractionToStepWhenSnapPaging = 0.4f;
static constexpr float kMinFractionToStepWhenPaging = 0.875f;
#if BUILDFLAG(IS_MAC)
static constexpr int kMaxOverlapBetweenPages = 40;
#else
static constexpr int kMaxOverlapBetweenPages = std::numeric_limits<int>::max();
#endif  // BUILDFLAG(IS_MAC)

// Class for scroll helper methods in cc and blink.
class CC_EXPORT ScrollUtils {
 public:
  // Transforms a |scroll_delta| in percent units to pixel units based on its
  // scroller_size and viewport_size. Inputs and output must be in physical
  // pixels. Currently used for converting kScrollByPage wheel scrolls
  // (available on some platforms and can be enabled via the OS setting) that
  // are handled on the compositor thread to pixel units.
  static gfx::Vector2dF ResolveScrollPercentageToPixels(
      const gfx::Vector2dF& scroll_delta,
      const gfx::SizeF& scroller_size,
      const gfx::SizeF& viewport_size);

  static int CalculateMinPageSnap(int length);
  static int CalculateMaxPageSnap(int length);
  static int CalculatePageStep(int length);

  // Calculates the length of the scrollbar thumb.
  // |total_size| is the total size of the scrolled contents.
  // |visible_size| is the visible size of the contents.
  // |track_length| is the visible length of the scrollbar.
  // |minimum_thumb_length| specifies the lower limit of the return value.
  static int CalculateScrollbarThumbLength(int total_size,
                                           int visible_size,
                                           int track_length,
                                           int minimum_thumb_length);
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_UTILS_H_
