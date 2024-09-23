// Copyright 2021 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_FILTER_ANIMATION_CURVE_H_
#define CC_ANIMATION_FILTER_ANIMATION_CURVE_H_

#include <memory>
#include <utility>
#include <vector>

#include "ui/gfx/animation/keyframe/keyframed_animation_curve.h"

#include "cc/animation/animation_export.h"
#include "cc/paint/filter_operations.h"

namespace cc {

class CC_ANIMATION_EXPORT FilterAnimationCurve : public gfx::AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(FilterOperations, Filter)
};

class CC_ANIMATION_EXPORT FilterKeyframe : public gfx::Keyframe {
 public:
  static std::unique_ptr<FilterKeyframe> Create(
      base::TimeDelta time,
      const FilterOperations& value,
      std::unique_ptr<gfx::TimingFunction> timing_function);
  ~FilterKeyframe() override;

  const FilterOperations& Value() const;

  std::unique_ptr<FilterKeyframe> Clone() const;

 private:
  FilterKeyframe(base::TimeDelta time,
                 const FilterOperations& value,
                 std::unique_ptr<gfx::TimingFunction> timing_function);

  FilterOperations value_;
};

class CC_ANIMATION_EXPORT KeyframedFilterAnimationCurve
    : public FilterAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedFilterAnimationCurve> Create();

  KeyframedFilterAnimationCurve(const KeyframedFilterAnimationCurve&) = delete;
  ~KeyframedFilterAnimationCurve() override;

  KeyframedFilterAnimationCurve& operator=(
      const KeyframedFilterAnimationCurve&) = delete;

  void AddKeyframe(std::unique_ptr<FilterKeyframe> keyframe);
  void SetTimingFunction(std::unique_ptr<gfx::TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<gfx::AnimationCurve> Clone() const override;
  base::TimeDelta TickInterval() const override;

  // FilterAnimationCurve implementation
  FilterOperations GetValue(base::TimeDelta t) const override;
  FilterOperations GetTransformedValue(
      base::TimeDelta t,
      gfx::TimingFunction::LimitDirection limit_direction) const override;

 private:
  KeyframedFilterAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<FilterKeyframe>> keyframes_;
  std::unique_ptr<gfx::TimingFunction> timing_function_;
  double scaled_duration_;
};

}  // namespace cc

#endif  // CC_ANIMATION_FILTER_ANIMATION_CURVE_H_
