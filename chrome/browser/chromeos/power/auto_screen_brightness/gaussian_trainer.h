// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#ifndef CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_GAUSSIAN_TRAINER_H_
#define CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_GAUSSIAN_TRAINER_H_

#include <memory>

#include "chrome/browser/chromeos/power/auto_screen_brightness/trainer.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

struct TrainingResult {
  TrainingResult();
  TrainingResult(const base::Optional<MonotoneCubicSpline>& new_curve,
                 double error);
  TrainingResult(const TrainingResult& result);
  ~TrainingResult();
  // |new_curve| will be nullopt if trainer's curve stays unchanged after
  // training.
  base::Optional<MonotoneCubicSpline> new_curve;
  // Evaluation error of the latest curve (possibly updated) using the training
  // data points.
  double error;
};

// GaussianTrainer updates an existing brightness curve (a mapping from
// ambient light to screen brightness) using training data points that represent
// how user changes brightness following an ambient value change. The update
// procedure is Gaussian hence the name. It uses a global curve to check for
// outliers that may exist in training data. It also ensures new curves are
// monotone and also satisfy requirements on the slope.
class GaussianTrainer : public Trainer {
 public:
  // TODO(jiameng): revise default values.
  struct Params {
    Params();
    // |brightness_bound_scale| and |brightness_bound_offset| are used to define
    // training example outliers.
    double brightness_bound_scale = 1.5;
    double brightness_bound_offset = 40;

    // |brightness_step_size| defines reasonable brightness change scale: a
    // reasonable change would be between
    // brightness_old/(1+|brightness_step_size|) and
    // brightness_old*(1+|brightness_step_size|)
    double brightness_step_size = 1.5;

    // Similar to |brightness_step_size| except it defines reasonable brightness
    // change scale between target brightness and model predicted brightness.
    double model_brightness_step_size = 2.0;

    // One training data point could modify all the points on the curve, but its
    // effect is greatest on the point nearest to it (as measured by difference
    // in ambient value). The effect on the other points decay with a Gaussian
    // distribution with standard deviation |sigma|.
    double sigma = 1;

    // If log lux is below |low_log_lux_threshold| then we'll use
    // |min_grad_low_lux| as gradient constraint.
    double low_log_lux_threshold = 0.1;
    double min_grad_low_lux = 0;

    // If log lux is above |high_log_lux_threshold| then we'll use
    // |min_grad_high_lux| as gradient constraint.
    double high_log_lux_threshold = 7.5;
    double min_grad_high_lux = 0;

    // Min and max grad as a power of brightness ratios.
    double min_grad = 0.25;
    double max_grad = 1;

    double min_brightness = 0;
  };

  GaussianTrainer();
  ~GaussianTrainer() override;

  // Trainer overrides:
  bool HasValidConfiguration() const override;
  bool SetInitialCurves(const MonotoneCubicSpline& global_curve,
                        const MonotoneCubicSpline& current_curve) override;
  MonotoneCubicSpline GetGlobalCurve() const override;
  MonotoneCubicSpline GetCurrentCurve() const override;
  TrainingResult Train(const std::vector<TrainingDataPoint>& data) override;

 private:
  // Returns whether initial personal curve (passed in by |SetInitialCurves|) is
  // valid, i.e. satisfying min/max ratio constraints.
  bool IsInitialPersonalCurveValid() const;

  // Updates |brightness_| using |data|. It also sets |need_to_update_curve_|
  // to true if |brightness_| is actually changed.
  void AdjustCurveWithSingleDataPoint(const TrainingDataPoint& data);

  // Called each time |AdjustCurveWithSingleDataPoint| changes |brightness_|.
  // It ensures the curve is still monotone and also satisfies min/max grad
  // constraints. It does this by changing points to the left and to the right
  // of |center_index|.
  void EnforceMonotonicity(size_t center_index);

  // Calculates (possibly) updated curve's MAE error w.r.t. |data|. The error
  // will be in the range of [0, 100].
  double CalculateCurveError(const std::vector<TrainingDataPoint>& data) const;

  // Default params_ are valid.
  bool valid_params_ = true;

  Params params_;
  // |global_curve| does not change after |SetInitialCurves| is called.
  base::Optional<MonotoneCubicSpline> global_curve_;
  // |current_curve_| initially is set by |SetInitialCurves| and then gets
  // updated during training.
  base::Optional<MonotoneCubicSpline> current_curve_;

  // Whether the |brightness_| has been updated since last time |Train| updated
  // the curve.
  bool need_to_update_curve_ = false;

  // (|ambient_log_lux_|, |brightness_|) are the control points of
  // |current_curve_|. |ambient_log_lux_| doesn't change, but |brightness_| may
  // be updated during training.
  std::vector<double> ambient_log_lux_;
  std::vector<double> brightness_;

  // Minimum and max brightness ratios of two adjacent control points. They are
  // calculated from the global curve's brightness values.
  std::vector<double> min_ratios_;
  std::vector<double> max_ratios_;

  DISALLOW_COPY_AND_ASSIGN(GaussianTrainer);
};

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos

#endif  // CHROME_BROWSER_CHROMEOS_POWER_AUTO_SCREEN_BRIGHTNESS_GAUSSIAN_TRAINER_H_
