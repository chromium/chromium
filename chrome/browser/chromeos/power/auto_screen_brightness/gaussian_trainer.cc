// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/gaussian_trainer.h"

#include "base/metrics/field_trial_params.h"
#include "chromeos/chromeos_features.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

constexpr double kTol = 1e-10;

// Calculates lower bound from |reference_brightness| using the min of
// 1. Division by a scaling factor and
// 2. Subtraction of an offset.
double BrightnessLowerBound(double reference_brightness,
                            double scale,
                            double offset) {
  DCHECK_GT(scale, 0.0);
  DCHECK_GE(offset, 0.0);

  return std::max(0.0, std::min(reference_brightness / scale,
                                reference_brightness - offset));
}

// Calculates upper bound from |reference_brightness| using the max of
// 1. Multiplication by a scaling factor and
// 2. Addition of an offset.
// The upper bound is also capped at 100.0.
double BrightnessUpperBound(double reference_brightness,
                            double scale,
                            double offset) {
  DCHECK_GT(scale, 0.0);
  DCHECK_GE(offset, 0.0);

  return std::min(100.0, std::max(reference_brightness * scale,
                                  reference_brightness + offset));
}

// Returns whether |brightness| is an outlier from a |reference_brightness|.
bool IsBrightnessOutlier(double brightness,
                         double reference_brightness,
                         const GaussianTrainer::Params& params) {
  DCHECK_GE(reference_brightness, 0.0);
  DCHECK_LE(reference_brightness, 100.0);
  return brightness < BrightnessLowerBound(reference_brightness,
                                           params.brightness_bound_scale,
                                           params.brightness_bound_offset) ||
         brightness > BrightnessUpperBound(reference_brightness,
                                           params.brightness_bound_scale,
                                           params.brightness_bound_offset);
}

// User's selected |brightness_new| may not be the value that the user needs for
// various reasons, e.g. they could overshoot. Hence this function calculates
// the bounded brightness change based on a heuristic magnitude. The new
// brightness is bounded within a factor of 1+|brightness_step_size| from
// |brightness_old|.
double BoundedBrightnessAdjustment(double brightness_old,
                                   double brightness_new,
                                   double brightness_step_size) {
  const double lower_bound = brightness_old / (1.0 + brightness_step_size);
  const double upper_bound = brightness_old * (1.0 + brightness_step_size);

  return std::min(std::max(brightness_new, lower_bound), upper_bound) -
         brightness_old;
}

// Calculates recommended brightness change, given old brightness, user's
// selected new brghtness and model's predicted brightness.
double ModelPredictionAdjustment(double brightness_old,
                                 double brightness_new,
                                 double model_brightness,
                                 const GaussianTrainer::Params& params) {
  DCHECK_GE(brightness_old, 0.0);
  DCHECK_LE(brightness_old, 100.0);
  DCHECK_GE(brightness_new, 0.0);
  DCHECK_LE(brightness_new, 100.0);
  DCHECK_GE(model_brightness, 0.0);
  DCHECK_LE(model_brightness, 100.0);

  const double bounded_user_adjustment = BoundedBrightnessAdjustment(
      brightness_old, brightness_new, params.brightness_step_size);

  DCHECK_GE(bounded_user_adjustment, -100.0);
  DCHECK_LE(bounded_user_adjustment, 100.0);

  const double target_brightness = brightness_old + bounded_user_adjustment;
  DCHECK_GE(target_brightness, 0.0);
  DCHECK_LE(target_brightness, 100.0);

  // If model's prediction is consistent with user's selection, then no
  // brightness change will be necessary.
  // TODO(jiameng): add UMA metrics.
  if ((model_brightness >= target_brightness && bounded_user_adjustment >= 0) ||
      (model_brightness <= target_brightness && bounded_user_adjustment <= 0))
    return 0.0;

  // Model prediction is incorrect, calculate the change we need to make by
  // treating |model_brightness| as the old brightness and |target_brightness|
  // as the new brightness.
  // TODO(jiameng): we currently use 2*step_size, revise.
  return BoundedBrightnessAdjustment(model_brightness, target_brightness,
                                     2.0 * params.brightness_step_size);
}

