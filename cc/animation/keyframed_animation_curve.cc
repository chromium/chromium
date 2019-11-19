// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/keyframed_animation_curve.h"

#include <stddef.h>

#include <algorithm>

#include "base/memory/ptr_util.h"
#include "cc/base/time_util.h"
#include "ui/gfx/animation/tween.h"
#include "ui/gfx/geometry/box_f.h"

namespace cc {

namespace {

template <class KeyframeType>
void InsertKeyframe(std::unique_ptr<KeyframeType> keyframe,
                    std::vector<std::unique_ptr<KeyframeType>>* keyframes) {
  // Usually, the keyframes will be added in order, so this loop would be
  // unnecessary and we should skip it if possible.
  if (!keyframes->empty() && keyframe->Time() < keyframes->back()->Time()) {
    for (size_t i = 0; i < keyframes->size(); ++i) {
      if (keyframe->Time() < keyframes->at(i)->Time()) {
        keyframes->insert(keyframes->begin() + i, std::move(keyframe));
        return;
      }
    }
  }

  keyframes->push_back(std::move(keyframe));
}

template <typename KeyframeType>
base::TimeDelta TransformedAnimationTime(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    const std::unique_ptr<TimingFunction>& timing_function,
    double scaled_duration,
    base::TimeDelta time) {
  if (timing_function) {
    base::TimeDelta start_time = keyframes.front()->Time() * scaled_duration;
    base::TimeDelta duration =
        (keyframes.back()->Time() - keyframes.front()->Time()) *
        scaled_duration;
    double progress = TimeUtil::Divide(time - start_time, duration);

    time = (duration * timing_function->GetValue(progress)) + start_time;
  }

  return time;
}

template <typename KeyframeType>
size_t GetActiveKeyframe(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    double scaled_duration,
    base::TimeDelta time) {
  DCHECK_GE(keyframes.size(), 2ul);
  size_t i = 0;
  for (; i < keyframes.size() - 2; ++i) {  // Last keyframe is never active.
    if (time < (keyframes[i + 1]->Time() * scaled_duration))
      break;
  }

  return i;
}

template <typename KeyframeType>
double TransformedKeyframeProgress(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    double scaled_duration,
    base::TimeDelta time,
    size_t i) {
  base::TimeDelta time1 = keyframes[i]->Time() * scaled_duration;
  base::TimeDelta time2 = keyframes[i + 1]->Time() * scaled_duration;

  double progress = TimeUtil::Divide(time - time1, time2 - time1);

  if (keyframes[i]->timing_function()) {
    progress = keyframes[i]->timing_function()->GetValue(progress);
  }

  return progress;
}

}  // namespace

Keyframe::Keyframe(base::TimeDelta time,
                   std::unique_ptr<TimingFunction> timing_function)
    : time_(time), timing_function_(std::move(timing_function)) {}

Keyframe::~Keyframe() = default;

base::TimeDelta Keyframe::Time() const {
  return time_;
}

std::unique_ptr<ColorKeyframe> ColorKeyframe::Create(
    base::TimeDelta time,
    SkColor value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new ColorKeyframe(time, value, std::move(timing_function)));
}

ColorKeyframe::ColorKeyframe(base::TimeDelta time,
                             SkColor value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

ColorKeyframe::~ColorKeyframe() = default;

SkColor ColorKeyframe::Value() const { return value_; }

std::unique_ptr<ColorKeyframe> ColorKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return ColorKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Create(
    base::TimeDelta time,
    float value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new FloatKeyframe(time, value, std::move(timing_function)));
}

FloatKeyframe::FloatKeyframe(base::TimeDelta time,
                             float value,
                             std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FloatKeyframe::~FloatKeyframe() = default;

float FloatKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FloatKeyframe> FloatKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FloatKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Create(
    base::TimeDelta time,
    const TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new TransformKeyframe(time, value, std::move(timing_function)));
}

