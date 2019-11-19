// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_
#define CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_

#include <vector>

#include "base/time/time.h"
#include "cc/animation/animation_curve.h"
#include "cc/animation/animation_export.h"
#include "cc/animation/timing_function.h"
#include "cc/animation/transform_operations.h"
#include "ui/gfx/geometry/size_f.h"

namespace cc {

class CC_ANIMATION_EXPORT Keyframe {
 public:
  Keyframe(const Keyframe&) = delete;
  Keyframe& operator=(const Keyframe&) = delete;

  base::TimeDelta Time() const;
  const TimingFunction* timing_function() const {
    return timing_function_.get();
  }

 protected:
  Keyframe(base::TimeDelta time,
           std::unique_ptr<TimingFunction> timing_function);
  virtual ~Keyframe();

 private:
  base::TimeDelta time_;
  std::unique_ptr<TimingFunction> timing_function_;
};

class CC_ANIMATION_EXPORT ColorKeyframe : public Keyframe {
 public:
  static std::unique_ptr<ColorKeyframe> Create(
      base::TimeDelta time,
      SkColor value,
      std::unique_ptr<TimingFunction> timing_function);
  ~ColorKeyframe() override;

  SkColor Value() const;

  std::unique_ptr<ColorKeyframe> Clone() const;

 private:
  ColorKeyframe(base::TimeDelta time,
                SkColor value,
                std::unique_ptr<TimingFunction> timing_function);

  SkColor value_;
};

class CC_ANIMATION_EXPORT FloatKeyframe : public Keyframe {
 public:
  static std::unique_ptr<FloatKeyframe> Create(
      base::TimeDelta time,
      float value,
      std::unique_ptr<TimingFunction> timing_function);
  ~FloatKeyframe() override;

  float Value() const;

  std::unique_ptr<FloatKeyframe> Clone() const;

 private:
  FloatKeyframe(base::TimeDelta time,
                float value,
                std::unique_ptr<TimingFunction> timing_function);

  float value_;
};

class CC_ANIMATION_EXPORT TransformKeyframe : public Keyframe {
 public:
  static std::unique_ptr<TransformKeyframe> Create(
      base::TimeDelta time,
      const TransformOperations& value,
      std::unique_ptr<TimingFunction> timing_function);
  ~TransformKeyframe() override;

  const TransformOperations& Value() const;

  std::unique_ptr<TransformKeyframe> Clone() const;

 private:
  TransformKeyframe(base::TimeDelta time,
                    const TransformOperations& value,
                    std::unique_ptr<TimingFunction> timing_function);

  TransformOperations value_;
};

class CC_ANIMATION_EXPORT FilterKeyframe : public Keyframe {
 public:
  static std::unique_ptr<FilterKeyframe> Create(
      base::TimeDelta time,
      const FilterOperations& value,
      std::unique_ptr<TimingFunction> timing_function);
  ~FilterKeyframe() override;

  const FilterOperations& Value() const;

  std::unique_ptr<FilterKeyframe> Clone() const;

 private:
  FilterKeyframe(base::TimeDelta time,
                 const FilterOperations& value,
                 std::unique_ptr<TimingFunction> timing_function);

  FilterOperations value_;
};

class CC_ANIMATION_EXPORT SizeKeyframe : public Keyframe {
 public:
  static std::unique_ptr<SizeKeyframe> Create(
      base::TimeDelta time,
      const gfx::SizeF& bounds,
      std::unique_ptr<TimingFunction> timing_function);
  ~SizeKeyframe() override;

  const gfx::SizeF& Value() const;

  std::unique_ptr<SizeKeyframe> Clone() const;

 private:
  SizeKeyframe(base::TimeDelta time,
               const gfx::SizeF& value,
               std::unique_ptr<TimingFunction> timing_function);

  gfx::SizeF value_;
};

class CC_ANIMATION_EXPORT KeyframedColorAnimationCurve
    : public ColorAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedColorAnimationCurve> Create();

  KeyframedColorAnimationCurve(const KeyframedColorAnimationCurve&) = delete;
  ~KeyframedColorAnimationCurve() override;

