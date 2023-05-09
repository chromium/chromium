// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_JITTER_CALCULATOR_H_
#define ASH_AMBIENT_UI_JITTER_CALCULATOR_H_

#include "ash/ash_export.h"
#include "ash/public/cpp/ambient/ambient_ui_model.h"
#include "base/functional/callback.h"
#include "ui/gfx/geometry/vector2d.h"

namespace ash {

// Calculates jitter to apply to a UI with the ultimate goal of preventing
// screen burn. The methodology is:
// * Each time jitter is calculated, move by a fixed "step size" (in pixels) in
//   both x and y directions. Possible incremental offsets are -step_size, 0,
//   and step_size. This is decided randomly.
// * There are limits to the total amount that the jitter can displace the UI
//   from its original unshifted position.
// The caller is responsible for calling Calculate() at the desired frequency.
// The recommendation is approximately every 1-2 minutes, but this is specific
// to each use case.
class ASH_EXPORT JitterCalculator {
 public:
  // Must return either 0 or 1.
  using RandomBinaryGenerator = base::RepeatingCallback<int()>;

  explicit JitterCalculator(AmbientJitterConfig config);
  // Constructor exposed for testing purposes to allow injecting a custom
  // random number generator.
  JitterCalculator(AmbientJitterConfig config,
                   RandomBinaryGenerator random_binary_generator);
  JitterCalculator(const JitterCalculator& other) = delete;
  JitterCalculator& operator=(const JitterCalculator& other) = delete;
  ~JitterCalculator();

  // Returns the new total translation to apply from the UI's original unshifted
  // position (0, 0).
  gfx::Vector2d Calculate();

  const AmbientJitterConfig& config() { return config_; }

 private:
  void AssertCurrentTranslationWithinBounds() const;

  const AmbientJitterConfig config_;
  const RandomBinaryGenerator random_binary_generator_;
  // Current total translation from the original unshifted position.
  gfx::Vector2d current_translation_;
  // The direction to translate for the x/y coordinates.  `1` means positive
  // translate, `-1` negative. Initial values are arbitrary as they are
  // continuously updated as the jitter is calculated.
  int translate_x_direction = 1;
  int translate_y_direction = -1;
};

}  // namespace ash

#endif  // ASH_AMBIENT_UI_JITTER_CALCULATOR_H_
