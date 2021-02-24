// Copyright 2021 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/filter_animation_curve.h"

#include "base/memory/ptr_util.h"

namespace cc {

// TODO(crbug.com/747185): All code in this namespace duplicates code from
// ui/gfx/keyframe/animation/ unnecessarily.
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

struct TimeValues {
  base::TimeDelta start_time;
  base::TimeDelta duration;
  double progress;
};

template <typename KeyframeType>
TimeValues GetTimeValues(const KeyframeType& start_frame,
                         const KeyframeType& end_frame,
                         double scaled_duration,
                         base::TimeDelta time) {
  TimeValues values;
  values.start_time = start_frame.Time() * scaled_duration;
  values.duration = (end_frame.Time() * scaled_duration) - values.start_time;
  const base::TimeDelta elapsed = time - values.start_time;
  values.progress = (elapsed.is_inf() || values.duration.is_zero())
                        ? 1.0
                        : (elapsed / values.duration);
  return values;
}

template <typename KeyframeType>
base::TimeDelta TransformedAnimationTime(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    const std::unique_ptr<gfx::TimingFunction>& timing_function,
    double scaled_duration,
    base::TimeDelta time) {
  if (timing_function) {
    const auto values = GetTimeValues(*keyframes.front(), *keyframes.back(),
                                      scaled_duration, time);
    time = (values.duration * timing_function->GetValue(values.progress)) +
           values.start_time;
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
  while ((i < keyframes.size() - 2) &&  // Last keyframe is never active.
         (time >= (keyframes[i + 1]->Time() * scaled_duration)))
    ++i;

  return i;
}

template <typename KeyframeType>
double TransformedKeyframeProgress(
    const std::vector<std::unique_ptr<KeyframeType>>& keyframes,
    double scaled_duration,
    base::TimeDelta time,
    size_t i) {
  const double progress =
      GetTimeValues(*keyframes[i], *keyframes[i + 1], scaled_duration, time)
          .progress;
  return keyframes[i]->timing_function()
             ? keyframes[i]->timing_function()->GetValue(progress)
             : progress;
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

std::unique_ptr<KeyframedFilterAnimationCurve>
KeyframedFilterAnimationCurve::Create() {
  return base::WrapUnique(new KeyframedFilterAnimationCurve);
}

KeyframedFilterAnimationCurve::KeyframedFilterAnimationCurve()
    : scaled_duration_(1.0) {}

KeyframedFilterAnimationCurve::~KeyframedFilterAnimationCurve() = default;

}  // namespace cc
