// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_UTILS_H_
#define ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_UTILS_H_

#include "ash/ash_export.h"
#include "base/time/time.h"

namespace aura {
class Window;
}  // namespace aura

namespace gfx {
class Point;
class Rect;
}  // namespace gfx

namespace ash {
namespace magnifier_utils {

// Factor of magnification scale. For example, when this value is 1.189, scale
// value will be changed x1.000, x1.189, x1.414, x1.681, x2.000, ...
// Note: this value is 2.0 ^ (1 / 4).
constexpr float kMagnificationScaleFactor = 1.18920712f;

// When magnifier wants to make visible a rect that's wider than the viewport,
// we want to align the left edge of the rect to the left edge of the viewport.
// In a right-to-left language, such as Hebrew, we'll want to align the right
// edge of the rect with the right edge of the viewport. This way, the user can
// see the maximum amount of useful information in the rect, assuming the
// information begins at that edge (e.g. an omnibox entry). We also want to
// include a bit of padding beyond that edge of the rect, to provide more
// context about what's around the rect to the user. |kLeftEdgeContextPadding|
// defines how much padding to include in the viewport.
// TODO(accessibility): Add support for right-to-left languages.
constexpr int kLeftEdgeContextPadding = 32;

// The duration of time to ignore caret update after the last move magnifier to
// rect call. Prevents jumping magnifier viewport to caret when user is
// navigating through other things, but which happen to cause text/caret
// updates (e.g. the omnibox results). Try keep under one frame buffer length
// (~16ms assuming 60hz screen updates), however most importantly keep it short,
// so e.g. when user focuses an element, and then starts typing, the viewport
// quickly moves to the caret position.
constexpr base::TimeDelta kPauseCaretUpdateDuration = base::Milliseconds(15);

// Calculates the new scale if it were to be adjusted exponentially by the
// given |linear_offset|. This allows linear changes in scroll offset
// to have exponential changes on the scale, so that as the user zooms in,
// they appear to zoom faster through higher resolutions. This also has the
// effect that whether the user moves their fingers quickly or slowly on
// the trackpad (changing the number of events fired), the resulting zoom
// will only depend on the distance their fingers traveled.
// |linear_offset| should generally be between 0 and 1, to result in a set
// scale between |min_scale| and |max_scale|.
// The resulting scale should be an exponential of the form
// y = M * x ^ 2 + c, where y is the resulting scale, M is the scale range which
// is the difference between |max_scale| and |min_scale|, and c is the
// |min_scale|. This creates a mapping from |linear_offset| in (0, 1) to a scale
// in [min_scale, max_scale].
float ASH_EXPORT GetScaleFromScroll(float linear_offset,
                                    float current_scale,
                                    float min_scale,
                                    float max_scale);

// Converts the |current_range| to an integral index such that
// `current_scale = kMagnificationScaleFactor ^ index`, increments it by
// |delta_index| and converts it back to a scale value in the range between
// |min_scale| and |max_scale|.
float ASH_EXPORT GetNextMagnifierScaleValue(int delta_index,
                                            float current_scale,
                                            float min_scale,
                                            float max_scale);

// Gets the bounds of the Docked Magnifier viewport widget when placed in the
// display whose root window is |root|. The bounds returned correspond to the
// top quarter portion of the screen.
gfx::Rect ASH_EXPORT GetViewportWidgetBoundsInRoot(aura::Window* root,
                                                   float screen_height_divisor);

// If either of the fullscreen or docked magnifier is enabled, its focus will be
// updated to center around the given `point_in_screen`. Note that both
// magnifiers are mutually exclusive.
void MaybeUpdateActiveMagnifierFocus(const gfx::Point& point_in_screen);

}  // namespace magnifier_utils
}  // namespace ash

#endif  // ASH_ACCESSIBILITY_MAGNIFIER_MAGNIFIER_UTILS_H_
