// Copyright 2026 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CC_INPUT_BROWSER_CONTROLS_ANIMATION_H_
#define CC_INPUT_BROWSER_CONTROLS_ANIMATION_H_

#include <optional>

#include "base/time/time.h"
#include "cc/cc_export.h"
#include "ui/gfx/animation/tween.h"

namespace cc {

// Holds and manages the state of a browser controls shown-ratio animation.
class CC_EXPORT BrowserControlsAnimation {
 public:
  enum class Direction {
    kNoAnimation,
    kShowingControls,
    kHidingControls,
  };

  BrowserControlsAnimation();

  // Whether the animation is initialized with a direction and start and stop
  // values.
  bool IsInitialized() const { return initialized_; }
  Direction direction() const { return direction_; }
  void Initialize(Direction direction,
                  float start_value,
                  float stop_value,
                  base::TimeDelta duration,
                  bool jump_to_end_on_reset,
                  gfx::Tween::Type tween_type);
  // Returns the animated value for the given monotonic time tick if the
  // animation is initialized. Otherwise, returns `std::nullopt`.
  std::optional<float> Tick(base::TimeTicks monotonic_time);
  // Sets the range of values the animation can produce. The stop value may lie
  // outside the bounds, in which case the animation keeps its speed and ends
  // early at the bound. The caller uses this to animate towards an out-of-range
  // stop value and let the bound decide where the controls come to rest.
  void SetBounds(float min, float max);
  // Reset the properties. If `jump_to_end_on_reset_` is false, this function
  // will return `std::nullopt`. Otherwise, it will return `FinalValue()`.
  std::optional<float> Reset();

  // Returns the value the animation will end on. This will be the stop value
  // passed to Initialize() clamped by the currently configured bounds.
  float FinalValue() const;

  // Return the bounds.
  float min_value() const { return min_value_; }
  float max_value() const { return max_value_; }

  bool jump_to_end_on_reset() const { return jump_to_end_on_reset_; }

 private:
  bool IsComplete(float value) const;

  // Whether the animation is running.
  bool started_ = false;
  // Whether the animation is initialized by setting start and stop time and
  // values.
  bool initialized_ = false;
  Direction direction_ = Direction::kNoAnimation;
  // Monotonic start and stop times.
  base::TimeTicks start_time_;
  base::TimeTicks stop_time_;
  // Animation duration.
  base::TimeDelta duration_;
  // The animation interpolates from `start_value_` to `stop_value_`.
  float start_value_ = 0.0f;
  float stop_value_ = 0.0f;
  // Range of values the animation can produce. Reaching a bound completes the
  // animation even if `stop_value_` lies beyond it.
  float min_value_ = 0.0f;
  float max_value_ = 1.0f;
  // Whether to fast-forward to end when reset. It is still the caller's
  // responsibility to actually set the shown ratios using the value returned
  // by ::Reset().
  bool jump_to_end_on_reset_ = false;
  gfx::Tween::Type tween_type_ = gfx::Tween::LINEAR;
};

}  // namespace cc

#endif  // CC_INPUT_BROWSER_CONTROLS_ANIMATION_H_