TransformKeyframe::TransformKeyframe(
    base::TimeDelta time,
    const TransformOperations& value,
    std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

TransformKeyframe::~TransformKeyframe() = default;

const TransformOperations& TransformKeyframe::Value() const {
  return value_;
}

std::unique_ptr<TransformKeyframe> TransformKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return TransformKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<FilterKeyframe> FilterKeyframe::Create(
    base::TimeDelta time,
    const FilterOperations& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new FilterKeyframe(time, value, std::move(timing_function)));
}

FilterKeyframe::FilterKeyframe(base::TimeDelta time,
                               const FilterOperations& value,
                               std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

FilterKeyframe::~FilterKeyframe() = default;

const FilterOperations& FilterKeyframe::Value() const {
  return value_;
}

std::unique_ptr<FilterKeyframe> FilterKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return FilterKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<SizeKeyframe> SizeKeyframe::Create(
    base::TimeDelta time,
    const gfx::SizeF& value,
    std::unique_ptr<TimingFunction> timing_function) {
  return base::WrapUnique(
      new SizeKeyframe(time, value, std::move(timing_function)));
}

SizeKeyframe::SizeKeyframe(base::TimeDelta time,
                           const gfx::SizeF& value,
                           std::unique_ptr<TimingFunction> timing_function)
    : Keyframe(time, std::move(timing_function)), value_(value) {}

SizeKeyframe::~SizeKeyframe() = default;

const gfx::SizeF& SizeKeyframe::Value() const {
  return value_;
}

std::unique_ptr<SizeKeyframe> SizeKeyframe::Clone() const {
  std::unique_ptr<TimingFunction> func;
  if (timing_function())
    func = timing_function()->Clone();
  return SizeKeyframe::Create(Time(), Value(), std::move(func));
}

std::unique_ptr<KeyframedColorAnimationCurve>
KeyframedColorAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedColorAnimationCurve);
}

KeyframedColorAnimationCurve::KeyframedColorAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedColorAnimationCurve::~KeyframedColorAnimationCurve() = default;

void KeyframedColorAnimationCurve::AddKeyframe(
    std::unique_ptr<ColorKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedColorAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<AnimationCurve> KeyframedColorAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedColorAnimationCurve> to_return =
      KeyframedColorAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

SkColor KeyframedColorAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return gfx::Tween::ColorValueBetween(
      progress, keyframes_[i]->Value(), keyframes_[i + 1]->Value());
}

std::unique_ptr<KeyframedFloatAnimationCurve>
KeyframedFloatAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFloatAnimationCurve);
}

KeyframedFloatAnimationCurve::KeyframedFloatAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedFloatAnimationCurve::~KeyframedFloatAnimationCurve() = default;

void KeyframedFloatAnimationCurve::AddKeyframe(
    std::unique_ptr<FloatKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFloatAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<AnimationCurve> KeyframedFloatAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedFloatAnimationCurve> to_return =
      KeyframedFloatAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

float KeyframedFloatAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return keyframes_[i]->Value() +
      (keyframes_[i+1]->Value() - keyframes_[i]->Value()) * progress;
}

std::unique_ptr<KeyframedTransformAnimationCurve>
KeyframedTransformAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedTransformAnimationCurve);
}

KeyframedTransformAnimationCurve::KeyframedTransformAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedTransformAnimationCurve::~KeyframedTransformAnimationCurve() = default;

