// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_FILTER_ANIMATION_CURVE_H_
#define CC_ANIMATION_FILTER_ANIMATION_CURVE_H_

#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

#include "cc/animation/animation_export.h"
#include "cc/paint/filter_operations.h"

namespace cc {

class CC_ANIMATION_EXPORT FilterAnimationCurve : public gfx::AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(FilterOperations, Filter)
};

class CC_ANIMATION_EXPORT FilterKeyframe : public gfx::Keyframe {
  DECLARE_KEYFRAME_BODY(FilterOperations, Filter)
};

class CC_ANIMATION_EXPORT KeyframedFilterAnimationCurve
    : public FilterAnimationCurve {
  DECLARE_KEYFRAMED_CURVE_BODY(FilterOperations, Filter)
};

}  // namespace cc

#endif  // CC_ANIMATION_FILTER_ANIMATION_CURVE_H_
