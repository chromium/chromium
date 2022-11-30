// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_AMBIENT_ANIMATION_BACKGROUND_COLOR_H_
#define ASH_AMBIENT_UI_AMBIENT_ANIMATION_BACKGROUND_COLOR_H_

#include "ash/ash_export.h"
#include "third_party/skia/include/core/SkColor.h"

namespace cc {
class SkottieWrapper;
}  // namespace cc

namespace ash {

// Returns the color of the background in the ambient-mode |skottie| animation.
// If the the background color could not be parsed from the animation, a default
// color is returned on production builds and a fatal error occurs on debug
// builds.
ASH_EXPORT SkColor
GetAnimationBackgroundColor(const cc::SkottieWrapper& skottie);

}  // namespace ash

#endif  // ASH_AMBIENT_UI_AMBIENT_ANIMATION_BACKGROUND_COLOR_H_
