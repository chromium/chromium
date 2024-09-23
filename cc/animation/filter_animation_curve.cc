// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/filter_animation_curve.h"

#include "base/memory/ptr_util.h"
#include "ui/gfx/animation/keyframe/keyframed_animation_curve-inl.h"

namespace cc {

void FilterAnimationCurve::Tick(
    base::TimeDelta t,
    int property_id,
    gfx::KeyframeModel* keyframe_model,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  if (target_) {
    target_->OnFilterAnimated(GetTransformedValue(t, limit_direction),
                              property_id, keyframe_model);
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

std::unique_ptr<FilterKeyframe> FilterKeyframe::Create(
    base::TimeDelta time,
    const FilterOperations& value,
    std::unique_ptr<gfx::TimingFunction> timing_function) {
  return base::WrapUnique(
      new FilterKeyframe(time, value, std::move(timing_function)));
}

FilterKeyframe::FilterKeyframe(
    base::TimeDelta time,
    const FilterOperations& value,
    std::unique_ptr<gfx::TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FilterKeyframe::~FilterKeyframe() = default;

const FilterOperations& FilterKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FilterKeyframe> FilterKeyframe::Clone() const {
  std::unique_ptr<gfx::TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FilterKeyframe::Create(Time(), Value(), std::move(func));
}

base::TimeDelta KeyframedFilterAnimationCurve::TickInterval() const {
  return ComputeTickInterval(timing_function_, scaled_duration(), keyframes_);
}

void KeyframedFilterAnimationCurve::AddKeyframe(
    std::unique_ptr<FilterKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFilterAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<gfx::AnimationCurve> KeyframedFilterAnimationCurve::Clone()
    const {
  std::unique_ptr<KeyframedFilterAnimationCurve> to_return =
      KeyframedFilterAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

// Use GetTransformedValue instead. This method is for animation curves that
// do not use timing functions.
FilterOperations KeyframedFilterAnimationCurve::GetValue(
    base::TimeDelta t) const {
  NOTREACHED();
}

FilterOperations KeyframedFilterAnimationCurve::GetTransformedValue(
    base::TimeDelta t,
    gfx::TimingFunction::LimitDirection limit_direction) const {
  KeyframesAndProgress values = GetKeyframesAndProgress(
      keyframes_, timing_function_, scaled_duration(), t, limit_direction);
  return keyframes_[values.to]->Value().Blend(keyframes_[values.from]->Value(),
                                              values.progress);
}

std::unique_ptr<KeyframedFilterAnimationCurve>
KeyframedFilterAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFilterAnimationCurve);
}

KeyframedFilterAnimationCurve::KeyframedFilterAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedFilterAnimationCurve::~KeyframedFilterAnimationCurve() = default;

}  // namespace cc
