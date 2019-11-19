// Copyright 2018 The Chromium Authors. All rights reserved.
// Use of this source code is governed by a BSD-style license that can be
// found in the LICENSE file.

#include "chrome/browser/chromeos/power/auto_screen_brightness/gaussian_trainer.h"

#include <algorithm>
#include <cmath>
#include <limits>

#include "base/logging.h"
#include "base/metrics/field_trial_params.h"
#include "base/metrics/histogram_functions.h"
#include "base/metrics/histogram_macros.h"
#include "base/numerics/ranges.h"
#include "base/strings/stringprintf.h"
#include "chrome/browser/chromeos/power/auto_screen_brightness/utils.h"
#include "chromeos/constants/chromeos_features.h"

namespace chromeos {
namespace power {
namespace auto_screen_brightness {

namespace {

// These values are persisted to logs. Entries should not be renumbered and
// numeric values should never be reused.
// Logs whether a new brightness exceeded the reasonable distance from the old
// brightness. A reasonable distance is defined by the params
// |brightness_step_size| and |model_brightness_step_size|.
enum class BoundedBrightnessChange {
  // User's chosen new brightness is within their [lower_bound, upper_bound].
  kUserWithinBounds = 0,
  // Target brightness has a reasonable distance model's predicted brightness.
  kModelWithinBounds = 1,
  // User's chosen new brightness is below their lower bound.
  kUserLower = 2,
  // User's chosen new brightness is above their upper bound.
  kUserUpper = 3,
  // Target brightness is below model's predicted brightness and exceeded the
  // bound.
  kModelLower = 4,
  // Target brightness is above model's predicted brightness and exceeded the
  // bound.
  kModelUpper = 5,
  kMaxValue = kModelUpper
};

// Returns a |BoundedBrightnessChange| to be logged to UMA.
// |is_lower_bound_exceeded| is nullopt if the new brightness is within the
// bounds.
BoundedBrightnessChange GetBoundedBrightnessChange(
    base::Optional<bool> is_lower_bound_exceeded,
    bool is_user) {
  if (!is_lower_bound_exceeded.has_value()) {
    if (is_user) {
      return BoundedBrightnessChange::kUserWithinBounds;
    }
    return BoundedBrightnessChange::kModelWithinBounds;
  }

  if (*is_lower_bound_exceeded) {
    if (is_user) {
      return BoundedBrightnessChange::kUserLower;
    }
    return BoundedBrightnessChange::kModelLower;
  }

  if (is_user) {
    return BoundedBrightnessChange::kUserUpper;
  }
  return BoundedBrightnessChange::kModelUpper;
}

constexpr double kTol = 1e-10;

// Calculates lower bound from |reference_brightness| using the min of
// 1. Division by a scaling factor and
// 2. Subtraction of an offset.
double BrightnessLowerBound(double reference_brightness,
                            double scale,
                            double offset) {
  DCHECK_GT(scale, 0.0);
  DCHECK_GE(offset, 0.0);

  return base::ClampToRange(reference_brightness / scale, 0.0,
                            reference_brightness - offset);
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

  return base::ClampToRange(reference_brightness * scale,
                            reference_brightness + offset, 100.0);
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
                                   double brightness_step_size,
                                   bool is_user) {
  const double lower_bound = brightness_old / (1.0 + brightness_step_size);
  const double upper_bound = brightness_old * (1.0 + brightness_step_size);

  const bool exceeded_upper = brightness_new > upper_bound;
  const bool exceeded_lower = brightness_new < lower_bound;

  const BoundedBrightnessChange change = GetBoundedBrightnessChange(
      exceeded_lower || exceeded_upper ? base::Optional<bool>(exceeded_lower)
                                       : base::nullopt,
      is_user);
  UMA_HISTOGRAM_ENUMERATION(
      "AutoScreenBrightness.ModelTraining.BrightnessChange", change);
  VLOG(1) << "ABTrainer bounded brightness change type: "
          << static_cast<int>(change);

  return base::ClampToRange(brightness_new, lower_bound, upper_bound) -
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
      brightness_old, brightness_new, params.brightness_step_size,
      true /* is_user */);

  DCHECK_GE(bounded_user_adjustment, -100.0);
  DCHECK_LE(bounded_user_adjustment, 100.0);

  const double target_brightness = brightness_old + bounded_user_adjustment;
  DCHECK_GE(target_brightness, 0.0);
  DCHECK_LE(target_brightness, 100.0);

  // Check if model prediction and user adjustment are consistent.
  const bool is_consistent =
      (model_brightness >= target_brightness && bounded_user_adjustment >= 0) ||
      (model_brightness <= target_brightness && bounded_user_adjustment <= 0);
  UMA_HISTOGRAM_BOOLEAN(
      "AutoScreenBrightness.ModelTraining.ModelUserConsistent", is_consistent);

