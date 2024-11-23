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
};

}  // namespace cc

#endif  // CC_INPUT_SCROLL_UTILS_H_
