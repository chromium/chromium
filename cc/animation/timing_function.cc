// Copyright 2012 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "cc/animation/timing_function.h"

#include <cmath>
#include <memory>

#include "base/logging.h"
#include "base/memory/ptr_util.h"

namespace cc {

TimingFunction::TimingFunction() = default;

TimingFunction::~TimingFunction() = default;

std::unique_ptr<CubicBezierTimingFunction>
CubicBezierTimingFunction::CreatePreset(EaseType ease_type) {
  // These numbers come from
  // http://www.w3.org/TR/css3-transitions/#transition-timing-function_tag.
  switch (ease_type) {
    case EaseType::EASE:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.25, 0.1, 0.25, 1.0));
    case EaseType::EASE_IN:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 1.0, 1.0));
    case EaseType::EASE_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.0, 0.0, 0.58, 1.0));
    case EaseType::EASE_IN_OUT:
      return base::WrapUnique(
          new CubicBezierTimingFunction(ease_type, 0.42, 0.0, 0.58, 1));
    default:
      NOTREACHED();
      return nullptr;
  }
}
std::unique_ptr<CubicBezierTimingFunction>
CubicBezierTimingFunction::Create(double x1, double y1, double x2, double y2) {
  return base::WrapUnique(
      new CubicBezierTimingFunction(EaseType::CUSTOM, x1, y1, x2, y2));
}

CubicBezierTimingFunction::CubicBezierTimingFunction(EaseType ease_type,
                                                     double x1,
                                                     double y1,
                                                     double x2,
                                                     double y2)
    : bezier_(x1, y1, x2, y2), ease_type_(ease_type) {}

CubicBezierTimingFunction::~CubicBezierTimingFunction() = default;

TimingFunction::Type CubicBezierTimingFunction::GetType() const {
  return Type::CUBIC_BEZIER;
}

double CubicBezierTimingFunction::GetValue(double x) const {
  return bezier_.Solve(x);
}

double CubicBezierTimingFunction::Velocity(double x) const {
  return bezier_.Slope(x);
}

std::unique_ptr<TimingFunction> CubicBezierTimingFunction::Clone() const {
  return base::WrapUnique(new CubicBezierTimingFunction(*this));
}

std::unique_ptr<StepsTimingFunction> StepsTimingFunction::Create(
    int steps,
    StepPosition step_position) {
  return base::WrapUnique(new StepsTimingFunction(steps, step_position));
}

StepsTimingFunction::StepsTimingFunction(int steps, StepPosition step_position)
    : steps_(steps), step_position_(step_position) {}

StepsTimingFunction::~StepsTimingFunction() = default;

TimingFunction::Type StepsTimingFunction::GetType() const {
  return Type::STEPS;
}

double StepsTimingFunction::GetValue(double t) const {
  return GetPreciseValue(t, TimingFunction::LimitDirection::RIGHT);
}

std::unique_ptr<TimingFunction> StepsTimingFunction::Clone() const {
  return base::WrapUnique(new StepsTimingFunction(*this));
}

double StepsTimingFunction::Velocity(double x) const {
  return 0;
}

double StepsTimingFunction::GetPreciseValue(double t,
                                            LimitDirection direction) const {
  const double steps = static_cast<double>(steps_);
  double current_step = std::floor((steps * t) + GetStepsStartOffset());
  // Adjust step if using a left limit at a discontinuous step boundary.
  if (direction == LimitDirection::LEFT &&
      steps * t - std::floor(steps * t) == 0) {
    current_step -= 1;
  }
  // Jumps may differ from steps based on the number of end-point
  // discontinuities, which may be 0, 1 or 2.
  int jumps = NumberOfJumps();
  if (t >= 0 && current_step < 0)
    current_step = 0;
  if (t <= 1 && current_step > jumps)
    current_step = jumps;
  return current_step / jumps;
}

int StepsTimingFunction::NumberOfJumps() const {
  switch (step_position_) {
    case StepPosition::END:
    case StepPosition::START:
    case StepPosition::JUMP_END:
    case StepPosition::JUMP_START:
      return steps_;

    case StepPosition::JUMP_BOTH:
      return steps_ + 1;

    case StepPosition::JUMP_NONE:
      DCHECK_GT(steps_, 1);
      return steps_ - 1;

    default:
      NOTREACHED();
      return steps_;
  }
}

float StepsTimingFunction::GetStepsStartOffset() const {
  switch (step_position_) {
    case StepPosition::JUMP_BOTH:
    case StepPosition::JUMP_START:
    case StepPosition::START:
      return 1;

    case StepPosition::JUMP_END:
    case StepPosition::JUMP_NONE:
    case StepPosition::END:
      return 0;

    default:
      NOTREACHED();
      return 1;
  }
}

std::unique_ptr<LinearTimingFunction> LinearTimingFunction::Create() {
  return base::WrapUnique(new LinearTimingFunction());
}

LinearTimingFunction::LinearTimingFunction() = default;

LinearTimingFunction::~LinearTimingFunction() = default;

TimingFunction::Type LinearTimingFunction::GetType() const {
  return Type::LINEAR;
}

std::unique_ptr<TimingFunction> LinearTimingFunction::Clone() const {
  return base::WrapUnique(new LinearTimingFunction(*this));
}

double LinearTimingFunction::Velocity(double x) const {
  return 0;
}

double LinearTimingFunction::GetValue(double t) const {
  return t;
}

}  // namespace cc
