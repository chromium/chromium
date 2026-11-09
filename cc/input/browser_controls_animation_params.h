// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_ANIMATION_PARAMS_H_
#define CC_INPUT_BROWSER_CONTROLS_ANIMATION_PARAMS_H_

#include "base/time/time.h"
#include "ui/gfx/geometry/cubic_bezier.h"

namespace cc {

// Describes an animation of the browser controls visible heights that the
// browser process delegates to the renderer compositor.
struct BrowserControlsAnimationParams {
  // Visible heights, in physical pixels, at the end of the animation.
  float target_top_controls_visible_height = 0.0f;
  float target_bottom_controls_visible_height = 0.0f;
  // A zero duration applies the targets on the next frame. Must not be
  // negative.
  base::TimeDelta duration;
  // Easing applied to the animation progress. Defaults to a linear curve.
  gfx::CubicBezier curve{0.0, 0.0, 1.0, 1.0};
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_ANIMATION_PARAMS_H_
