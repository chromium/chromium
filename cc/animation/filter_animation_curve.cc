// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/filter_animation_curve.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve-inl.h"

namespace cc {

namespace {

FilterOperations FilterOperationsValueBetween(double progress,
                                              const FilterOperations& from,
                                              const FilterOperations& to) {
  return to.Blend(from, progress);
}

}  // namespace

void FilterAnimationCurve::Tick(base::TimeDelta t,
                                int property_id,
                                gfx::KeyframeModel* keyframe_model) const {
  if (target_) {
    target_->OnFilterAnimated(GetValue(t), property_id, keyframe_model);
  }
}

int FilterAnimationCurve::Type() const {
  return gfx::AnimationCurve::FILTER;
}

const char* FilterAnimationCurve::TypeName() const {
  return "Filter";
}

const FilterAnimationCurve* FilterAnimationCurve::ToFilterAnimationCurve(
    const gfx::AnimationCurve* c) {
  DCHECK_EQ(gfx::AnimationCurve::FILTER, c->Type());
  return static_cast<const FilterAnimationCurve*>(c);
}

FilterAnimationCurve* FilterAnimationCurve::ToFilterAnimationCurve(
    gfx::AnimationCurve* c) {
  DCHECK_EQ(AnimationCurve::FILTER, c->Type());
  return static_cast<FilterAnimationCurve*>(c);
}

KEYFRAMED_ANIMATION_CURVE_DEFINITION(FilterOperations,
                                     Filter,
                                     FilterOperationsValueBetween)

}  // namespace cc
