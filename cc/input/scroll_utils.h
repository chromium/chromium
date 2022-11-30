// Copyright 2020 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_SCROLL_UTILS_H_
#define CC_INPUT_SCROLL_UTILS_H_

#include "cc/cc_export.h"

namespace gfx {
class Vector2dF;
class SizeF;
}  // namespace gfx

namespace cc {

static constexpr int kPixelsPerLineStep = 40;
static constexpr float kMinFractionToStepWhenPaging = 0.875f;

// Each directional scroll for percentage-based units should scroll 1/8th of
// the scrollable area.
static constexpr float kPercentDeltaForDirectionalScroll = 0.125f;

// Class for scroll helper methods in cc and blink.
class CC_EXPORT ScrollUtils {
 public:
  // Transforms a |scroll_delta| in percent units to pixel units based in its
  // |scroller_size|. Limits it by a maximum of 12.5% of |viewport_size| to
  // avoid too large deltas. Inputs and output must be in physical pixels.
  static gfx::Vector2dF ResolveScrollPercentageToPixels(
      const gfx::Vector2dF& scroll_delta,
      const gfx::SizeF& scroller_size,
      const gfx::SizeF& viewport_size);

  // Transforms a pixel delta into a percentage. Used for when a test needs to
  // work with percent based scrolling and non percent based scrolling.
  static gfx::Vector2dF ResolvePixelScrollToPercentageForTesting(
      const gfx::Vector2dF& pixel_scroll_delta,
      const gfx::SizeF& scroller_size,
      const gfx::SizeF& viewport_size);
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_UTILS_H_