  // If model's prediction is consistent with user's selection, then no
  // brightness change will be necessary.
  if (is_consistent) {
    VLOG(1) << "ABTrainer model user brightness consistent (model,user): "
            << FormatToPrint(model_brightness - target_brightness) << ","
            << FormatToPrint(bounded_user_adjustment);
    return 0.0;
  }

  // Model prediction is incorrect, calculate the change we need to make by
  // treating |model_brightness| as the old brightness and |target_brightness|
  // as the new brightness.
  return BoundedBrightnessAdjustment(model_brightness, target_brightness,
                                     params.model_brightness_step_size,
                                     false /* is_user */);
}

double Gaussian(double x, double sigma) {
  double xs = x / sigma;
  return std::exp(-xs * xs);
}

void LogModelCurveError(double error, bool model_updated) {
  DCHECK_GE(error, 0.0);
  const std::string histogram_name =
      std::string("AutoScreenBrightness.ModelTraining.Inaccuracy.") +
      (model_updated ? "Update" : "NoUpdate");
  base::UmaHistogramPercentage(histogram_name, std::round(error));
  VLOG(1) << "ABTrainer model error " << (model_updated ? "with " : "without ")
          << "model updated: " << FormatToPrint(error);
}

}  // namespace

TrainingResult::TrainingResult() = default;
TrainingResult::TrainingResult(
    const base::Optional<MonotoneCubicSpline>& new_curve,
    double error)
    : new_curve(new_curve), error(error) {}

TrainingResult::TrainingResult(const TrainingResult& result) = default;
TrainingResult::~TrainingResult() = default;

GaussianTrainer::Params::Params() = default;

GaussianTrainer::GaussianTrainer() {
  params_.brightness_bound_scale = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_bound_scale",
      params_.brightness_bound_scale);
  if (params_.brightness_bound_scale <= 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.brightness_bound_offset = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_bound_offset",
      params_.brightness_bound_offset);
  if (params_.brightness_bound_offset < 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.brightness_step_size = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "brightness_step_size",
      params_.brightness_step_size);
  if (params_.brightness_step_size <= 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.model_brightness_step_size = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "model_brightness_step_size",
      params_.model_brightness_step_size);
  if (params_.model_brightness_step_size <= 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.sigma = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "sigma", params_.sigma);
  if (params_.sigma <= 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.low_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "low_log_lux_threshold",
      params_.low_log_lux_threshold);
  params_.high_log_lux_threshold = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "high_log_lux_threshold",
      params_.high_log_lux_threshold);
  if (params_.low_log_lux_threshold >= params_.high_log_lux_threshold) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.min_grad_low_lux = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_grad_low_lux",
      params_.min_grad_low_lux);
  params_.min_grad_high_lux = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_grad_high_lux",
      params_.min_grad_high_lux);

  params_.min_grad = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_grad", params_.min_grad);
  params_.max_grad = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "max_grad", params_.max_grad);

  if (params_.min_grad_low_lux < 0.0 || params_.min_grad_low_lux >= 1.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  if (params_.min_grad_high_lux < 0.0 || params_.min_grad_high_lux >= 1.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  if (params_.min_grad < 0.0 || params_.min_grad >= 1.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  if (params_.min_grad < params_.min_grad_low_lux) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  if (params_.min_grad < params_.min_grad_high_lux) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  if (params_.max_grad < 1.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }

  params_.min_brightness = GetFieldTrialParamByFeatureAsDouble(
      features::kAutoScreenBrightness, "min_brightness",
      params_.min_brightness);
  if (params_.min_brightness < 0.0) {
    valid_params_ = false;
    LogParameterError(ParameterError::kModelError);
    return;
  }
}

GaussianTrainer::~GaussianTrainer() = default;

bool GaussianTrainer::HasValidConfiguration() const {
  return valid_params_;
}

bool GaussianTrainer::SetInitialCurves(
    const MonotoneCubicSpline& global_curve,
    const MonotoneCubicSpline& current_curve) {
  DCHECK(valid_params_);

  // This function could be called again if the caller wants to reset the
  // curves.
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
    double min_grad = params_.min_grad;
    if (global_log_lux[i] < params_.low_log_lux_threshold) {
      min_grad = params_.min_grad_low_lux;
    } else if (global_log_lux[i] > params_.high_log_lux_threshold) {
      min_grad = params_.min_grad_high_lux;
    }

    const double ratio = global_brightness[i + 1] / global_brightness[i];
    DCHECK_GE(ratio, 1);
    min_ratios_[i] = std::pow(ratio, min_grad);
    max_ratios_[i] = std::pow(ratio, params_.max_grad);
  }

  if (!IsInitialPersonalCurveValid()) {
    // Use global curve instead if personal curve isn't valid.
    current_curve_.emplace(global_curve);
    brightness_ = current_curve_->GetControlPointsY();
    return false;
  }

  return true;
}

MonotoneCubicSpline GaussianTrainer::GetGlobalCurve() const {
  DCHECK(valid_params_);
  DCHECK(global_curve_);
  return *global_curve_;
}

MonotoneCubicSpline GaussianTrainer::GetCurrentCurve() const {
  DCHECK(valid_params_);
  DCHECK(current_curve_);
  return *current_curve_;
}

TrainingResult GaussianTrainer::Train(
    const std::vector<TrainingDataPoint>& data) {
  DCHECK(global_curve_);
  DCHECK(current_curve_);
  DCHECK(!data.empty());

  for (const auto& data_point : data) {
    AdjustCurveWithSingleDataPoint(data_point);
  }

  if (!need_to_update_curve_) {
    const double error = CalculateCurveError(data);
    LogModelCurveError(error, false /* model_updated */);
    VLOG(1) << "ABTrainer training finished without new curve: \n"
            << current_curve_->ToString();
    return TrainingResult(base::nullopt, error);
  }

  current_curve_ = MonotoneCubicSpline::CreateMonotoneCubicSpline(
      ambient_log_lux_, brightness_);
  DCHECK(current_curve_);
  VLOG(1) << "ABTrainer training finished with new curve: \n"
          << current_curve_->ToString();
  need_to_update_curve_ = false;

  const double error = CalculateCurveError(data);
  LogModelCurveError(error, true /* model_updated */);
  return TrainingResult(current_curve_, error);
}

bool GaussianTrainer::IsInitialPersonalCurveValid() const {
  // |global_curve_| is valid by construction.
  if (*global_curve_ == *current_curve_)
    return true;

  for (size_t i = 0; i < brightness_.size() - 1; ++i) {
    const double ratio = brightness_[i + 1] / brightness_[i];
    if (ratio < min_ratios_[i] || ratio > max_ratios_[i])
      return false;
  }

  return true;
}

void GaussianTrainer::AdjustCurveWithSingleDataPoint(
    const TrainingDataPoint& data) {
  const double brightness_global =
      global_curve_->Interpolate(data.ambient_log_lux);

  VLOG(1) << "ABTrainer training data point (global,old,new,log_lux): "
          << FormatToPrint(brightness_global) << ", "
          << FormatToPrint(data.brightness_old) << ", "
          << FormatToPrint(data.brightness_new) << ", " << data.ambient_log_lux;

  // Check if this |data| is an outlier and should be ignored. It's an outlier
  // if its original/old brightness is too far off from the brightness as
  // predicted by the global curve. This assumes the global curve is reasonably
  // accurate.
  const bool is_brightness_outlier =
      IsBrightnessOutlier(data.brightness_old, brightness_global, params_);
  UMA_HISTOGRAM_BOOLEAN("AutoScreenBrightness.ModelTraining.BrightnessOutlier",
                        is_brightness_outlier);

  if (is_brightness_outlier) {
    VLOG(1) << "ABTrainer outlier (user,global): "
            << FormatToPrint(data.brightness_old) << ","
            << FormatToPrint(brightness_global);
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
  brightness_[center_index] = base::ClampToRange(brightness_[center_index],
                                                 params_.min_brightness, 100.0);

  // Updates control points to the left of |center_index| so that brightness
  // values satisfy min/max ratio requirement.
  for (size_t i = center_index; i > 0; --i) {
    const double min_value = brightness_[i] / max_ratios_[i - 1];
    const double max_value = brightness_[i] / min_ratios_[i - 1];
    brightness_[i - 1] =
        base::ClampToRange(brightness_[i - 1], min_value, max_value);
    if (brightness_[i - 1] > 100.0) {
      VLOG(1) << "ABTrainer exceeded max at " << (i - 1)
              << " with value: " << FormatToPrint(brightness_[i - 1]);
      brightness_[i - 1] = 100.0;
    }
  }

  // Updates control points to the right of |center_index| so that brightness
  // values satisfy min/max ratio requirement.
  for (size_t i = center_index; i < ambient_log_lux_.size() - 1; ++i) {
    const double min_value = brightness_[i] * min_ratios_[i];
    const double max_value = brightness_[i] * max_ratios_[i];
    brightness_[i + 1] =
        base::ClampToRange(brightness_[i + 1], min_value, max_value);
    if (brightness_[i + 1] > 100.0) {
      VLOG(1) << "ABTrainer exceeded max at " << (i + 1)
              << " with value: " << FormatToPrint(brightness_[i + 1]);
      brightness_[i + 1] = 100.0;
    }
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

double GaussianTrainer::CalculateCurveError(
    const std::vector<TrainingDataPoint>& data) const {
  DCHECK(current_curve_);
  double error = 0.0;
  for (const auto& data_point : data) {
    error += std::abs(data_point.brightness_new -
                      current_curve_->Interpolate(data_point.ambient_log_lux));
  }
  return error / data.size();
}

}  // namespace auto_screen_brightness
}  // namespace power
}  // namespace chromeos
