// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_ANIMATION_ANIMATION_CURVE_H_
#define CC_ANIMATION_ANIMATION_CURVE_H_

#include <memory>

#include "base/time/time.h"
#include "cc/animation/animation_export.h"
#include "cc/paint/filter_operations.h"
#include "ui/gfx/geometry/size_f.h"
#include "ui/gfx/transform.h"
#include "ui/gfx/transform_operations.h"

namespace gfx {
class TransformOperations;
}  // namespace gfx

namespace cc {

class KeyframeModel;
class AnimationCurve;

// An animation curve is a function that returns a value given a time.
class CC_ANIMATION_EXPORT AnimationCurve {
 public:
  // TODO(crbug.com/1176334): we shouldn't need the curve type, long term.
  //
  // In the meanime, external clients of the animation machinery may well have
  // other curve types and should be added to this enum to ensure uniqueness.
  enum CurveType {
    COLOR = 0,
    FLOAT,
    TRANSFORM,
    FILTER,
    SCROLL_OFFSET,
    SIZE,
    // This must be last
    LAST_CURVE_TYPE = SIZE,
  };

  virtual ~AnimationCurve() = default;

  virtual base::TimeDelta Duration() const = 0;
  virtual int Type() const = 0;
  virtual const char* TypeName() const = 0;
  virtual std::unique_ptr<AnimationCurve> Clone() const = 0;
  virtual void Tick(base::TimeDelta t,
                    int property_id,
                    KeyframeModel* keyframe_model) const = 0;

  // Returns true if this animation preserves axis alignment.
  virtual bool PreservesAxisAlignment() const;

  // Set |max_scale| to the maximum scale along any dimension during the
  // animation, of all steps (keyframes) with calculatable scale. Returns
  // false if none of the steps can calculate a scale.
  virtual bool MaximumScale(float* max_scale) const;
};

#define DECLARE_ANIMATION_CURVE_BODY(T, Name)                                  \
 public:                                                                       \
  static const Name##AnimationCurve* To##Name##AnimationCurve(                 \
      const AnimationCurve* c);                                                \
  static Name##AnimationCurve* To##Name##AnimationCurve(AnimationCurve* c);    \
  class Target {                                                               \
   public:                                                                     \
    virtual ~Target() = default;                                               \
    virtual void On##Name##Animated(const T& value,                            \
                                    int target_property_id,                    \
                                    KeyframeModel* keyframe_model) = 0;        \
  };                                                                           \
  ~Name##AnimationCurve() override = default;                                  \
  virtual T GetValue(base::TimeDelta t) const = 0;                             \
  void Tick(base::TimeDelta t, int property_id, KeyframeModel* keyframe_model) \
      const override;                                                          \
  void set_target(Target* target) { target_ = target; }                        \
  int Type() const override;                                                   \
  const char* TypeName() const override;                                       \
                                                                               \
 private:                                                                      \
  Target* target_ = nullptr;

class CC_ANIMATION_EXPORT ColorAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(SkColor, Color)
};

class CC_ANIMATION_EXPORT FloatAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(float, Float)
};

class CC_ANIMATION_EXPORT SizeAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(gfx::SizeF, Size)
};

class CC_ANIMATION_EXPORT FilterAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(FilterOperations, Filter)
};

class CC_ANIMATION_EXPORT TransformAnimationCurve : public AnimationCurve {
  DECLARE_ANIMATION_CURVE_BODY(gfx::TransformOperations, Transform)
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_CURVE_H_
