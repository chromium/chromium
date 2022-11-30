// Copyright 2018 The Chromium Authors
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
#define CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_

#include "base/time/time.h"
#include "chrome/browser/ash/power/auto_screen_brightness/monotone_cubic_spline.h"

namespace ash {
namespace power {
namespace auto_screen_brightness {

struct TrainingDataPoint {
  double brightness_old;
  double brightness_new;
  double ambient_log_lux;
  base::TimeTicks sample_time;
};

struct TrainingResult;

// Interface to train an on-device adaptive brightness curve.
// User should call |HasValidConfiguration| first. If it returns true, then user
// should call |SetInitialCurves| before calling other methods.
class Trainer {
 public:
  virtual ~Trainer() = default;

  // Returns whether trainer has been configured properly, i.e. if all params
  // are set up. It is an error to call other methods unless
  // |HasValidConfiguration| returns true.
  virtual bool HasValidConfiguration() const = 0;

  // Initializes this trainer with the specified default global curve and
  // initial current curve (the personal curve). This should only be called if
  // trainer |HasValidConfiguration| returns true.
  // Returns true if |current_curve| is valid, i.e. satisfying constraints (e.g.
  // slope). If |current_curve| is invalid, |global_curve| will be used in its
  // place. The caller has an option to reset these curves.
  virtual bool SetInitialCurves(const MonotoneCubicSpline& global_curve,
                                const MonotoneCubicSpline& current_curve) = 0;

  // Returns the global curve. This should only be called if trainer
  // |HasValidConfiguration| returns true and after |SetInitialCurves| is
  // called.
  virtual MonotoneCubicSpline GetGlobalCurve() const = 0;

  // Returns the curve currently used as personal curve. It could be the same as
  // the global curve. This should only be called if trainer
  // |HasValidConfiguration| returns true and after |SetInitialCurves| is
  // called.
  virtual MonotoneCubicSpline GetCurrentCurve() const = 0;

  // Possibly updates current curve stored in trainer with |data|, and returns
  // training result.
  // This function should only be called after |SetInitialCurves|.
  virtual TrainingResult Train(const std::vector<TrainingDataPoint>& data) = 0;
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace ash

#endif  // CHROME_BROWSER_ASH_POWER_AUTO_SCREEN_BRIGHTNESS_TRAINER_H_
