// Copyright 2022 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef ASH_AMBIENT_UI_JITTER_CALCULATOR_H_
#define ASH_AMBIENT_UI_JITTER_CALCULATOR_H_

#include "ash/ash_export.h"
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
  struct Config {
    static constexpr int kDefaultStepSize = 2;
    static constexpr int kDefaultMaxAbsTranslation = 10;

    int step_size = kDefaultStepSize;
    // Largest the UI can be globally displaced from its original position in
    // both x and y directions. Bounds are inclusive. Requirements:
    // * Range (max_translation - min_translation) must be >= the |step_size|.
    // * Range must include 0 (the original unshifted position),
    int x_min_translation = -kDefaultMaxAbsTranslation;
    int x_max_translation = kDefaultMaxAbsTranslation;
    int y_min_translation = -kDefaultMaxAbsTranslation;
    int y_max_translation = kDefaultMaxAbsTranslation;
  };

  // Must return either 0 or 1.
  using RandomBinaryGenerator = base::RepeatingCallback<int()>;

  explicit JitterCalculator(Config config);
  // Constructor exposed for testing purposes to allow injecting a custom
  // random number generator.
  JitterCalculator(Config config,
                   RandomBinaryGenerator random_binary_generator);
  JitterCalculator(const JitterCalculator& other) = delete;
  JitterCalculator& operator=(const JitterCalculator& other) = delete;
  ~JitterCalculator();

  // Returns the new total translation to apply from the UI's original unshifted
  // position (0, 0).
  gfx::Vector2d Calculate();
  void SetConfigForTesting(Config config);

 private:
  void AssetCurrentTranslationWithinBounds() const;

  Config config_;
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
