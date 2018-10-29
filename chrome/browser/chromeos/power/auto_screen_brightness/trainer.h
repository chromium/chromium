// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_

#include "base/time/time.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/monotone_cubic_spline.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

struct TrainingDataPoint {
  double brightness_old;
  double brightness_new;
  double ambient_log_lux;
  base::TimeTicks sample_time;
};

// Interface to train an on-device adaptive brightness curve.
class Trainer {
 public:
  virtual ~Trainer() = default;

  virtual void SetInitialCurves(const MonotoneCubicSpline& global_curve,
                                const MonotoneCubicSpline& current_curve) = 0;

  // Updates current curve stored in trainer with |data|. This function should
  // only be called after |SetInitialCurves|.
  virtual MonotoneCubicSpline Train(
      const std::vector<TrainingDataPoint>& data) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