double Gaussian(double x, double sigma) {
  double xs = x / sigma;
  return std::exp(-xs * xs);
}

}  // namespace

// TODO(jiameng): move these params checking into another method and log errors
// in UMA if any value is invalid. Also disable it if param isn't valid.
GaussianTrainer::GaussianTrainer() {
  params_.brightness_bound_scale = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_bound_scale",
      params_.brightness_bound_scale);
  DCHECK_GT(params_.brightness_bound_scale, 0.0);

  params_.brightness_bound_offset = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_bound_offset",
      params_.brightness_bound_offset);
  DCHECK_GE(params_.brightness_bound_offset, 0.0);

  params_.brightness_step_size = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_step_size",
      params_.brightness_step_size);
  DCHECK_GT(params_.brightness_step_size, 0.0);

  params_.sigma = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "sigma", params_.sigma);
  DCHECK_GT(params_.sigma, 0.0);

  params_.low_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "low_log_lux_threshold",
      params_.low_log_lux_threshold);

  params_.min_grad_low_lux = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_grad_low_lux",
      params_.min_grad_low_lux);
  params_.min_grad = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_grad", params_.min_grad);
  params_.max_grad = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "max_grad", params_.max_grad);

  DCHECK_GE(params_.min_grad_low_lux, 0.0);
  DCHECK_LT(params_.min_grad_low_lux, 1.0);
  DCHECK_GE(params_.min_grad, 0.0);
  DCHECK_LT(params_.min_grad, 1.0);
  DCHECK_GE(params_.min_grad, params_.min_grad_low_lux);
  DCHECK_GT(params_.max_grad, params_.min_grad);

  params_.min_brightness = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_brightness",
      params_.min_brightness);
  DCHECK_GE(params_.min_brightness, 0.0);
}

GaussianTrainer::~GaussianTrainer() = default;

// TODO(jiameng): add slope constraint check in |current_curve| and return check
// result to the caller.
void GaussianTrainer::SetInitialCurves(
    const MonotoneCubicSpline& global_curve,
    const MonotoneCubicSpline& current_curve) {
  // This function should be called once only.
  DCHECK(!global_curve_);
  DCHECK(!current_curve_);
  global_curve_.emplace(global_curve);
  current_curve_.emplace(current_curve);

  ambient_log_lux_ = current_curve_->GetControlPointsX();
  brightness_ = current_curve_->GetControlPointsY();
  const size_t num_points = ambient_log_lux_.size();

  // Global curve and personal curve should have the same ambient log lux.
  const std::vector<double> global_log_lux = global_curve_->GetControlPointsX();
  DCHECK_EQ(global_log_lux.size(), num_points);

  for (size_t i = 0; i < num_points; ++i) {
    DCHECK_LE(std::abs(global_log_lux[i] - ambient_log_lux_[i]), kTol);
  }

  // Calculate |min_ratios_| and |max_ratios_| from global curve.
  min_ratios_.resize(num_points - 1);
  max_ratios_.resize(num_points - 1);
  const std::vector<double> global_brightness =
      global_curve_->GetControlPointsY();

  // TODO(jiameng): may revise to allow 0 as a control point.
  DCHECK_GT(global_brightness[0], 0);

  for (size_t i = 0; i < num_points - 1; ++i) {
    const double min_grad = global_log_lux[i] < params_.low_log_lux_threshold
                                ? params_.min_grad_low_lux
                                : params_.min_grad;

    const double ratio = global_brightness[i + 1] / global_brightness[i];
    DCHECK_GE(ratio, 1);
    min_ratios_[i] = std::pow(ratio, min_grad);
    max_ratios_[i] = std::pow(ratio, params_.max_grad);
  }
}