  KeyframedColorAnimationCurve& operator=(const KeyframedColorAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<ColorKeyframe> keyframe);
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

  // BackgrounColorAnimationCurve implementation
  SkColor GetValue(base::TimeDelta t) const override;

  using Keyframes = std::vector<std::unique_ptr<ColorKeyframe>>;
  const Keyframes& keyframes_for_testing() const { return keyframes_; }

 private:
  KeyframedColorAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class CC_ANIMATION_EXPORT KeyframedFloatAnimationCurve
    : public FloatAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedFloatAnimationCurve> Create();

  KeyframedFloatAnimationCurve(const KeyframedFloatAnimationCurve&) = delete;
  ~KeyframedFloatAnimationCurve() override;

  KeyframedFloatAnimationCurve& operator=(const KeyframedFloatAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<FloatKeyframe> keyframe);

  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  TimingFunction* timing_function_for_testing() const {
    return timing_function_.get();
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

  // FloatAnimationCurve implementation
  float GetValue(base::TimeDelta t) const override;

  using Keyframes = std::vector<std::unique_ptr<FloatKeyframe>>;
  const Keyframes& keyframes_for_testing() const { return keyframes_; }

 private:
  KeyframedFloatAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  Keyframes keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class CC_ANIMATION_EXPORT KeyframedTransformAnimationCurve
    : public TransformAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedTransformAnimationCurve> Create();

  KeyframedTransformAnimationCurve(const KeyframedTransformAnimationCurve&) =
      delete;
  ~KeyframedTransformAnimationCurve() override;

  KeyframedTransformAnimationCurve& operator=(
      const KeyframedTransformAnimationCurve&) = delete;

  void AddKeyframe(std::unique_ptr<TransformKeyframe> keyframe);
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

  // TransformAnimationCurve implementation
  TransformOperations GetValue(base::TimeDelta t) const override;
  bool PreservesAxisAlignment() const override;
  bool IsTranslation() const override;
  bool AnimationStartScale(bool forward_direction,
                           float* start_scale) const override;
  bool MaximumTargetScale(bool forward_direction,
                          float* max_scale) const override;

 private:
  KeyframedTransformAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<TransformKeyframe>> keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
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
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

  // FilterAnimationCurve implementation
  FilterOperations GetValue(base::TimeDelta t) const override;
  bool HasFilterThatMovesPixels() const override;

 private:
  KeyframedFilterAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<FilterKeyframe>> keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

class CC_ANIMATION_EXPORT KeyframedSizeAnimationCurve
    : public SizeAnimationCurve {
 public:
  // It is required that the keyframes be sorted by time.
  static std::unique_ptr<KeyframedSizeAnimationCurve> Create();

  KeyframedSizeAnimationCurve(const KeyframedSizeAnimationCurve&) = delete;
  ~KeyframedSizeAnimationCurve() override;

  KeyframedSizeAnimationCurve& operator=(const KeyframedSizeAnimationCurve&) =
      delete;

  void AddKeyframe(std::unique_ptr<SizeKeyframe> keyframe);
  void SetTimingFunction(std::unique_ptr<TimingFunction> timing_function) {
    timing_function_ = std::move(timing_function);
  }
  double scaled_duration() const { return scaled_duration_; }
  void set_scaled_duration(double scaled_duration) {
    scaled_duration_ = scaled_duration;
  }

  // AnimationCurve implementation
  base::TimeDelta Duration() const override;
  std::unique_ptr<AnimationCurve> Clone() const override;

  // SizeAnimationCurve implementation
  gfx::SizeF GetValue(base::TimeDelta t) const override;

 private:
  KeyframedSizeAnimationCurve();

  // Always sorted in order of increasing time. No two keyframes have the
  // same time.
  std::vector<std::unique_ptr<SizeKeyframe>> keyframes_;
  std::unique_ptr<TimingFunction> timing_function_;
  double scaled_duration_;
};

}  // namespace cc

#endif  // CC_ANIMATION_KEYFRAMED_ANIMATION_CURVE_H_
