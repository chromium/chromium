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

namespace cc {

class ColorAnimationCurve;
class FilterAnimationCurve;
class FloatAnimationCurve;
class ScrollOffsetAnimationCurve;
class SizeAnimationCurve;
class TransformAnimationCurve;
class TransformOperations;

// An animation curve is a function that returns a value given a time.
class CC_ANIMATION_EXPORT AnimationCurve {
 public:
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

  virtual ~AnimationCurve() {}

  virtual base::TimeDelta Duration() const = 0;
  virtual CurveType Type() const = 0;
  virtual std::unique_ptr<AnimationCurve> Clone() const = 0;

  const ColorAnimationCurve* ToColorAnimationCurve() const;
  const FloatAnimationCurve* ToFloatAnimationCurve() const;
  const TransformAnimationCurve* ToTransformAnimationCurve() const;
  const FilterAnimationCurve* ToFilterAnimationCurve() const;
  const ScrollOffsetAnimationCurve* ToScrollOffsetAnimationCurve() const;
  const SizeAnimationCurve* ToSizeAnimationCurve() const;

  ScrollOffsetAnimationCurve* ToScrollOffsetAnimationCurve();
};

class CC_ANIMATION_EXPORT ColorAnimationCurve : public AnimationCurve {
 public:
  ~ColorAnimationCurve() override {}

  virtual SkColor GetValue(base::TimeDelta t) const = 0;

  CurveType Type() const override;
};

class CC_ANIMATION_EXPORT FloatAnimationCurve : public AnimationCurve {
 public:
  ~FloatAnimationCurve() override {}

  virtual float GetValue(base::TimeDelta t) const = 0;

  CurveType Type() const override;
};

class CC_ANIMATION_EXPORT TransformAnimationCurve : public AnimationCurve {
 public:
  ~TransformAnimationCurve() override {}

  virtual TransformOperations GetValue(base::TimeDelta t) const = 0;

  // Returns true if this animation preserves axis alignment.
  virtual bool PreservesAxisAlignment() const = 0;

  // Set |max_scale| to the maximum scale along any dimension during the
  // animation. Returns false if the maximum scale cannot be computed.
  virtual bool MaximumScale(float* max_scale) const = 0;

  CurveType Type() const override;
};

class CC_ANIMATION_EXPORT FilterAnimationCurve : public AnimationCurve {
 public:
  ~FilterAnimationCurve() override {}

  virtual FilterOperations GetValue(base::TimeDelta t) const = 0;
  virtual bool HasFilterThatMovesPixels() const = 0;

  CurveType Type() const override;
};

class CC_ANIMATION_EXPORT SizeAnimationCurve : public AnimationCurve {
 public:
  ~SizeAnimationCurve() override {}

  virtual gfx::SizeF GetValue(base::TimeDelta t) const = 0;

  CurveType Type() const override;
};

}  // namespace cc

#endif  // CC_ANIMATION_ANIMATION_CURVE_H_