MonotoneCubicSpline GaussianTrainer::Train(
    const std::vector<TrainingDataPoint>& data) {
  DCHECK(global_curve_);
  DCHECK(current_curve_);
  DCHECK(!data.empty());

  for (const auto& data_point : data) {
    AdjustCurveWithSingleDataPoint(data_point);
  }

  if (!need_to_update_curve_)
    return *current_curve_;

  current_curve_.emplace(MonotoneCubicSpline(ambient_log_lux_, brightness_));
  need_to_update_curve_ = false;
  return *current_curve_;
}

void GaussianTrainer::AdjustCurveWithSingleDataPoint(
    const TrainingDataPoint& data) {
  const double brightness_global =
      global_curve_->Interpolate(data.ambient_log_lux);

  // Check if this |data| is an outlier and should be ignored. It's an outlier
  // if its original/old brightness is too far off from the brightness as
  // predicted by the global curve. This assumes the global curve is reasonably
  // accurate.
  // TODO(jiameng): add UMA metrics to record this.
  if (IsBrightnessOutlier(data.brightness_old, brightness_global, params_)) {
    return;
  }

  // Calculate how much adjustment we need to make to the current personal
  // curve at |data.ambient_log_lux|.
  const double model_brightness =
      current_curve_->Interpolate(data.ambient_log_lux);
  const double brightness_adjustment = ModelPredictionAdjustment(
      data.brightness_old, data.brightness_new, model_brightness, params_);

  if (std::abs(brightness_adjustment) <= kTol)
    return;

  need_to_update_curve_ = true;

  // Index of the log-lux in |ambient_log_lux_| that's closest to
  // |data.ambient_log_lux|.
  size_t center_index = 0;
  double min_dist = std::numeric_limits<double>::max();
  for (size_t i = 0; i < ambient_log_lux_.size(); ++i) {
    // Adjust brightness of each control point in the current brightness curve.
    const double dist = std::abs(data.ambient_log_lux - ambient_log_lux_[i]);
    brightness_[i] += brightness_adjustment * Gaussian(dist, params_.sigma);

    if (dist < min_dist) {
      center_index = i;
      min_dist = dist;
    }
  }

  EnforceMonotonicity(center_index);
}

void GaussianTrainer::EnforceMonotonicity(size_t center_index) {
  DCHECK_LT(center_index, ambient_log_lux_.size());
  brightness_[center_index] = std::min(
      100.0, std::max(params_.min_brightness, brightness_[center_index]));

  // Updates control points to the left of |center_index| so that brightness
  // values satisfy min/max ratio requirement.
  for (size_t i = center_index; i > 0; --i) {
    const double min_value = brightness_[i] / max_ratios_[i - 1];
    const double max_value = brightness_[i] / min_ratios_[i - 1];
    brightness_[i - 1] =
        std::max(std::min(brightness_[i - 1], max_value), min_value);
    // TODO(jiameng): add UMA metrics.
    if (brightness_[i - 1] > 100.0)
      brightness_[i - 1] = 100.0;
  }

  // Updates control points to the right of |center_index| so that brightness
  // values satisfy min/max ratio requirement.
  for (size_t i = center_index; i < ambient_log_lux_.size() - 1; ++i) {
    const double min_value = brightness_[i] * min_ratios_[i];
    const double max_value = brightness_[i] * max_ratios_[i];
    brightness_[i + 1] =
        std::max(std::min(brightness_[i + 1], max_value), min_value);
    // TODO(jiameng): add UMA metrics.
    if (brightness_[i + 1] > 100.0)
      brightness_[i + 1] = 100.0;
  }

#ifndef NDEBUG
  // Check that final |brightness_| array is monotonic across whole range and
  // each value is in [0, 100].
  for (size_t i = 0; i < ambient_log_lux_.size() - 1; ++i) {
    DCHECK_GE(brightness_[i], 0);
    DCHECK_LE(brightness_[i], 100);
    DCHECK_LE(brightness_[i], brightness_[i + 1]);
  }

  DCHECK_GE(brightness_.back(), 0);
  DCHECK_LE(brightness_.back(), 100);
#endif
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