void KeyframedTransformAnimationCurve::AddKeyframe(
    std::unique_ptr<TransformKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedTransformAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<AnimationCurve> KeyframedTransformAnimationCurve::Clone()
    const {
  std::unique_ptr<KeyframedTransformAnimationCurve> to_return =
      KeyframedTransformAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

TransformOperations KeyframedTransformAnimationCurve::GetValue(
    base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return keyframes_[i + 1]->Value().Blend(keyframes_[i]->Value(), progress);
}

bool KeyframedTransformAnimationCurve::PreservesAxisAlignment() const {
  for (const auto& keyframe : keyframes_) {
    if (!keyframe->Value().PreservesAxisAlignment())
      return false;
  }
  return true;
}

bool KeyframedTransformAnimationCurve::IsTranslation() const {
  for (const auto& keyframe : keyframes_) {
    if (!keyframe->Value().IsTranslation() && !keyframe->Value().IsIdentity())
      return false;
  }
  return true;
}

bool KeyframedTransformAnimationCurve::AnimationStartScale(
    bool forward_direction,
    float* start_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *start_scale = 0.f;
  size_t start_location = 0;
  if (!forward_direction) {
    start_location = keyframes_.size() - 1;
  }

  return keyframes_[start_location]->Value().ScaleComponent(start_scale);
}

bool KeyframedTransformAnimationCurve::MaximumTargetScale(
    bool forward_direction,
    float* max_scale) const {
  DCHECK_GE(keyframes_.size(), 2ul);
  *max_scale = 0.f;

  // If |forward_direction| is true, then skip the first frame, otherwise
  // skip the last frame, since that is the original position in the animation.
  size_t start = 1;
  size_t end = keyframes_.size();
  if (!forward_direction) {
    --start;
    --end;
  }

  for (size_t i = start; i < end; ++i) {
    float target_scale_for_segment = 0.f;
    if (!keyframes_[i]->Value().ScaleComponent(&target_scale_for_segment))
      return false;
    *max_scale = fmax(*max_scale, target_scale_for_segment);
  }
  return true;
}

std::unique_ptr<KeyframedFilterAnimationCurve>
KeyframedFilterAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFilterAnimationCurve);
}

KeyframedFilterAnimationCurve::KeyframedFilterAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedFilterAnimationCurve::~KeyframedFilterAnimationCurve() = default;

void KeyframedFilterAnimationCurve::AddKeyframe(
    std::unique_ptr<FilterKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedFilterAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<AnimationCurve> KeyframedFilterAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedFilterAnimationCurve> to_return =
      KeyframedFilterAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

FilterOperations KeyframedFilterAnimationCurve::GetValue(
    base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return keyframes_[i + 1]->Value().Blend(keyframes_[i]->Value(), progress);
}

bool KeyframedFilterAnimationCurve::HasFilterThatMovesPixels() const {
  for (const auto& keyframe : keyframes_) {
    if (keyframe->Value().HasFilterThatMovesPixels()) {
      return true;
    }
  }
  return false;
}

std::unique_ptr<KeyframedSizeAnimationCurve>
KeyframedSizeAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedSizeAnimationCurve);
}

KeyframedSizeAnimationCurve::KeyframedSizeAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedSizeAnimationCurve::~KeyframedSizeAnimationCurve() = default;

void KeyframedSizeAnimationCurve::AddKeyframe(
    std::unique_ptr<SizeKeyframe> keyframe) {
  InsertKeyframe(std::move(keyframe), &keyframes_);
}

base::TimeDelta KeyframedSizeAnimationCurve::Duration() const {
  return (keyframes_.back()->Time() - keyframes_.front()->Time()) *
         scaled_duration();
}

std::unique_ptr<AnimationCurve> KeyframedSizeAnimationCurve::Clone() const {
  std::unique_ptr<KeyframedSizeAnimationCurve> to_return =
      KeyframedSizeAnimationCurve::Create();
  for (const auto& keyframe : keyframes_)
    to_return->AddKeyframe(keyframe->Clone());

  if (timing_function_)
    to_return->SetTimingFunction(timing_function_->Clone());

  to_return->set_scaled_duration(scaled_duration());

  return std::move(to_return);
}

gfx::SizeF KeyframedSizeAnimationCurve::GetValue(base::TimeDelta t) const {
  if (t <= (keyframes_.front()->Time() * scaled_duration()))
    return keyframes_.front()->Value();

  if (t >= (keyframes_.back()->Time() * scaled_duration()))
    return keyframes_.back()->Value();

  t = TransformedAnimationTime(keyframes_, timing_function_, scaled_duration(),
                               t);
  size_t i = GetActiveKeyframe(keyframes_, scaled_duration(), t);
  double progress =
      TransformedKeyframeProgress(keyframes_, scaled_duration(), t, i);

  return gfx::Tween::SizeFValueBetween(progress, keyframes_[i]->Value(),
                                       keyframes_[i + 1]->Value());
}

}  // namespace cc
